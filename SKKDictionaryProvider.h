#pragma once
#include "globals.h"

#include <string>
#include <vector>
#include <map>
#include <fstream>


#define _Candidate .first
#define _Annotation .second

#define SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_STR L";"
#define SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_WCHAR L';'
#define SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_ASTR ";"

#define SKK_CHOOSE_CANDIDATES_BIGSTR  (L"ASDFJKL")
#define SKK_CHOOSE_CANDIDATES_SMLSTR  (L"asdfjkl")
#define SKK_CHOOSE_CANDIDATES_BIGSTR_LEN  (7)


#define MAX_HISTORY_STR_LEN 32
#define MAX_HISTORY_ANNO_LEN 32
#define MAX_SKK_DICTIONARY_HISTORY 1024

struct SKKCandidate {
	std::wstring candidate; // 一つの候補文字列
	std::wstring annotation;  // 注釈
};
struct SharedSKKHistoryItem {
	WCHAR reading[MAX_HISTORY_STR_LEN]; // ひらがな読み
	WCHAR result[MAX_HISTORY_STR_LEN];  // 変換結果
	WCHAR anno[MAX_HISTORY_ANNO_LEN]; // 注釈
};
struct SharedSKKHistory {
	int count; // 履歴の件数
	int user_dict_version; // ユーザー辞書のバージョン(起動直後は0)
	SharedSKKHistoryItem items[MAX_SKK_DICTIONARY_HISTORY]; // 履歴の内容
};

using SKKCandidates = std::vector<SKKCandidate>;//候補文字列群
using SKKDictionary = std::map<std::wstring, SKKCandidates>;//読み，候補文字列群

// SKKDictionary[L"こうしょう"]={{"交渉",";話し合って条件を決めあう"},{"校章",";学校や大学の紋章"},{"工廠",";工場みたいなところ"}, ... }


#define EUC_JP_CODEPAGE 20932

class SKKDictionaryProvider
{
public:
	SKKDictionaryProvider();
	~SKKDictionaryProvider();

	BOOL LoadDictionary(const std::wstring dictpath, const std::wstring user_dictpath);
	void GetCandidates(const std::wstring& hiragana_key, SKKCandidates& candidates);
	bool GetPrediction(const std::wstring& hiragana_key, SKKCandidate& candidate);
	void RegisterUserDictionary(const std::wstring& hiragana_key,const std::wstring okurigana_alphaet, const SKKCandidate& candidate);
	void AddHistory(SKKCandidate& candidate, const std::wstring& reading);
private:
	SKKDictionary m_skkdict;
	SKKDictionary m_user_skkdict;

	HANDLE m_hHistoryMap;
	HANDLE m_hHistoryMutex;
	SharedSKKHistory* m_pHistory;
	int m_local_user_dict_version; // ローカルのユーザー辞書バージョン(起動直後は0)

	BOOL _LoadDictionary(const std::wstring& filepath, SKKDictionary& _dict);
	BOOL SaveUserDictionary();
	BOOL _LoadHistory();
	BOOL _SaveHistory();
	void _InitSharedHistory();
	void _ReleaseSharedHistory();

	void _CheckAndUpdateUserDictVersion();
};

