#include "pch.h"
#include "display_attribute.h"


void reset_setting_display_attribute() {
    initialize_setting_display_attribute();
}

CDisplayAttributeInfo::CDisplayAttributeInfo() : _refCount(1) {
    reset_setting_display_attribute();
    // _attribute = g_Setting.display_attribute; // 設定値のグローバル変数から初期化
}

CDisplayAttributeInfo::~CDisplayAttributeInfo() {}

STDAPI CDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) {
        return E_INVALIDARG;
    }
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
        *ppvObj = static_cast<ITfDisplayAttributeInfo*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDAPI_(ULONG) CDisplayAttributeInfo::AddRef() {
    return InterlockedIncrement(&_refCount);
}

STDAPI_(ULONG) CDisplayAttributeInfo::Release() {
    LONG refCount = InterlockedDecrement(&_refCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

STDAPI CDisplayAttributeInfo::GetGUID(GUID* pguid) {
    if (pguid == nullptr) {
        return E_INVALIDARG;
    }
    *pguid = GUID_DisplayAttributeInput;
    return S_OK;
}

STDAPI CDisplayAttributeInfo::GetDescription(BSTR* pbstrDesc) {
    if (pbstrDesc == nullptr) {
        return E_INVALIDARG;
    }
    *pbstrDesc = SysAllocString(L"tettySKK Input Attribute");
    return (*pbstrDesc != nullptr) ? S_OK : E_OUTOFMEMORY;
}

STDAPI CDisplayAttributeInfo::GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) {
    if (pda == nullptr) {
        return E_INVALIDARG;
    }
    *pda = g_Setting.display_attribute;//_attribute;
    return S_OK;
}

STDAPI CDisplayAttributeInfo::SetAttributeInfo(const TF_DISPLAYATTRIBUTE* pda) {
    if (pda == nullptr) {
        return E_INVALIDARG;
    }
    //  _attribute = *pda;
    g_Setting.display_attribute = *pda;
    return S_OK;
}

STDAPI CDisplayAttributeInfo::Reset() {
    reset_setting_display_attribute(); // 設定値のグローバル変数からリセット
    return S_OK;
}

CEnumDisplayAttributeInfo::CEnumDisplayAttributeInfo() : _refCount(1), _index(0) {}
CEnumDisplayAttributeInfo::~CEnumDisplayAttributeInfo() {}

STDAPI CEnumDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) {
        return E_INVALIDARG;
    }
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_IEnumTfDisplayAttributeInfo)) {
        *ppvObj = static_cast<IEnumTfDisplayAttributeInfo*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDAPI_(ULONG) CEnumDisplayAttributeInfo::AddRef() {
    return InterlockedIncrement(&_refCount);
}

STDAPI_(ULONG) CEnumDisplayAttributeInfo::Release() {
    LONG refCount = InterlockedDecrement(&_refCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

STDAPI CEnumDisplayAttributeInfo::Clone(IEnumTfDisplayAttributeInfo** ppEnum) {
    if (ppEnum == nullptr) {
        return E_INVALIDARG;
    }
    CEnumDisplayAttributeInfo* pClone = new CEnumDisplayAttributeInfo();
    if (pClone == nullptr) {
        return E_OUTOFMEMORY;
    }
    pClone->_index = _index;
    *ppEnum = pClone;
    return S_OK;
}

STDAPI CEnumDisplayAttributeInfo::Next(ULONG ulCount,
    ITfDisplayAttributeInfo** rgInfo, ULONG* pcFetched) {
    ULONG fetched = 0;
    while (fetched < ulCount && _index == 0) {
        CDisplayAttributeInfo* pInfo = new CDisplayAttributeInfo();
        if (pInfo == nullptr) {
            break;
        }
        rgInfo[fetched] = pInfo;
        _index++;
        fetched++;
    }
    if (pcFetched != nullptr) {
        *pcFetched = fetched;
    }
    return (fetched == ulCount) ? S_OK : S_FALSE;
}

STDAPI CEnumDisplayAttributeInfo::Reset() {
    _index = 0;
    return S_OK;
}

STDAPI CEnumDisplayAttributeInfo::Skip(ULONG ulCount) {
    if (ulCount > 0) {
        _index = 1;
    }
    return S_OK;
}

