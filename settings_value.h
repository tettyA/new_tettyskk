#pragma once
#define WIN32_LEAN_AND_MEAN
#include <atlbase.h>
#include <msctf.h>
#include <windows.h>

struct settings_value {
    TF_DISPLAYATTRIBUTE display_attribute;
};

extern settings_value g_Setting;

void initialize_setting();
void initialize_setting_display_attribute();