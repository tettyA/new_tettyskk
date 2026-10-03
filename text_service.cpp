#include "pch.h"
#include <ctffunc.h>

#include "text_service.h"
#include "display_attribute.h"
#include "edit_session.h"
#include "globals.h"

#define _isAlphabet(_key_) ((_key_ >= 'A' && _key_ <= 'Z'))

// このIMEが処理するキーか判定する(TRUE=IMEが食べる / FALSE=アプリへ素通し)。
// 対応: Q-ELF配列・記号テーブルに載っているキー, Backspace/Enter/Escape(composition中のみ),
//       Space(変換中・辞書登録中のみ)。Hankakuモードと、Ctrl/Alt併用キーは全て素通しする。
// NOTE: IME ON/OFF切替コンパートメントは未対応のため、このTIPがアクティブな間は常にこの判定が働く。
BOOL  CTextService::__IsKeyEaten(WPARAM wParam)const  {
    WPARAM key = (WPARAM)wParam;
    if (g_currentMode == SKKMode::Hankaku) {
        return FALSE;
    }
    // Ctrl/Alt併用のショートカット(Ctrl+C等)はアプリに渡す。Ctrl+Jだけは呼び出し側で処理する
    if ((GetKeyState(VK_CONTROL) & 0x8000) || (GetKeyState(VK_MENU) & 0x8000)) {
        return FALSE;
    }
    if (key <= 0xFFFF && (m_qelfTranslater._is_contain_key((KeyCode)key) || m_romajiKanaTranslater.contain_kigou((KeyCode)key))) {
        return TRUE;
    }
    switch (key) {
    case VK_BACK:
    case VK_RETURN:
    case VK_ESCAPE:
        return __HasComposition();
    case VK_SPACE:
        // 変換中/辞書登録中以外のスペースは通常の空白としてアプリに渡す
        return __HasComposition() || g_currentMode == SKKMode::Henkan;
    default:
        return FALSE;
    }

    return FALSE;
}

void _ConvertToKatakana(std::wstring& str)
{
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] >= L'ぁ' && str[i] <= L'ん') {
            str[i] = str[i] - L'ぁ' + L'ァ';
        }
    }
}

void _ConvertToHiragana(std::wstring& str)
{
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] >= L'ァ' && str[i] <= L'ン') {
            str[i] = str[i] + L'ぁ' - L'ァ';
        }
    }
}

void CTextService::__StartHenkanMode(ITfContext *pic)
{
    __changeCurrentMode(SKKMode::Henkan);

    // 前回の変換状態が残っていると候補表示が壊れるため初期化する
    m_candidates.clear();
    m_candidateIndex = 0;
    m_drawmode = CandidateWindowDrawMode::Single;
    m_henkanGokan.clear();
    m_okurigana.clear();

    // 辞書登録中は既にcompositionが存在するので新規作成しない
    HRESULT hrSession = S_OK;
    __RequestEditSession(pic, __HasComposition() ? EditSessionRequest::DoComposition : EditSessionRequest::StartComposition, hrSession);
}

