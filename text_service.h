#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <atlbase.h>
#include <msctf.h>
#include <string>
#include "globals.h"
#include "edit_session.h"
#include "romaji_kana_translater.h"
#include "qelf_translater.h"
#include "ClangBarItemButton.h"
#include "SKKDictionaryProvider.h"


class CTextService : public ITfTextInputProcessor,
    public ITfKeyEventSink,
    public ITfCompositionSink,
    public ITfDisplayAttributeProvider {
public:
    CTextService();
    ~CTextService();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfTextInputProcessor methods
    STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid);
    STDMETHODIMP Deactivate();

    // ITfKeyEventSink methods
    STDMETHODIMP OnSetFocus(BOOL fForeground);
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
        BOOL* pfEaten);
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten);

    // ITfCompositionSink methods
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition);

    // ITfDisplayAttributeProvider methods
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum);
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo);


    inline void __GetThreadMgr(ITfThreadMgr** ppThreadMgr) const
    {
        if (ppThreadMgr == nullptr)return;
        *ppThreadMgr = _pThreadMgr;
        (*ppThreadMgr)->AddRef();
    }
private:
    friend class CEditSession;

    // CEditSession経由(RequestEditSessionのコールバック内)からのみ呼ばれる。
    // 変換ロジックは未実装で、入力した英字をそのままバッファに反映するのみ。
    HRESULT _StartComposition(TfEditCookie ec, ITfContext* pContext);
    HRESULT _DoComposition(TfEditCookie ec, ITfContext* pContext);
    HRESULT _InsertText(TfEditCookie ec, ITfContext* pContext);
    HRESULT _EndComposition(TfEditCookie ec, ITfContext* pContext, BOOL fCommit);

	inline void __RequestEditSession(ITfContext* pContext, EditSessionRequest req, HRESULT hrSession);
	inline bool __HasComposition() const { return _pComposition != nullptr; }
	BOOL __IsKeyEaten(WPARAM wParam) const;

    void __changeCurrentMode(const SKKMode mode);
    void __changeCurrentKanaMode(const KanaMode mode);

    LONG _refCount;
    // TSF 連絡用
    CComPtr<ITfThreadMgr> _pThreadMgr;
    TfClientId _clientId;

    // composition(未確定文字列)の状態。
    CComPtr<ITfComposition> _pComposition;
    std::wstring _compositionBuffer;

	TfGuidAtom m_displayAttributeInputAtom; // GUID_DisplayAttributeInputのTfGuidAtom

	romaji_kana_translater m_romajiKanaTranslater;
	qelf_translater m_qelfTranslater;

	CComPtr<CLangBarItemButton> m_pLangBarItemButton;

	SKKDictionaryProvider m_skkdictProvider;
};