#include "pch.h"
#define WIN32_LEAN_AND_MEAN
#include <atlbase.h>
#include <msctf.h>
#include <strsafe.h>
#include <windows.h>

#include "class_factory.h"
#include "globals.h"

#define CLSIDLEN 39
#define _INPROCSERVER32STR TEXT("InprocServer32")
#define _THREADINGMODELSTR TEXT("ThreadingModel")
#define APARTMENTSTR TEXT("Apartment")
#define CLSIDSTR_ TEXT("CLSID")

#define IME_ICON_IDX 0

#define TEXTSERVICE_LANGID MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT)

static const GUID cGUID_Category[]{
    GUID_TFCAT_TIP_KEYBOARD, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT };

BOOL SetRegistryValue(HKEY hKeyRoot, LPCWSTR lpSubKey, LPCWSTR lpValueName,
    DWORD dwType, CONST BYTE* lpData, DWORD cbData) {
    HKEY hKey;
    if (RegCreateKeyEx(hKeyRoot, lpSubKey, 0, NULL, REG_OPTION_NON_VOLATILE,
        KEY_WRITE, NULL, &hKey, NULL)) {
        return FALSE;
    }
    BOOL bResult = (RegSetValueEx(hKey, lpValueName, 0, dwType, lpData,
        cbData) == ERROR_SUCCESS);
    RegCloseKey(hKey);

    return bResult;
}

