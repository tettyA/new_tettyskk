#include "pch.h"
#include <ctffunc.h>

#include "text_service.h"
#include "display_attribute.h"
#include "edit_session.h"
#include "globals.h"

#define _isAlphabet(_key_) ((_key_ >= 'A' && _key_ <= 'Z'))

namespace {
	inline BOOL _IsCtrlKeyPressed() {
		return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	}

    inline BOOL _IsShiftKeyPressed() {
		return (GetKeyState(VK_SHIFT) & 0x8000) != 0;
	}


} // namespace

// 未確定入力として捕捉するキーか判定する。
// 対応: 英字キー(A-Z, ローマ字バッファへ追加), Backspace(1文字削除),
//       Enter(確定), Escape(取消)。他のキーは全て素通しする。
// NOTE: IME ON/OFF切替コンパートメントは未対応のため、このTIPがアクティブな間は
BOOL  CTextService::__IsKeyEaten(WPARAM wParam)const  {
    WPARAM key = (WPARAM)wParam;
    if (g_currentMode == SKKMode::Hankaku) {
        return FALSE;
    }
   // if (_isAlphabet(key)) {
    if (m_qelfTranslater._is_contain_key(key)) {
        return TRUE;
    }
    switch (key) {
    case VK_BACK:
    case VK_RETURN:
    case VK_ESCAPE:
        return TRUE;
    default:
        return FALSE;
    }

    return FALSE;
}

void CTextService::__changeCurrentMode(const SKKMode mode)
{
    g_currentMode = mode;
    if (m_pLangBarItemButton != nullptr) {
        m_pLangBarItemButton->_UpdateIcon();
    }
}

void CTextService::__changeCurrentKanaMode(const KanaMode mode)
{
    g_CurrentKanaMode = mode;
	if (m_pLangBarItemButton != nullptr) {
		m_pLangBarItemButton->_UpdateIcon();
	}
}


CTextService::CTextService() {
    initialize_setting();

    _refCount = 1;
    _pThreadMgr = nullptr;
    _clientId = TF_CLIENTID_NULL;
	m_displayAttributeInputAtom = TF_INVALID_GUIDATOM;
	m_romajiKanaTranslater = romaji_kana_translater();
	m_qelfTranslater = qelf_translater();


    m_pLangBarItemButton = new CLangBarItemButton(this, GUID_LBI_INPUTMODE);
    
	m_qelfTranslater.load_KMP_table(g_Setting.kmp_file_path);

	g_CurrentKanaMode = KanaMode::Hiragana;
	g_currentMode = SKKMode::Hankaku;

	m_skkdictProvider = SKKDictionaryProvider();
	m_skkdictProvider.LoadDictionary(g_Setting.skk_dic_file_path, g_Setting.skk_user_dic_file_path);

    InterlockedIncrement(&g_cRefDll);
}

CTextService::~CTextService() { 
    if (m_pLangBarItemButton)
    {
        m_pLangBarItemButton.Release();
        m_pLangBarItemButton = nullptr;
    }
    if (_pThreadMgr)
    {
        _pThreadMgr.Release();
        _pThreadMgr = nullptr;
    }
    InterlockedDecrement(&g_cRefDll); 
    
}

STDAPI CTextService::QueryInterface(REFIID riid, void** ppvObj) {
    if (ppvObj == nullptr) {
        return E_INVALIDARG;
    }
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfTextInputProcessor)) {
        *ppvObj = static_cast<ITfTextInputProcessor*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    }
    else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
        *ppvObj = static_cast<ITfDisplayAttributeProvider*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDAPI_(ULONG) CTextService::AddRef() {
    return InterlockedIncrement(&_refCount);
}

STDAPI_(ULONG) CTextService::Release() {
    LONG refCount = InterlockedDecrement(&_refCount);
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}

