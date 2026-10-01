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

BOOL SKKDictionaryProvider::_LoadDictionary(const std::wstring& filepath, SKKDictionary& _dict)
{
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

	return TRUE;
}
