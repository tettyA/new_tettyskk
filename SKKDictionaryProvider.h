#pragma once
#include "globals.h"

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <utility>

#include <deque>

#define _Candidate .first
#define _Annotation .second

#define SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_STR L";"
#define SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_WCHAR L';'

#define SKK_CHOOSE_CANDIDATES_BIGSTR  (L"ASDFJKL")
#define SKK_CHOOSE_CANDIDATES_SMLSTR  (L"asdfjkl")
#define SKK_CHOOSE_CANDIDATES_BIGSTR_LEN  (7)
struct SKKCandidate {
	std::wstring candidate; // 一つの候補文字列
	std::wstring annotation;  // 注釈
};
using SKKCandidates = std::vector<SKKCandidate>;//候補文字列群
using SKKDictionary = std::map<std::wstring, SKKCandidates>;//読み，候補文字列群

// SKKDictionary[L"こうしょう"]={{"交渉","話し合って条件を決めあう"},{"校章","学校や大学の紋章"},{"工廠","工場みたいなところ"}, ... }

//#define MAX_SKK_DICTIONARY_HISTORY 1000
//using SKKdictionaryHistory = std::deque<std::wstring>; //読みのみ 変換履歴

#define EUC_JP_CODEPAGE 20932

class SKKDictionaryProvider
{
public:
	SKKDictionaryProvider();
	~SKKDictionaryProvider();

	BOOL LoadDictionary(const std::wstring dictpath, const std::wstring user_dictpath);
	void GetCandidates(const std::wstring& hiragana_key, SKKCandidates& candidates) const;
	void RegisterUserDictionary(const std::wstring& hiragana_key,const std::wstring okurigana_alphaet, const SKKCandidate& candidate);
private:
	SKKDictionary m_skkdict;
	SKKDictionary m_user_skkdict;

	BOOL _LoadDictionary(const std::wstring& filepath, SKKDictionary& _dict);
};

