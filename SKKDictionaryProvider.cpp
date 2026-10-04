#include "pch.h"
#include "SKKDictionaryProvider.h"

#include <ranges>
SKKDictionaryProvider::SKKDictionaryProvider()
{
	m_skkdict = SKKDictionary();
	m_user_skkdict = SKKDictionary();
}

SKKDictionaryProvider::~SKKDictionaryProvider()
{}

BOOL SKKDictionaryProvider::LoadDictionary(const std::wstring dictpath, const std::wstring user_dictpath)
{
	if (_LoadDictionary(dictpath, m_skkdict) == FALSE) {
		return FALSE;
	}
	if (_LoadDictionary(user_dictpath, m_user_skkdict) == FALSE) {
		return FALSE;
	}

	return TRUE;
}

void SKKDictionaryProvider::GetCandidates(const std::wstring& hiragana_key, SKKCandidates& candidates) const
{
	candidates.clear();
	auto it = m_skkdict.find(hiragana_key);
	if (it != m_skkdict.end()) {
		candidates = it->second;
	}

	it = m_user_skkdict.find(hiragana_key);
	if (it != m_user_skkdict.end()) {
		// ユーザー辞書の候補を追加
		candidates.insert(candidates.end(), it->second.begin(), it->second.end());
	}
}

void SKKDictionaryProvider::RegisterUserDictionary(const std::wstring& hiragana_key, const std::wstring okurigana_alphaet, const SKKCandidate& candidate)
{
	m_user_skkdict[hiragana_key + okurigana_alphaet].push_back(candidate);
	SaveUserDictionary();
}

BOOL SKKDictionaryProvider::_LoadDictionary(const std::wstring& filepath, SKKDictionary& _dict)
{
	std::ifstream file(filepath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return FALSE;
	}

	std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	if (fileSize <= 0) {
		return TRUE;
	}

	// ファイル全体を一括読み込み
	std::vector<char> buffer(static_cast<size_t>(fileSize));
	if (!file.read(buffer.data(), fileSize)) {
		return FALSE;
	}

	// ファイル全体を一括で文字コード変換 (MultiByteToWideCharの呼び出しオーバーヘッドを削減)
	int wideSize = MultiByteToWideChar(EUC_JP_CODEPAGE, 0, buffer.data(), static_cast<int>(fileSize), nullptr, 0);
	if (wideSize <= 0) {
		return FALSE;
	}

	std::vector<wchar_t> wbuffer(static_cast<size_t>(wideSize));
	MultiByteToWideChar(EUC_JP_CODEPAGE, 0, buffer.data(), static_cast<int>(fileSize), wbuffer.data(), wideSize);

	// ポインタを利用したゼロアロケーション解析
	const wchar_t* p = wbuffer.data();
	const wchar_t* end = p + wideSize;

	while (p < end) {
		const wchar_t* nl = static_cast<const wchar_t*>(wmemchr(p, L'\n', end - p));
		if (!nl) {
			nl = end;
		}

		const wchar_t* lineEnd = nl;
		if (lineEnd > p && *(lineEnd - 1) == L'\r') {
			--lineEnd;
		}

		size_t lineLen = lineEnd - p;

		// 空行およびコメント行のスキップ
		if (lineLen > 0 && !(lineLen >= 2 && p[0] == L';' && p[1] == L';')) {
			const wchar_t* space = static_cast<const wchar_t*>(wmemchr(p, L' ', lineLen));
			if (space) {
				std::wstring reading(p, space - p);
				SKKCandidates candidates;

				const wchar_t* candStart = space + 1;

				// 候補数の予測とメモリ事前確保による再確保コストの削減
				size_t slashCount = 0;
				for (const wchar_t* c = candStart; c < lineEnd; ++c) {
					if (*c == L'/') {
						++slashCount;
					}
				}
				candidates.reserve(slashCount);

				// 候補文字列の抽出
				while (candStart < lineEnd) {
					const wchar_t* slash = static_cast<const wchar_t*>(wmemchr(candStart, L'/', lineEnd - candStart));
					if (!slash) {
						slash = lineEnd;
					}

					size_t candLen = slash - candStart;
					if (candLen > 0) {
						const wchar_t* semi = static_cast<const wchar_t*>(wmemchr(candStart, SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_WCHAR, candLen));
						if (semi) {
							// 注釈がある場合
							candidates.push_back({
								std::wstring(candStart, semi - candStart),
								std::wstring(semi + 1, slash - semi - 1)
								});
						}
						else {
							// 注釈がない場合
							candidates.push_back({
								std::wstring(candStart, candLen),
								std::wstring()
								});
						}
					}
					candStart = slash + 1;
				}

				if (!candidates.empty()) {
					// ムーブセマンティクスを利用して無駄なコピーを回避
					_dict.insert_or_assign(std::move(reading), std::move(candidates));
				}
			}
		}
		p = nl + 1;

	}
	return TRUE;
}

