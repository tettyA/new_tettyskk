#include "pch.h"

#include "class_factory.h"
#include "text_service.h"
#include "globals.h"

LONG DllAddRef(void) { return InterlockedIncrement(&g_cRefDll); }
LONG DllRelease(void) { return InterlockedDecrement(&g_cRefDll); }

CClassFactory::CClassFactory() : m_cRef(1) { InterlockedIncrement(&g_cRefDll); }
CClassFactory::~CClassFactory() { InterlockedDecrement(&g_cRefDll); }

STDAPI CClassFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) {
	if (ppvObj == nullptr) {
		return E_INVALIDARG;
	}
	*ppvObj = nullptr;

	if (pUnkOuter != nullptr) {
		return CLASS_E_NOAGGREGATION;
	}

	CTextService* pTextService = new CTextService();
	if (pTextService == nullptr) {
		return E_OUTOFMEMORY;
	}

	HRESULT hr = pTextService->QueryInterface(riid, ppvObj);
	pTextService->Release();

    return hr;
}

STDAPI CClassFactory::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) {
        return E_INVALIDARG;
    }

    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
        *ppvObj = static_cast<IClassFactory*>(this);
        
    }
    else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDAPI_(ULONG) CClassFactory::AddRef() { return InterlockedIncrement(&m_cRef); }
STDAPI_(ULONG) CClassFactory::Release() {
    LONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0) {
        delete this;
    }
    return cRef;
}


STDAPI CClassFactory::LockServer(BOOL fLock) {
    if (fLock) {
        DllAddRef();
    }
    else {
        DllRelease();
    }
    return S_OK;
}