void CTextService::__EndHenkanMode(ITfContext* pic)
{
    // 確定・キャンセル後は次の入力に備えて変換状態とQ-ELF/ローマ字の内部状態を全て初期化する
    m_candidates.clear();
    m_candidateIndex = 0;
    m_drawmode = CandidateWindowDrawMode::Single;
	m_okurigana.clear();
	m_okuriganashiin = L'\0';
	g_currentMode = SKKMode::Kakutei;
    _HideCandidateWindow();

	m_romajiKanaTranslater.reset();
	m_qelfTranslater.reset_kmp_state();
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

inline bool CTextService::_IsCtrlKeyPressed()
{
    return (GetKeyState(VK_CONTROL) & 0x8000) != 0;
}

// 候補を未確定バッファに反映する。範囲外のindexは最後の候補に丸める
void CTextService::_SetCandidateText()
{
    if (m_candidates.empty()) {
        return;
    }
    if (m_candidateIndex < 0) {
        m_candidateIndex = 0;
    }
    if (m_candidateIndex >= (int)m_candidates.size()) {
        m_candidateIndex = (int)m_candidates.size() - 1;
    }
    _compositionBuffer = m_candidates[m_candidateIndex].candidate + m_okurigana;
}

void CTextService::_IncrementCandidateIndex()
{
	if (m_drawmode == CandidateWindowDrawMode::Single) {
        m_candidateIndex++;
        if(m_candidateIndex>=g_Setting.single_candidate_max_count){
			m_drawmode = CandidateWindowDrawMode::Multi;
        }
	}
	else if (m_drawmode == CandidateWindowDrawMode::Multi) {
        m_candidateIndex += SKK_CHOOSE_CANDIDATES_BIGSTR_LEN;
	}

}

void CTextService::_DecrementCandidateIndex()
{
	if (m_drawmode == CandidateWindowDrawMode::Single) {
		m_candidateIndex--;
		if (m_candidateIndex < 0) {
			m_candidateIndex = 0;
		}
	}
	else if (m_drawmode == CandidateWindowDrawMode::Multi) {
		m_candidateIndex -= SKK_CHOOSE_CANDIDATES_BIGSTR_LEN;
        if (m_candidateIndex < g_Setting.single_candidate_max_count) {
			m_drawmode = CandidateWindowDrawMode::Single;
			m_candidateIndex = g_Setting.single_candidate_max_count - 1;
            if (m_candidateIndex < 0) {
                m_candidateIndex = 0;
            }
        } 
	}
}

void CTextService::_UpdateCandidateWindow(TfEditCookie ec ,ITfContext* pContext)
{
    if (!m_candidateWindow.GetHwnd()) {
        m_candidateWindow.Create(NULL, g_hModule);
    }

    bool isRegisterMode = !m_stateStack.empty();
    std::wstring registerTarget = L"";
    if (isRegisterMode) {
        registerTarget = m_stateStack.back().henkanGokan;
    }

    if (m_candidates.empty() && !isRegisterMode) {
		_HideCandidateWindow();
        return;
    }

	m_candidateWindow.SetCandidates(m_candidates);
	m_candidateWindow.SetShowIndex(m_candidateIndex, m_drawmode);
	// 登録中は「確定済みの文字列 + 変換中の読み/候補」をウィンドウ上の入力として見せる
	m_candidateWindow.SetRegisterInfo(isRegisterMode, registerTarget, m_regText, _compositionBuffer);

    RECT rc = { 0 };
	if (_GetCaretRect(ec,pContext, &rc)) {
		m_candidateWindow.Show(rc.left, rc.bottom);
	}
}

void CTextService::_HideCandidateWindow()
{
	m_candidateWindow.Hide();
}

// 候補が尽きた時に呼ばれる。現在の変換状態をスタックへ退避し、登録用の入力状態(Kakutei)へ移る
void CTextService::_EnterDictionaryRegistration(ITfContext * pContext)
{
	SKK_RegDict_State state;
    state.mode = g_currentMode;
	state.compositionBuffer = _compositionBuffer;
	state.henkanGokan = m_henkanGokan;
	state.okurigana = m_okurigana;
	state.candidates = m_candidates;
	// 範囲外indexのまま退避すると復元後にm_candidates[index]で範囲外参照になるため、最後の候補・Singleに丸める
	state.candidateIndex = m_candidates.empty() ? 0 : (int)m_candidates.size() - 1;
	state.drawmode = CandidateWindowDrawMode::Single;
	state.regText = m_regText;
	m_stateStack.push_back(state);

    m_candidates.clear();
    m_candidateIndex = 0;
	m_drawmode = CandidateWindowDrawMode::Single;
    m_regText.clear();
    m_okurigana.clear();

    _compositionBuffer.clear();
	__changeCurrentMode(SKKMode::Kakutei);
    m_romajiKanaTranslater.reset();
    m_qelfTranslater.reset_kmp_state();

	HRESULT hrSession = S_OK;
	__RequestEditSession(pContext, EditSessionRequest::DoComposition, hrSession);
}

// 辞書登録を抜けて退避しておいた変換状態を復元する。
// apply=true : 入力された語で確定する(入れ子の場合は親の登録バッファへ取り込む)
// apply=false: 登録をキャンセルして変換中の状態へ戻る
void CTextService::_ExitDictionaryRegistration(ITfContext* pic, bool apply)
{
    if (m_stateStack.empty()) {
        return;
    }

    // 登録中に開始した内側の変換が残っていれば、その読み/候補も登録語に含めてから破棄する
    std::wstring word = m_regText + _compositionBuffer;
    if (g_currentMode == SKKMode::Henkan) {
        __EndHenkanMode(pic);
    }

    SKK_RegDict_State parent = m_stateStack.back();
    m_stateStack.pop_back();

    m_henkanGokan = parent.henkanGokan;
    m_okurigana = parent.okurigana;
    m_candidates = parent.candidates;
    m_candidateIndex = parent.candidateIndex;
    m_drawmode = parent.drawmode;
    m_regText = parent.regText;
    _compositionBuffer.clear();
    __changeCurrentMode(SKKMode::Henkan);

    if (apply && !word.empty()) {
        // 辞書への実登録処理(注釈付き "語;注釈" のまま渡す)
        _OnDictionaryRegistration(parent.henkanGokan, word);

        // アプリへ確定する文字列には注釈を含めない
        size_t semi = word.find(SKK_CANDIDOTATES_ANNOTATION_SEPARATOR_WCHAR);
        if (semi != std::wstring::npos) {
            word.erase(semi);
        }
        _compositionBuffer = word + m_okurigana;
        _CommitHenkan(pic);
        return;
    }

    // キャンセル: 最後の候補(無ければ読み)を表示した変換中の状態へ戻る
    if (!m_candidates.empty()) {
        _SetCandidateText();
    }
    else {
        // 候補が無い状態に戻るので送り仮名は読みへ含めず破棄する
        m_okurigana.clear();
        _compositionBuffer = m_henkanGokan;
    }
    HRESULT hrSession = S_OK;
    __RequestEditSession(pic, EditSessionRequest::DoComposition, hrSession);
}

// 変換中の文字列を確定する。
// 通常時: compositionへ反映してアプリに確定。辞書登録中: アプリ側には何も確定せず、登録用バッファへ取り込む
void CTextService::_CommitHenkan(ITfContext* pic)
{
    HRESULT hrSession = S_OK;
    if (m_stateStack.empty()) {
        if (__HasComposition()) {
            __RequestEditSession(pic, EditSessionRequest::DoComposition, hrSession);
            hrSession = S_OK;
            __RequestEditSession(pic, EditSessionRequest::EndCompositionCommit, hrSession);
        }
        else {
            __EndHenkanMode(pic);
        }
        return;
    }

    m_regText += _compositionBuffer;
    _compositionBuffer.clear();
    __EndHenkanMode(pic);
    __RequestEditSession(pic, EditSessionRequest::DoComposition, hrSession);
}

// フォーカス喪失など、状態を引き継げない場面で全ての変換/登録状態を捨てる
void CTextService::_ResetAllState()
{
    m_stateStack.clear();
    m_regText.clear();
    _compositionBuffer.clear();
    m_henkanGokan.clear();
    __EndHenkanMode(nullptr);
    g_currentMode = (g_currentMode == SKKMode::Hankaku) ? SKKMode::Hankaku : SKKMode::Kakutei;
}

void CTextService::_OnDictionaryRegistration(const std::wstring & reading, const std::wstring & word)
{
	// TODO: 辞書登録処理
}

// キャレットのスクリーン座標を取得する。取得出来ない場合はマウスカーソル付近をフェイルセーフとして返す
bool CTextService::_GetCaretRect(TfEditCookie ec, ITfContext * pContext, RECT * pRect)
{
	if (!pContext || !pRect) {
		return false;
	}

	if (_pComposition) {
		CComPtr<ITfContextView> pContextView;
		CComPtr<ITfRange> pRange;
		CComPtr<ITfRange> pRangeEnd;
		BOOL fClipped = FALSE;
		// 末尾位置(Collapse済み)のRangeの矩形を取る
		if (SUCCEEDED(pContext->GetActiveView(&pContextView)) && pContextView &&
			SUCCEEDED(_pComposition->GetRange(&pRange)) && pRange &&
			SUCCEEDED(pRange->Clone(&pRangeEnd)) && pRangeEnd &&
			SUCCEEDED(pRangeEnd->Collapse(ec, TF_ANCHOR_END)) &&
			SUCCEEDED(pContextView->GetTextExt(ec, pRangeEnd, pRect, &fClipped))) {
			return true;
		}
	}

    // GetTextExt非対応のアプリ用
    POINT pt;
    if (GetCursorPos(&pt)) {
        pRect->left = pt.x + 10;
        pRect->bottom = pt.y + 20;
        pRect->right = pRect->left;
        pRect->top = pRect->bottom;
        return true;
    }
    return false;
}

CTextService::CTextService() {
    initialize_setting();

    _refCount = 1;
    _pThreadMgr = nullptr;
    _clientId = TF_CLIENTID_NULL;
	m_displayAttributeInputAtom = TF_INVALID_GUIDATOM;
	m_romajiKanaTranslater = romaji_kana_translater();
	m_qelfTranslater = qelf_translater();


    m_pLangBarItemButton.Attach(new CLangBarItemButton(this, GUID_LBI_INPUTMODE));
    
	m_qelfTranslater.load_KMP_table(g_Setting.kmp_file_path);

	g_CurrentKanaMode = KanaMode::Hiragana;
	g_currentMode = SKKMode::Hankaku;

	m_skkdictProvider = SKKDictionaryProvider();
	m_skkdictProvider.LoadDictionary(g_Setting.skk_dic_file_path, g_Setting.skk_user_dic_file_path);

	m_candidates = SKKCandidates();
	m_okuriganashiin = L'\0';
	m_okurigana = L"";
    m_candidateIndex = 0;

	m_drawmode = CandidateWindowDrawMode::Single;
    m_candidateIndex = 0;

    InterlockedIncrement(&g_cRefDll);
}

CTextService::~CTextService() { 

    m_candidateWindow.Destroy();

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
    }
    _ResetAllState();
    // 候補ウィンドウはこのDLL内のウィンドウプロシージャを持つので、DLLが外れる前に破棄する
    m_candidateWindow.Destroy();

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




// キーが押された瞬間
STDAPI CTextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (pfEaten == nullptr || pic == nullptr) {
        return E_INVALIDARG;
    }

    // EditSessionの要求結果は使わないので、呼び出しごとに使い捨てのHRESULTで要求する
    auto request = [&](EditSessionRequest req) {
        HRESULT hrSession = S_OK;
        __RequestEditSession(pic, req, hrSession);
    };

    *pfEaten = __IsKeyEaten(wParam);

    // Ctrl+J: ひらがな入力(Kakutei)へ。変換中なら先に確定する
    if (_IsCtrlKeyPressed() && wParam == 'J') {
        *pfEaten = TRUE;
        if (g_currentMode == SKKMode::Henkan) {
            _CommitHenkan(pic);
        }
        __changeCurrentMode(SKKMode::Kakutei);
        __changeCurrentKanaMode(KanaMode::Hiragana);
        m_romajiKanaTranslater.reset();
        m_qelfTranslater.reset_kmp_state();
        return S_OK;
    }

    if (!(*pfEaten)) {
        return S_OK;
    }

    // ここにいるということは、SKKMode::Hankaku ではない。
    if (wParam == 'R') {
        // 辞書登録中の半角英数への遷移は入力先が無くなるため無視する
        if (!m_stateStack.empty()) {
            return S_OK;
        }
        if (g_currentMode == SKKMode::Henkan) {
            _CommitHenkan(pic);
        }
        m_romajiKanaTranslater.reset();
        m_qelfTranslater.reset_kmp_state();
        __changeCurrentMode(SKKMode::Hankaku);
        return S_OK;
    }
    else if (wParam == 'Q') {
        KanaMode gotokanaMode = KanaMode::Katakana;
        // 変換中なら、未確定バッファ(候補表示中は読み)をカタカナにして確定
        if (g_currentMode == SKKMode::Henkan && __HasComposition()) {
            if (!m_candidates.empty()) {
                _compositionBuffer = m_henkanGokan + m_okurigana;
            }
            _ConvertToKatakana(_compositionBuffer);
            _CommitHenkan(pic);
            gotokanaMode = g_CurrentKanaMode;
        }
        m_romajiKanaTranslater.reset();
        m_qelfTranslater.reset_kmp_state();
        __changeCurrentMode(SKKMode::Kakutei);
        if (gotokanaMode == KanaMode::Katakana) {
            __changeCurrentKanaMode(gotokanaMode);
        }
        return S_OK;
    }
    else if (wParam == VK_RETURN) {
        if (!__HasComposition()) {
            *pfEaten = FALSE; // composition中でなければEnterはアプリに渡す
            return S_OK;
        }

        // 辞書登録中: 入力があればその語で登録・確定、空なら登録をキャンセルして元の変換状態へ戻る
        if (!m_stateStack.empty()) {
            if (g_currentMode == SKKMode::Henkan) {
                _CommitHenkan(pic);
                return S_OK;
            }
            _ExitDictionaryRegistration(pic, true);
            return S_OK;
        }

        if (g_currentMode == SKKMode::Henkan) {
            _CommitHenkan(pic);
        }
        else {
            request(EditSessionRequest::EndCompositionCommit);
        }
        _HideCandidateWindow();
        m_romajiKanaTranslater.reset();
        m_qelfTranslater.reset_kmp_state();
        return S_OK;
    }
    else if (wParam == VK_ESCAPE) {
        if (!__HasComposition()) {
            *pfEaten = FALSE;
            return S_OK;
        }
        // 辞書登録中のEscapeは登録だけをキャンセルして元の変換状態へ戻る
        if (!m_stateStack.empty()) {
            _ExitDictionaryRegistration(pic, false);
            return S_OK;
        }
        request(EditSessionRequest::EndCompositionCancel);
        _HideCandidateWindow();
        m_romajiKanaTranslater.reset();
        m_qelfTranslater.reset_kmp_state();
        return S_OK;
    }
    else if (wParam == VK_BACK) {
        if (!__HasComposition()) {
            *pfEaten = FALSE; // composition中でなければBackspaceはアプリに渡す
            return S_OK;
        }
        const bool registering = !m_stateStack.empty();

        if (g_currentMode == SKKMode::Henkan) {
            // 候補表示中は一旦読みに戻してから1文字削除する(候補文字列自体は削らない)
            if (!m_candidates.empty()) {
                m_candidates.clear();
                m_candidateIndex = 0;
                m_drawmode = CandidateWindowDrawMode::Single;
                m_okurigana.clear();
                _compositionBuffer = m_henkanGokan;
            }
            if (!_compositionBuffer.empty()) {
                _compositionBuffer.pop_back();
            }
            if (_compositionBuffer.empty()) {
                // 空になったらキャンセル扱い(辞書登録中は登録入力に戻るだけ)
                if (registering) {
                    __EndHenkanMode(pic);
                    request(EditSessionRequest::DoComposition);
                }
                else {
                    request(EditSessionRequest::EndCompositionCancel);
                    m_romajiKanaTranslater.reset();
                    m_qelfTranslater.reset_kmp_state();
                }
            }
            else {
                request(EditSessionRequest::DoComposition);
            }
        }
        else if (registering) {
            // 登録中の確定済み文字列から1文字削除。空でもアプリの表示は変わらないのでBackspaceは食べる
            if (!m_regText.empty()) {
                m_regText.pop_back();
            }
            request(EditSessionRequest::DoComposition);
        }
        else {
            request(EditSessionRequest::EndCompositionCancel);
            m_romajiKanaTranslater.reset();
            m_qelfTranslater.reset_kmp_state();
        }
        return S_OK;
    }
    else if (wParam == VK_SPACE) {
        if (g_currentMode == SKKMode::Henkan) {
            if (_compositionBuffer.empty()) {
                return S_OK; // 読みが無いので何もしない
            }
            if (m_candidates.empty()) {
                // 初めての変換: 未確定バッファ全体を読みとして辞書を引く
                m_candidateIndex = 0;
                m_henkanGokan = _compositionBuffer;
                m_okurigana.clear();
                m_drawmode = CandidateWindowDrawMode::Single;
                m_skkdictProvider.GetCandidates(m_henkanGokan, m_candidates);
            }
            else {
                _IncrementCandidateIndex();
            }

            // 候補が尽きたら辞書登録へ
            if (m_candidates.empty() || m_candidateIndex >= (int)m_candidates.size()) {
                _EnterDictionaryRegistration(pic);
                return S_OK;
            }

            // Singleの時だけ候補をcomposition上にも表示する(Multiは候補ウィンドウのみ)
            if (m_drawmode == CandidateWindowDrawMode::Single) {
                _SetCandidateText();
            }
            request(EditSessionRequest::DoComposition);
            return S_OK;
        }
        if (!m_stateStack.empty()) {
            // 辞書登録中(Kakutei)のスペースは登録語の一部として扱う
            m_regText += L' ';
            request(EditSessionRequest::DoComposition);
            return S_OK;
        }
    }
    else if (wParam == 'X' && g_currentMode == SKKMode::Henkan && !m_candidates.empty()) {
        // 前の候補へ戻る(MultiからSingleへの逆行を含む)
        _DecrementCandidateIndex();
        if (m_drawmode == CandidateWindowDrawMode::Single) {
            _SetCandidateText();
        }
        request(EditSessionRequest::DoComposition);
        return S_OK;
    }
    else if (g_currentMode == SKKMode::Henkan && m_drawmode == CandidateWindowDrawMode::Multi && !m_candidates.empty()) {
        // Multi表示中: A,S,D,F,J,K,L で表示中の候補を直接選択して確定する
        std::wstring keys(SKK_CHOOSE_CANDIDATES_BIGSTR);
        size_t selectedOffset = keys.find((WCHAR)wParam);

        if (selectedOffset != std::wstring::npos) {
            int targetIndex = m_candidateIndex + (int)selectedOffset;
            if (targetIndex < (int)m_candidates.size()) {
                m_candidateIndex = targetIndex;
                _SetCandidateText();
                _CommitHenkan(pic);
            }
            return S_OK;
        }
    }

    // 暗黙の確定: 候補が表示されている状態で別の文字が入力されたら、まず表示中の候補を確定する
    if (g_currentMode == SKKMode::Henkan && !m_candidates.empty()) {
        _CommitHenkan(pic);
    }

    DebugTrace((L"OnKeyDown: " + std::wstring(1, static_cast<WCHAR>(wParam))));

    // このキーが生む文字列(output)と、Q-ELF上でShiftが押されていたか(shifted)を求める
    const WCHAR ch = static_cast<WCHAR>(wParam);
    std::wstring output;
    bool shifted = false;

    if (m_romajiKanaTranslater.translate_kigou(ch, output)) {
        // 読み入力中に記号(ー以外)が来たら、読みをそのまま確定してから記号を入力する
        if (g_currentMode == SKKMode::Henkan && output != L"ー") {
            _CommitHenkan(pic);
        }
        // 記号はShiftによる変換開始の対象外
    }
    else {
        std::wstring qelfOutput;
        if (m_qelfTranslater.translate_qwerty_to_qelf(ch, qelfOutput)) {
            // Q-ELF側でEnterが割り当てられているキー(;など)は、本物のEnterを送って通常のEnter処理に任せる
            if (qelfOutput.length() == 1 && qelfOutput[0] == L'\r') {
                INPUT inputs[2] = {};
                inputs[0].type = INPUT_KEYBOARD;
                inputs[0].ki.wVk = VK_RETURN;
                inputs[1].type = INPUT_KEYBOARD;
                inputs[1].ki.wVk = VK_RETURN;
                inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(2, inputs, sizeof(INPUT));
                return S_OK;
            }
            m_romajiKanaTranslater.translate_qelf(qelfOutput, output);
            // Shiftは同時押し2打のどちらで押されていても良い。仮名が確定した時だけ意味を持つ
            shifted = !output.empty() && m_qelfTranslater.get_shift_pushed_last();
        }
    }

    // 1打目だけ(同時押しの途中)など、まだ文字が確定していない
    if (output.empty()) {
        return S_OK;
    }

    const bool registering = !m_stateStack.empty();

    if (shifted && g_currentMode == SKKMode::Kakutei) {
        // Shift+文字: 変換(読み入力)の開始。この仮名が読みの先頭になる
        _compositionBuffer += output;
        __StartHenkanMode(pic);
        return S_OK;
    }

    if (shifted && g_currentMode == SKKMode::Henkan && !_compositionBuffer.empty()) {
        // 変換中のShift+文字: ここまでを語幹、この仮名を送り仮名として即座に変換する
        _compositionBuffer += output;
        m_okurigana = output;
        m_henkanGokan = _compositionBuffer.substr(0, _compositionBuffer.length() - output.length());
        m_candidateIndex = 0;
        m_drawmode = CandidateWindowDrawMode::Single;

        // 辞書キーは「語幹 + 送り仮名の子音」(例: おもw)
        std::wstring dictKey = m_henkanGokan;
        WCHAR shiin = m_romajiKanaTranslater.get_last_translate_last_shiin();
        if (shiin != L'\0') {
            dictKey += shiin;
        }
        DebugTrace(L"送り仮名変換開始:" + dictKey);
        m_skkdictProvider.GetCandidates(dictKey, m_candidates);

        if (m_candidates.empty()) {
            _EnterDictionaryRegistration(pic);
            return S_OK;
        }
        _SetCandidateText();
        request(EditSessionRequest::DoComposition);
        return S_OK;
    }

    if (g_currentMode == SKKMode::Kakutei) {
        if (registering) {
            // 辞書登録中の通常入力はアプリへ挿入せず、登録用バッファに溜めて候補ウィンドウに表示する
            m_regText += output;
            request(EditSessionRequest::DoComposition);
        }
        else {
            // 通常の確定入力: そのままアプリに挿入する
            _compositionBuffer += output;
            request(EditSessionRequest::InsertText);
            _compositionBuffer.clear();
        }
        return S_OK;
    }

    // Henkanモード(読みの入力中)
    _compositionBuffer += output;
    request(__HasComposition() ? EditSessionRequest::DoComposition : EditSessionRequest::StartComposition);
    return S_OK;
}