BOOL SKKDictionaryProvider::SaveUserDictionary()
{
	// Mutexによる排他制御。様々な権限のプロセスからアクセスされるためNull DACLを設定
	SECURITY_DESCRIPTOR sd;
	InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = &sd;
	sa.bInheritHandle = FALSE;

	HANDLE hMutex = CreateMutexW(&sa, FALSE, L"Global\\TettySKK_UserDictMutex");
	if (!hMutex) {
		hMutex = CreateMutexW(&sa, FALSE, L"Local\\TettySKK_UserDictMutex");
	}

	if (hMutex) {
		WaitForSingleObject(hMutex, INFINITE);
	}

	// 他のプロセスで追加された単語を取りこぼさないように、現在のファイルを再読み込みしてマージする
	SKKDictionary current_file_dict;
	if (_LoadDictionary(g_Setting.skk_user_dic_file_path, current_file_dict)) {
		for (const auto& [key, candidates] : m_user_skkdict) {
			auto& target_cands = current_file_dict[key];
			for (const auto& cand : candidates) {
				bool found = false;
				for (const auto& target_cand : target_cands) {
					if (target_cand.candidate == cand.candidate) {
						found = true;
						break;
					}
				}
				if (!found) {
					target_cands.push_back(cand);
				}
			}
		}
		m_user_skkdict = std::move(current_file_dict);
	}

	// 一時ファイルに保存して、アトミックにリネームすることで破損を防ぐ
	std::wstring tmp_path = g_Setting.skk_user_dic_file_path + L".tmp";

	// EUC-JPで保存する
	std::ofstream file(tmp_path, std::ios::binary);

	if (!file.is_open()) {
		if (hMutex) {
			ReleaseMutex(hMutex);
			CloseHandle(hMutex);
		}
		return FALSE;
	}

	for (const auto& [wkey, candidates] : m_user_skkdict) {
		//文字コードの変換
		std::string key;
		{
			int len = WideCharToMultiByte(EUC_JP_CODEPAGE, 0, wkey.c_str(), wkey.length(), nullptr, 0, nullptr, 0);
			if (len > 0) {
				key.resize(len);
				WideCharToMultiByte(EUC_JP_CODEPAGE, 0, wkey.c_str(), wkey.length(), &key[0], len, nullptr, 0);
			}
		}

		file << key << " ";

		for (const auto& cand : candidates) {
			const auto& wword = cand.candidate;
			const auto& wannotation = cand.annotation;

			//文字コードの変換
			std::string word;
			{
				int len = WideCharToMultiByte(EUC_JP_CODEPAGE, 0, wword.c_str(), wword.length(), nullptr, 0, nullptr, 0);
				if (len > 0) {
					word.resize(len);
					WideCharToMultiByte(EUC_JP_CODEPAGE, 0, wword.c_str(), wword.length(), &word[0], len, nullptr, 0);
				}
			}
			std::string annotation;
			if (!wannotation.empty()) {
				int len = WideCharToMultiByte(EUC_JP_CODEPAGE, 0, wannotation.c_str(), wannotation.length(), nullptr, 0, nullptr, 0);
				if (len > 0) {
					annotation.resize(len);
					WideCharToMultiByte(EUC_JP_CODEPAGE, 0, wannotation.c_str(), wannotation.length(), &annotation[0], len, nullptr, 0);
				}
			}

			file << "/" << word;
			if (!annotation.empty()) {
				file << SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_ASTR;
				file << annotation;
			}
		}
		file << "/" << std::endl;
	}

	file.close();

	// 一時ファイルから元のファイルへアトミックに置換
	MoveFileExW(tmp_path.c_str(), g_Setting.skk_user_dic_file_path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);

	if (hMutex) {
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
	}

	return TRUE;
}