STDAPI CTextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
    _pThreadMgr = ptim;
    _clientId = tid;

	CComPtr<ITfCategoryMgr> pCategoryMgr;
	if (SUCCEEDED(pCategoryMgr.CoCreateInstance(CLSID_TF_CategoryMgr))) {
		pCategoryMgr->RegisterGUID(GUID_DisplayAttributeInput, &m_displayAttributeInputAtom);
    }
    

    CComPtr<ITfKeystrokeMgr> pKeystrokeMgr;
    if (FAILED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
        return E_FAIL;
    }



    //キーが押されたときに通知を受け取るように登録
    HRESULT hr = pKeystrokeMgr->AdviseKeyEventSink(
        _clientId,
        static_cast<ITfKeyEventSink*>(this),
        TRUE
    );

    if (m_pLangBarItemButton) {
        m_pLangBarItemButton->_Init();
    }

    return hr;
}

STDAPI CTextService::Deactivate() {

    if (_pThreadMgr == nullptr || _clientId == TF_CLIENTID_NULL) {
        return E_FAIL;
    }
    if (m_pLangBarItemButton) {
        m_pLangBarItemButton->_Uninit();
    }

    // composition が残っている場合、ここではEditSessionを発行できない
    // (Deactivate自体がedit sessionの外で呼ばれるため、有効なITfContextを
    //  保持していない)。ローカル状態のみ破棄する。通常はフォーカス移動時に
    // OnCompositionTerminated経由で先に処理されるため、ここに到達するのは
    // 異常系(TIP強制解除など)への保険。
    if (_pComposition != nullptr) {
        _pComposition.Release();
        _compositionBuffer.clear();
    }

    CComPtr<ITfKeystrokeMgr> pKeystrokeMgr;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
        pKeystrokeMgr->UnadviseKeyEventSink(_clientId);
    }

    _pThreadMgr.Release();
    _clientId = TF_CLIENTID_NULL;

    return S_OK;
}

STDAPI CTextService::OnSetFocus(BOOL fForeground) {
    return S_OK;
}

// このキーをどのように処理するか呼び出される(キーが押された瞬間)
STDAPI CTextService::OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (pfEaten == nullptr) {
        return E_INVALIDARG;
    }
    *pfEaten = __IsKeyEaten(wParam);
    if (_IsCtrlKeyPressed() && wParam == 'J') {
        *pfEaten = TRUE;
    }

    return S_OK;
}

// このキーをどのように処理するか呼び出される(キーが離された瞬間)