// このキーをどのように処理するか呼び出される(キーが離された瞬間)
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
    if (pfEaten == nullptr) {//
        return E_INVALIDARG;
    }
    *pfEaten = FALSE;
    return S_OK;
}

STDAPI CTextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) {
    // フォーカス移動などでTSF側から強制終了された場合。
    // 既に終了済みのためEndCompositionを呼んではならず、ローカル状態のクリアのみ行う。
    _pComposition.Release();
    // 辞書登録中・変換中の状態も引き継げないので全て破棄する
    _ResetAllState();
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
    // 既にcompositionがある場合(辞書登録中など)は二重に開始せず表示更新のみ行う
    if (__HasComposition()) {
        return _DoComposition(ec, pContext);
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

	std::wstring displayText = _compositionBuffer;
    if(!m_stateStack.empty()) {
		displayText = m_stateStack[0].compositionBuffer;
    }

    // 辞書登録中のアプリ側表示は、登録を始めた時点の文字列で固定する(入力中の登録語は候補ウィンドウに出す)
    if (FAILED(pRange->SetText(ec, 0, displayText.c_str(),
        (LONG)displayText.length()))) {
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
            // 下線の付与に失敗しても入力自体は続行する
            pProperty->SetValue(ec, pRange, &var);
        }
    }

    // キャレットをcomposition末尾に移動する。
    pRange->Collapse(ec, TF_ANCHOR_END);
    TF_SELECTION selection;
    selection.range = pRange;
    selection.style.ase = TF_AE_NONE;
    selection.style.fInterimChar = FALSE;
    pContext->SetSelection(ec, 1, &selection);

	_UpdateCandidateWindow(ec, pContext);

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

	if (g_currentMode == SKKMode::Henkan) {
		__EndHenkanMode(pContext);
	}

    CComPtr<ITfRange> pRange;
    if (SUCCEEDED(_pComposition->GetRange(&pRange)) && pRange) {
        if (!fCommit) {
            pRange->SetText(ec, 0, L"", 0);
        }
        else if (pContext != nullptr) {
            // 確定後の文字に未確定の下線が残らないよう表示属性を外す
            CComPtr<ITfProperty> pProperty;
            if (SUCCEEDED(pContext->GetProperty(GUID_PROP_ATTRIBUTE, &pProperty)) && pProperty) {
                pProperty->Clear(ec, pRange);
            }
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
