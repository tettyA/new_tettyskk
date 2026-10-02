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
	Hankaku, // ”¼Šp‰p”“ü—Íƒ‚[ƒh
	Kakutei, // ‚»‚Ì‚Ü‚Ü•½‰¼–¼(or•Ğ‰¼–¼)‚Å“ü—Í‚µ‚Ä‘¦Šm’è
	Henkan,  // •ÏŠ·‚·‚é
};

enum class KanaMode {
	Hiragana, 
	Katakana
};

DECLSPEC_SELECTANY SKKMode g_currentMode;
DECLSPEC_SELECTANY KanaMode g_CurrentKanaMode;



#define gtettySKK_IME_NAME TEXT("new_tettySKK")

#ifdef _DEBUG
#define DebugTrace(msg) OutputDebugString((L"[DEBUG-tettySKK] "+ (msg)).c_str())
#else
#define DebugTrace(msg) ;
#endif
