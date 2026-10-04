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
	int candidate_window_font_size;
	LPCWSTR candidate_window_font_name;
	COLORREF candidate_window_normal_font_color;
	COLORREF candidate_window_unconfirmed_font_color;
	COLORREF candidate_window_dim_font_color;
	COLORREF candidate_window_hilight_font_color;
	COLORREF candidate_window_asdfjkl_font_color;
};

extern settings_value g_Setting;

void initialize_setting();
void initialize_setting_display_attribute();