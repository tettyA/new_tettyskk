#pragma once
#define WIN32_LEAN_AND_MEAN
#include <atlbase.h>
#include <msctf.h>
#include <windows.h>

#include <string>

struct settings_value {
    TF_DISPLAYATTRIBUTE display_attribute;
	std::wstring kmp_file_path;
	std::wstring skk_dic_file_path;
	std::wstring skk_user_dic_file_path;
	int single_candidate_max_count;
};

extern settings_value g_Setting;

void initialize_setting();
void initialize_setting_display_attribute();