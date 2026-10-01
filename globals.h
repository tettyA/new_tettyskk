#pragma once

#define WIN32_LEAN_AND_MEAN
#include <string>
#include <windows.h>
#include "pch.h"
#include "settings_value.h"

extern const GUID CLSID_new_tettyskk;
extern const GUID GUID_Profile_SkkIme;
extern const GUID GUID_DisplayAttributeInput;
// global DLL reference counter
extern LONG g_cRefDll;
extern HMODULE g_hModule;

enum class SKKMode {
	Hankaku, // îºäpâpêîì¸óÕÉÇÅ[Éh
	Kakutei, // ÇªÇÃÇ‹Ç‹ïΩâºñº(orï–âºñº)Ç≈ì¸óÕÇµÇƒë¶ämíË
	Henkan,  // ïœä∑Ç∑ÇÈ
};

enum class KanaMode {
	Hiragana, 
	Katakana
};

DECLSPEC_SELECTANY SKKMode g_currentMode;
DECLSPEC_SELECTANY KanaMode g_CurrentKanaMode;


#define _IsCtrlKeyPressed() ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
#define _IsShiftKeyPressed() ((GetKeyState(VK_SHIFT) & 0x8000) != 0)


#define gtettySKK_IME_NAME TEXT("new_tettySKK")

#ifdef _DEBUG
#define DebugTrace(msg) OutputDebugString((L"[DEBUG-tettySKK] "+ (msg)).c_str())
#elif
#define DebugTrace(msg) ;
#endif
