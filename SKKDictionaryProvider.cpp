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
	/*
	if (_dict.size() > 0)return TRUE;

	_dict.clear();

	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open()) {
		return FALSE;
	}

	std::string line;
	while (std::getline(file, line)) {
		if (line.empty()) {
			continue;
		}

		//文字コードの変換
		std::wstring linew;
		{
			int len = MultiByteToWideChar(EUC_JP_CODEPAGE, 0, line.c_str(), line.length(), nullptr, 0);
			if (len > 0) {
				linew.resize(len);
				MultiByteToWideChar(EUC_JP_CODEPAGE, 0, line.c_str(), line.length(), &linew[0], len);
			}
		}
		if (linew.starts_with(L";;"))continue;

		//辞書解析
		/*
		* 構文
		*
		* [ひらがな] /候補1/候補2/候補3/.../
		* 候補の中に;がある場合，その後は解説的なもの
		*/
	/*
		//  例:
		// いa /射/居/鋳;金物を鋳る/将;兵を将る/率;≒将る/鑄;「鋳」の旧字(人名用漢字)/

		size_t spacePos = linew.find(L' ');
		if (spacePos == std::wstring::npos) {
			continue;
		}

		std::wstring reading = linew.substr(0, spacePos);// 読み
		std::wstring candidateStr = linew.substr(spacePos + 1);// 候補文字列群

		SKKCandidates candidates;

		for (auto&& subrange : candidateStr | std::views::split(L'/')) {
			std::wstring_view splitline(subrange.begin(), subrange.end());

			// 候補文字列
			if (splitline.empty())continue;

			if (splitline.find(SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_WCHAR) != std::wstring_view::npos) {
				// 注釈がある場合
				size_t semicolonPos = splitline.find(SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_WCHAR);
				std::wstring candidate(splitline.substr(0, semicolonPos));
				std::wstring annotation(splitline.substr(semicolonPos + 1));
				candidates.push_back({ candidate, annotation });
			}
			else {
				// 注釈がない場合
				std::wstring candidate(splitline);
				candidates.push_back({ candidate, L"" });
			}
		}
		if (!candidates.empty()) {
			_dict[reading] = candidates;
		}
	}

	return TRUE;*/
}
