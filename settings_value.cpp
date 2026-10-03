#include "pch.h"
#include "settings_value.h"

__declspec(selectany) settings_value g_Setting; // 設定値のグローバル変数

void initialize_setting() {
    initialize_setting_display_attribute();

    g_Setting.kmp_file_path = LR"(C:\skktemp\kmp.kmp)";
	g_Setting.skk_dic_file_path = LR"(C:\skktemp\SKK-JISYO.L)";
	g_Setting.skk_user_dic_file_path = LR"(C:\skktemp\SKK-JISYO.user)";
    g_Setting.single_candidate_max_count = 5;

	g_Setting.candidate_window_font_size = 46; // 候補ウィンドウのフォントサイズを設定
	g_Setting.candidate_window_font_name = L"Meiryo UI"; // 候補ウィンドウのフォント名を設定
	g_Setting.candidate_window_font_color = RGB(0, 0, 0); // 候補ウィンドウのフォント色を設定 
	g_Setting.candidate_window_dim_font_color = RGB(128, 128, 128); // 候補ウィンドウの薄いフォント色を設定
	g_Setting.candidate_window_hilight_font_color = RGB(163, 41, 99); // 候補ウィンドウのハイライトフォント色を設定
	g_Setting.candidate_window_asdfjkl_font_color = RGB(106, 160, 99); // 候補ウィンドウのASDFJKLフォント色を設定

}

void initialize_setting_display_attribute() {
    g_Setting.display_attribute.crText.type = TF_CT_NONE; // テキストの前景色 (指定せず)
    g_Setting.display_attribute.crBk.type = TF_CT_NONE; // テキストの背景色 (指定せず)
    g_Setting.display_attribute.lsStyle = TF_LS_SOLID; // 下線の種類 (実線)
    g_Setting.display_attribute.fBoldLine = FALSE; // 下線を太くするか (通常の太さ)
    g_Setting.display_attribute.crLine.type = TF_CT_NONE; // 下線の色 (指定せず)
    g_Setting.display_attribute.bAttr = TF_ATTR_INPUT; // 未確定文字列の属性
}