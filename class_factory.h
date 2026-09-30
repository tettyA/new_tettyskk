#pragma once
#define WIN32_LEAN_AND_MEAN
#include <unknwn.h>
#include <windows.h>

class CClassFactory : public IClassFactory {
public:
    CClassFactory();
    ~CClassFactory();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();
    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid,
        void** ppvObject);
    STDMETHODIMP LockServer(BOOL fLock);

private:
    LONG m_cRef;
};