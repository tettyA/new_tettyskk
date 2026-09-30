#include "pch.h"
#include "edit_session.h"
#include "text_service.h"

CEditSession::CEditSession(CTextService* pTextService, ITfContext* pContext,
    EditSessionRequest req)
    : _refCount(1), _pTextService(pTextService), _pContext(pContext),
    _req(req) {
    if (_pTextService != nullptr) {
        _pTextService->AddRef();
    }
}

CEditSession::~CEditSession() {
    if (_pTextService != nullptr) {
        _pTextService->Release();
    }
}

STDAPI CEditSession::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) {
        return E_INVALIDARG;
    }
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
        *ppvObj = static_cast<ITfEditSession*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDAPI_(ULONG) CEditSession::AddRef() {
    return InterlockedIncrement(&_refCount);
}

STDAPI_(ULONG) CEditSession::Release() {
    LONG refCount = InterlockedDecrement(&_refCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

STDAPI CEditSession::DoEditSession(TfEditCookie ec) {
    if (_pTextService == nullptr || _pContext == nullptr) {
        return E_FAIL;
    }

    switch (_req) {
    case EditSessionRequest::StartComposition:
        return _pTextService->_StartComposition(ec, _pContext);
    case EditSessionRequest::DoComposition:
        return _pTextService->_DoComposition(ec, _pContext);
    case EditSessionRequest::InsertText:
        return _pTextService->_InsertText(ec, _pContext);
    case EditSessionRequest::EndCompositionCommit:
        return _pTextService->_EndComposition(ec, _pContext, TRUE);
    case EditSessionRequest::EndCompositionCancel:
        return _pTextService->_EndComposition(ec, _pContext, FALSE);
    }

    return E_UNEXPECTED;
}