// キーが押された瞬間
STDAPI CTextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (pfEaten == nullptr || pic == nullptr) {
        return E_INVALIDARG;
    }

    *pfEaten = __IsKeyEaten(wParam);

    if (_IsCtrlKeyPressed() && wParam == 'J') {
        *pfEaten = TRUE;
        __changeCurrentMode(SKKMode::Kakutei);
		__changeCurrentKanaMode(KanaMode::Hiragana);
        return S_OK;
    }
   

    if (!(*pfEaten)) {
        return S_OK;
    }

    // ここにいるということは，SKKMode::Hankaku ではない。
    if (wParam == 'R') {
        __changeCurrentMode(SKKMode::Hankaku);
        return S_OK;
    }
    else if (wParam == 'Q') {
        *pfEaten = TRUE;
        __changeCurrentMode(SKKMode::Kakutei);
        __changeCurrentKanaMode(KanaMode::Katakana);
        return S_OK;
    }

    // Enter で そのまま(変換せずに) 確定処理 
    if (wParam == VK_RETURN) {
        if (!__HasComposition()) {
            *pfEaten = FALSE; // composition中でなければEnterはアプリに渡す
            return S_OK;
        }
        HRESULT hrSession = S_OK;
		__RequestEditSession(pic, EditSessionRequest::EndCompositionCommit, hrSession);
        return S_OK;
    }

	// Escape で compositionをキャンセルする
    if (wParam == VK_ESCAPE) {
        if (!__HasComposition()) {
            *pfEaten = FALSE;
            return S_OK;
        }
        HRESULT hrSession = S_OK;
		__RequestEditSession(pic, EditSessionRequest::EndCompositionCancel, hrSession);
        return S_OK;
    }

	// 削除キーでバッファの最後の文字を削除する
    if (wParam == VK_BACK) {
        if (!__HasComposition()) {
            *pfEaten = FALSE; // composition中でなければBackspaceはアプリに渡す
            return S_OK;
        }
        if (_compositionBuffer.empty()) {
            *pfEaten = FALSE;
            return S_OK;
        }
        _compositionBuffer.pop_back();
        HRESULT hrSession = S_OK;
        if (_compositionBuffer.empty()) {// 空なら終了
			__RequestEditSession(pic, EditSessionRequest::EndCompositionCancel, hrSession);
        }
		else {// 空でなければ更新
			__RequestEditSession(pic, EditSessionRequest::DoComposition, hrSession);
        }
        return S_OK;
    }

    // 英字キー: バッファへ追加。
    WCHAR ch = static_cast<WCHAR>(wParam);
    
	DebugTrace((L"OnKeyDown: " + std::wstring(1,ch)));
    std::wstring kekkabutu = L"";
    if (m_qelfTranslater.translate_qwerty_to_qelf(ch, kekkabutu)) {
        std::wstring output;
        if (m_romajiKanaTranslater.translate_qelf(kekkabutu, output)) {
            _compositionBuffer = output;
        }
    }
    
    if (g_currentMode == SKKMode::Kakutei) {
		HRESULT hrSession = S_OK;
		__RequestEditSession(pic, EditSessionRequest::InsertText, hrSession);
    }
    else {
        HRESULT hrSession = S_OK;
        EditSessionRequest req = (!__HasComposition())
            ? EditSessionRequest::StartComposition
            : EditSessionRequest::DoComposition;
        __RequestEditSession(pic, req, hrSession);
    }
    return S_OK;
}

STDAPI CTextService::OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (pfEaten == nullptr) {
        return E_INVALIDARG;
    }
    *pfEaten = FALSE;
    return S_OK;
}

// キーが離された瞬間
STDAPI CTextService::OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam,
    BOOL* pfEaten) {
    if (pfEaten == nullptr) {
        return E_INVALIDARG;
    }
    *pfEaten = FALSE;
    return S_OK;
}

// 保存されたキーが押されたときに呼び出される
STDAPI CTextService::OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) {
    if (pfEaten == nullptr) {
        return E_INVALIDARG;
    }
    *pfEaten = FALSE;
    return S_OK;
}

STDAPI CTextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) {
    // フォーカス移動などでTSF側から強制終了された場合。
    // 既に終了済みのためEndCompositionを呼んではならず、ローカル状態のクリアのみ行う。
    _pComposition.Release();
    _compositionBuffer.clear();
    return S_OK;
}

STDAPI CTextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) {
    if (ppEnum == nullptr) {
        return E_INVALIDARG;
    }
    *ppEnum = new CEnumDisplayAttributeInfo();
    return (*ppEnum != nullptr) ? S_OK : E_OUTOFMEMORY;
}

STDAPI CTextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo) {
    if (ppInfo == nullptr) {
        return E_INVALIDARG;
    }
    *ppInfo = nullptr;

    if (!IsEqualGUID(guid, GUID_DisplayAttributeInput)) {
        return E_INVALIDARG;
    }

    *ppInfo = new CDisplayAttributeInfo();
    return (*ppInfo != nullptr) ? S_OK : E_OUTOFMEMORY;
}

