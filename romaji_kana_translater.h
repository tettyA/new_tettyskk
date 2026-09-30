#pragma once
#define WIN32_LEAN_AND_MEAN
#include <vector>
#include <string>
#include <unordered_map>
#include <Windows.h>

#define TrR2K_INDEX_HIRAGANA 0
#define TrR2K_INDEX_KATAKANA 1

#define MAKETRANSTABLE(romaji,hiragana,katakana) {L#romaji, {L#hiragana, L#katakana}}
#define MAKETKIGOUTRANSTABLE(keycode,jpnsymbol) { keycode, L#jpnsymbol}

class romaji_kana_translater
{
public:
	romaji_kana_translater();
	~romaji_kana_translater();
	// true: 変換され，m_bufferは空になる false: 変換されず，バッファはkeyが追加されて残る(qelf前提)
	BOOL translate_qelf(const std::wstring key, std::wstring& outstr);
private:
	std::wstring m_buffer;
	std::unordered_map<std::wstring, std::vector<std::wstring>> m_RomajiToKana;
	std::unordered_map<int, std::wstring>m_KigouToJpnKigou;
};

