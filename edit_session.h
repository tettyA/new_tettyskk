#pragma once
#define WIN32_LEAN_AND_MEAN
#include <atlbase.h>
#include <msctf.h>
#include <windows.h>

class CTextService;

enum class EditSessionRequest {
    StartComposition,
    DoComposition,
    InsertText,
    EndCompositionCommit,
    EndCompositionCancel,
};

// CTextServiceのcomposition操作をITfContext::RequestEditSession経由で
// 呼び出すためのラッパー。TfEditCookieはRequestEditSession呼び出し側からしか
// 取得できないため、実際の処理はCTextService側のメソッドに委譲する。
class CEditSession : public ITfEditSession {
public:
    CEditSession(CTextService* pTextService, ITfContext* pContext,
        EditSessionRequest req);
    ~CEditSession();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec);

private:
    LONG _refCount;
    CTextService* _pTextService; // AddRef/Releaseで所有権を持つ
    CComPtr<ITfContext> _pContext;
    EditSessionRequest _req;
};
