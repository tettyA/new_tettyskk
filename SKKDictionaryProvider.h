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

struct SKKCandidate {
	std::wstring candidate; // ˆê‚Â‚ÌŒó•â•¶š—ñ
	std::wstring annotation;  // ’ß
};
using SKKCandidates = std::vector<SKKCandidate>;//Œó•â•¶š—ñŒQ
using SKKDictionary = std::map<std::wstring, SKKCandidates>;//“Ç‚İCŒó•â•¶š—ñŒQ

// SKKDictionary[L"‚±‚¤‚µ‚å‚¤"]={{"ŒğÂ","˜b‚µ‡‚Á‚ÄğŒ‚ğŒˆ‚ß‚ ‚¤"},{"ZÍ","ŠwZ‚â‘åŠw‚Ì–äÍ"},{"H±","Hê‚İ‚½‚¢‚È‚Æ‚±‚ë"}, ... }

//#define MAX_SKK_DICTIONARY_HISTORY 1000
//using SKKdictionaryHistory = std::deque<std::wstring>; //“Ç‚İ‚Ì‚İ •ÏŠ·—š—ğ

#define EUC_JP_CODEPAGE 20932

class SKKDictionaryProvider
{
public:
	SKKDictionaryProvider();
	~SKKDictionaryProvider();

	BOOL LoadDictionary(const std::wstring dictpath, const std::wstring user_dictpath);
	void GetCandidates(const std::wstring& hiragana_key, SKKCandidates& candidates) const;
private:
	SKKDictionary m_skkdict;
	SKKDictionary m_user_skkdict;

	BOOL _LoadDictionary(const std::wstring& filepath, SKKDictionary& _dict);
};

