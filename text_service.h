#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <atlbase.h>
#include <msctf.h>
#include <vector>
#include <string>
#include "globals.h"
#include "edit_session.h"
#include "romaji_kana_translater.h"
#include "qelf_translater.h"
#include "ClangBarItemButton.h"
#include "SKKDictionaryProvider.h"
#include "CCandidateWindow.h"

struct SKK_RegDict_State {
    SKKMode mode;
	std::wstring compositionBuffer;
    std::wstring henkanGokan;
    std::wstring okurigana;
    WCHAR okuriganashiin; // 送り仮名の最初の子音 (例: 渡す -> 's')
    SKKCandidates candidates;
    int candidateIndex;
	CandidateWindowDrawMode drawmode;
    std::wstring regText; // 退避時点での登録中の確定済み文字列(入れ子登録用)
};

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
        // Deactivate後はnullptrになり得る
        if (*ppThreadMgr != nullptr) {
            (*ppThreadMgr)->AddRef();
        }
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

    void __StartHenkanMode(ITfContext* pic);
    void __EndHenkanMode(ITfContext* pic);

    void __changeCurrentMode(const SKKMode mode);
    void __changeCurrentKanaMode(const KanaMode mode);

    inline bool _IsCtrlKeyPressed();

    // 現在の候補を未確定バッファ(候補+送り仮名)に反映する
    void _SetCandidateText();
    // 変換中の文字列を確定する(辞書登録中は登録用バッファへ取り込む)
    void _CommitHenkan(ITfContext* pic);
    // 辞書登録を終了し、退避した変換状態を復元する。apply=trueなら登録した語で確定する
    void _ExitDictionaryRegistration(ITfContext* pic, bool apply);
    // 全ての変換/登録状態を破棄する(フォーカス喪失・Deactivate用)
    void _ResetAllState();

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
    SKKCandidates m_candidates;
    std::wstring m_henkanGokan; // 変換対象の語幹
    WCHAR m_okuriganashiin; // おもw の w
    std::wstring m_okurigana; // [Shift]おも[Shift]わ の わ
    int m_candidateIndex;


    // 候補描画関連
	void _IncrementCandidateIndex();
	void _DecrementCandidateIndex();

    std::vector<SKK_RegDict_State> m_stateStack;
    std::wstring m_regText; // 辞書登録中に確定した文字列(変換中の読みは_compositionBufferに入る)
    CandidateWindowDrawMode m_drawmode;

    CCandidateWindow m_candidateWindow;

    void _UpdateCandidateWindow(TfEditCookie ec, ITfContext* pContext);
    void _HideCandidateWindow();
    void _EnterDictionaryRegistration(ITfContext* pContext);
    void _OnDictionaryRegistration(const std::wstring& reading, WCHAR okuriganashiin, const std::wstring& word);
    bool _GetCaretRect(TfEditCookie ec, ITfContext* pContext, RECT* pRect);

};

