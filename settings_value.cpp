#include "pch.h"
#include "settings_value.h"

__declspec(selectany) settings_value g_Setting; // 設定値のグローバル変数

void initialize_setting() {
    initialize_setting_display_attribute();
}

void initialize_setting_display_attribute() {
    g_Setting.display_attribute.crText.type = TF_CT_NONE; // テキストの前景色 (指定せず)
    g_Setting.display_attribute.crBk.type = TF_CT_NONE; // テキストの背景色 (指定せず)
    g_Setting.display_attribute.lsStyle = TF_LS_SOLID; // 下線の種類 (実線)
    g_Setting.display_attribute.fBoldLine = FALSE; // 下線を太くするか (通常の太さ)
    g_Setting.display_attribute.crLine.type = TF_CT_NONE; // 下線の色 (指定せず)
    g_Setting.display_attribute.bAttr = TF_ATTR_INPUT; // 未確定文字列の属性
}