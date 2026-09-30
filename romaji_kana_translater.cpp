#include "pch.h"
#include "romaji_kana_translater.h"
#include "globals.h"

romaji_kana_translater::romaji_kana_translater(){
	m_RomajiToKana = {
        #include "romaji2kana_transtable.h"
	};
	m_KigouToJpnKigou = {
		#include "kigou2kana_translatable.h"
	};

}

romaji_kana_translater::~romaji_kana_translater()
{}

BOOL romaji_kana_translater::translate_qelf(const std::wstring input, std::wstring& outstr)
{
	outstr.clear();

	m_buffer += input;
	DebugTrace((L"translate_qelf: buffer=[" + m_buffer + L"]"));
	//変換出来なければ，確定で変換出来ない。
	auto it = m_RomajiToKana.find(m_buffer);
	if (it != m_RomajiToKana.end()) {
		outstr = it->second[(g_CurrentKanaMode == KanaMode::Hiragana ? TrR2K_INDEX_HIRAGANA : TrR2K_INDEX_KATAKANA)];
		m_buffer.clear();
		return TRUE;
	}
	m_buffer.clear();


	return 0;
}