long g_cRefDll = 0; // global DLL reference counter
_Check_return_ STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid,
    _Outptr_ LPVOID FAR* ppv) {
    if (ppv == nullptr) {
        return E_INVALIDARG;
    }
    *ppv = nullptr;
    if (!IsEqualIID(rclsid, CLSID_new_tettyskk)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    CClassFactory* pFactory = new CClassFactory();
    if (pFactory == nullptr) {
        return E_OUTOFMEMORY;
    }

    HRESULT hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release(); // Release the initial reference held by the factory
    return hr;
}

STDAPI DllCanUnloadNow(void) { return (g_cRefDll == 0) ? S_OK : S_FALSE; }

// ref
// https://github.com/nathancorvussolis/corvusskk/blob/2904b3ad7ba80e66e717aef6805164c74fcec71d/imcrvtip/Register.cpp#L49
STDAPI DllRegisterServer(void) {
    HRESULT hr = E_FAIL;
    WCHAR szClsidString[CLSIDLEN];
    WCHAR szDllPath[MAX_PATH];
    WCHAR szRegKeyPath[256];

    // CLSIDを文字列に変換
    if (!StringFromGUID2(CLSID_new_tettyskk, szClsidString,
        ARRAYSIZE(szClsidString))) {
        return E_FAIL;
    }

    // HKEY_CLASSES_ROOT/CLSID/{XXX....}にキーを作成
    StringCchPrintf(szRegKeyPath, ARRAYSIZE(szRegKeyPath),
        CLSIDSTR_ TEXT("\\%s"), szClsidString);
    if (!SetRegistryValue(HKEY_CLASSES_ROOT, szRegKeyPath, NULL, REG_SZ,
        (const BYTE*)gtettySKK_IME_NAME,
        (wcslen(gtettySKK_IME_NAME) + 1) * sizeof(WCHAR))) {
        return E_FAIL;
    }

    // InprocServer32サブキーにこのDLLのフルパスを書き込む
    StringCchPrintf(szRegKeyPath, ARRAYSIZE(szRegKeyPath),
        CLSIDSTR_ TEXT("\\%s\\") _INPROCSERVER32STR, szClsidString);
    if (GetModuleFileName(g_hModule, szDllPath, ARRAYSIZE(szDllPath)) == FALSE) {
        return E_FAIL;
    }
    if (!SetRegistryValue(HKEY_CLASSES_ROOT, szRegKeyPath, NULL, REG_SZ,
        (const BYTE*)szDllPath,
        (wcslen(szDllPath) + 1) * sizeof(WCHAR))) {
        return E_FAIL;
    }

    // スレッドモデルをApartmentとして書き込む。
    if (!SetRegistryValue(HKEY_CLASSES_ROOT, szRegKeyPath, _THREADINGMODELSTR,
        REG_SZ, (const BYTE*)APARTMENTSTR,
        (wcslen(APARTMENTSTR) + 1) * sizeof(WCHAR))) {
        return E_FAIL;
    }

    // Register Profiles
    // ref
    // https://github.com/nathancorvussolis/corvusskk/blob/2904b3ad7ba80e66e717aef6805164c74fcec71d/imcrvtip/Register.cpp#L35
    // TSFにIMEとして登録

    // プロファイルの登録
    {
        CComPtr<ITfInputProcessorProfileMgr> pProfileMgr;
        hr = pProfileMgr.CoCreateInstance(CLSID_TF_InputProcessorProfiles);
        if (SUCCEEDED(hr) && pProfileMgr != nullptr) {
            hr = pProfileMgr->RegisterProfile(
                CLSID_new_tettyskk, TEXTSERVICE_LANGID, GUID_Profile_SkkIme,
                gtettySKK_IME_NAME, (ULONG)wcslen(gtettySKK_IME_NAME),
                szDllPath, (ULONG)wcslen(szDllPath), IME_ICON_IDX, NULL, 0,
                TRUE, 0);
        }
        if (FAILED(hr))
            return hr;
    }

    // カテゴリの登録
    {
        CComPtr<ITfCategoryMgr> pCategoryMgr;
        HRESULT hrCat = pCategoryMgr.CoCreateInstance(CLSID_TF_CategoryMgr);
        if (SUCCEEDED(hrCat) && pCategoryMgr != nullptr) {
            for (int i = 0; i < _countof(cGUID_Category); i++) {
                pCategoryMgr->RegisterCategory(
                    CLSID_new_tettyskk, cGUID_Category[i], CLSID_new_tettyskk);
            }
        }
    }

    return hr;
}

STDAPI DllUnregisterServer(void) {
    WCHAR szClsidString[CLSIDLEN];
    WCHAR szRegKeyPath[MAX_PATH];

    if (!StringFromGUID2(CLSID_new_tettyskk, szClsidString,
        ARRAYSIZE(szClsidString))) {
        return E_FAIL;
    }

    // Unregister TSF Profiles
    {
        CComPtr<ITfInputProcessorProfileMgr> pProfileMgr;
        if (SUCCEEDED(pProfileMgr.CoCreateInstance(
            CLSID_TF_InputProcessorProfiles)) &&
            pProfileMgr != nullptr) {
            pProfileMgr->UnregisterProfile(
                CLSID_new_tettyskk, TEXTSERVICE_LANGID, GUID_Profile_SkkIme,
                (DWORD)TF_URP_ALLPROFILES);
        }
    }

    // Unregister Categories
    {
        CComPtr<ITfCategoryMgr> pCategoryMgr;
        if (SUCCEEDED(pCategoryMgr.CoCreateInstance(CLSID_TF_CategoryMgr)) &&
            pCategoryMgr != nullptr) {
            for (int i = 0; i < _countof(cGUID_Category); i++) {
                pCategoryMgr->UnregisterCategory(
                    CLSID_new_tettyskk, cGUID_Category[i], CLSID_new_tettyskk);
            }
        }
    }

    StringCchPrintf(szRegKeyPath, ARRAYSIZE(szRegKeyPath),
        CLSIDSTR_ TEXT("\\%s\\") _INPROCSERVER32STR, szClsidString);
    RegDeleteKey(HKEY_CLASSES_ROOT, szRegKeyPath);
    StringCchPrintf(szRegKeyPath, ARRAYSIZE(szRegKeyPath),
        CLSIDSTR_ TEXT("\\%s"), szClsidString);
    RegDeleteKey(HKEY_CLASSES_ROOT, szRegKeyPath);

    return S_OK;
}