HRESULT CTextService::_StartComposition(TfEditCookie ec, ITfContext* pContext) {
    if (pContext == nullptr) {
        return E_INVALIDARG;
    }

    CComPtr<ITfInsertAtSelection> pInsertAtSelection;
    if (FAILED(pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection))) {
        return E_FAIL;
    }

    // TF_IAS_QUERYONLY: テキストは挿入せず、現在の選択位置のRangeのみ取得する。
    CComPtr<ITfRange> pRange;
    if (FAILED(pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, &pRange))) {
        return E_FAIL;
    }

    CComPtr<ITfContextComposition> pContextComposition;
    if (FAILED(pContext->QueryInterface(IID_ITfContextComposition, (void**)&pContextComposition))) {
        return E_FAIL;
    }

    CComPtr<ITfComposition> pComposition;
    if (FAILED(pContextComposition->StartComposition(
        ec, pRange, static_cast<ITfCompositionSink*>(this), &pComposition))) {
        return E_FAIL;
    }

    _pComposition = pComposition;
    return _DoComposition(ec, pContext);
}

HRESULT CTextService::_DoComposition(TfEditCookie ec, ITfContext* pContext) {
    if (!__HasComposition() || pContext == nullptr) {
        return E_FAIL;
    }

    CComPtr<ITfRange> pRange;
    if (FAILED(_pComposition->GetRange(&pRange))) {
        return E_FAIL;
    }

    if (FAILED(pRange->SetText(ec, 0, _compositionBuffer.c_str(),
        (LONG)_compositionBuffer.length()))) {
        return E_FAIL;
    }

    // 未確定文字列であることを示す表示属性(下線)をRangeに適用する。
    CComPtr<ITfProperty> pProperty;
    if (SUCCEEDED(pContext->GetProperty(GUID_PROP_ATTRIBUTE, &pProperty))) {
        VARIANT var;
        VariantInit(&var);
        var.vt = VT_I4;

        if (m_displayAttributeInputAtom != TF_INVALID_GUIDATOM) {
			var.lVal = (LONG)m_displayAttributeInputAtom;
            if (FAILED(pProperty->SetValue(ec, pRange, &var))) {
                return E_FAIL;
            }
        }
    }

    // キャレットをcomposition末尾に移動する。
    pRange->Collapse(ec, TF_ANCHOR_END);
    TF_SELECTION selection;
    selection.range = pRange;
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    pContext->SetSelection(ec, 1, &selection);

    return S_OK;
}

HRESULT CTextService::_InsertText(TfEditCookie ec, ITfContext* pContext)
{
    if (pContext == nullptr || _compositionBuffer.empty()) {
        return E_FAIL;
    }

    CComPtr<ITfInsertAtSelection> pInsertAtSelection;
    if (FAILED(pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection))) {
        return E_FAIL;
    }

    CComPtr<ITfRange> pRange;
    if (FAILED(pInsertAtSelection->InsertTextAtSelection(ec, 0, _compositionBuffer.c_str(), (LONG)_compositionBuffer.length(), &pRange))) {
        return E_FAIL;
    }

    pRange->Collapse(ec, TF_ANCHOR_END);
    TF_SELECTION selection;
    selection.range = pRange;
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    pContext->SetSelection(ec, 1, &selection);

    _compositionBuffer.clear();
    return S_OK;
}

HRESULT CTextService::_EndComposition(TfEditCookie ec, ITfContext* pContext, BOOL fCommit) {
    if (!__HasComposition()) {
        return E_FAIL;
    }

    if (!fCommit) {
        CComPtr<ITfRange> pRange;
        if (SUCCEEDED(_pComposition->GetRange(&pRange))) {
            pRange->SetText(ec, 0, L"", 0);
        }
    }

    _pComposition->EndComposition(ec);
    _pComposition.Release();
    _compositionBuffer.clear();

    return S_OK;
}



inline void CTextService::__RequestEditSession(ITfContext* pContext, EditSessionRequest req, HRESULT hrSession) {
    CComPtr<CEditSession> pEditSession;
    pEditSession.Attach(new CEditSession(this, pContext, req));
    pContext->RequestEditSession(_clientId, pEditSession, TF_ES_SYNC | TF_ES_READWRITE, &hrSession);
}
