#include "pch.h"
#include "globals.h"
#include "ClangBarItemButton.h"
#include "text_service.h"

#include "resource3.h"

CLangBarItemButton::CLangBarItemButton(CTextService* cskkime, REFGUID guid)
{
	_refCount = 1;
	
	_LangBarItemInfo.clsidService = CLSID_new_tettyskk;
	_LangBarItemInfo.guidItem = guid;
	_LangBarItemInfo.dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
	_LangBarItemInfo.ulSort = 1;
	wcscpy_s(_LangBarItemInfo.szDescription, gtettySKK_IME_NAME);

	_pSkkIme = cskkime;
	if (_pSkkIme) {
		//_pSkkIme->AddRef();
	}
	_pLangBarItemSink = nullptr;
}

CLangBarItemButton::~CLangBarItemButton()
{
	_Uninit();
	//_pSkkIme->Release();
	_pSkkIme = nullptr;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::QueryInterface(REFIID riid, void** ppvObject)
{
	if (ppvObject == nullptr)
	{
		return E_POINTER;
	}
	if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) || IsEqualIID(riid, IID_ITfLangBarItemButton))
	{
		*ppvObject = static_cast<ITfLangBarItemButton*>(this);
	}
	else if (IsEqualIID(riid, IID_ITfSource))
	{
		*ppvObject = static_cast<ITfSource*>(this);
	}
	else
	{
		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}

	if (ppvObject) {
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG __stdcall) CLangBarItemButton::AddRef()
{
	return InterlockedIncrement(&_refCount);
}

STDMETHODIMP_(ULONG __stdcall) CLangBarItemButton::Release()
{
	LONG refCount = InterlockedDecrement(&_refCount);
	if (refCount == 0)
	{
		delete this;
		return 0;
	}
	return refCount;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::GetInfo(TF_LANGBARITEMINFO* pInfo)
{
	if (pInfo == nullptr)
	{
		return E_INVALIDARG;
	}
	*pInfo = _LangBarItemInfo;
	return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::GetStatus(DWORD* pdwStatus)
{
	if (pdwStatus == nullptr)
	{
		return E_INVALIDARG;
	}

	//TODO: _IsKeyBoardDisabled()的な関数を作成して、キーボードが無効化されている場合はTF_LBI_STATUS_DISABLEDを返すようにする
	*pdwStatus = 0;
	return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::Show(BOOL fShow)
{
	//表示・非表示の切り替え要求
    //特に何もしない
	return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::GetTooltipString(BSTR* pbstrToolTip)
{
	if (pbstrToolTip == nullptr)
	{
		return E_INVALIDARG;
	}
	*pbstrToolTip = SysAllocString(gtettySKK_IME_NAME);
	return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::OnClick(TfLBIClick click, POINT pt, const RECT* prcArea)
{
	// TODO: クリック時の処理を実装する
	return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::InitMenu(ITfMenu* pMenu)
{
	// TODO: メニューの初期化処理を実装するs
	return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::OnMenuSelect(UINT wID)
{
	// TODO: メニュー選択時の処理を実装する
	return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::GetIcon(HICON* phIcon)
{
	if (phIcon == nullptr)
	{
		return E_INVALIDARG;
	}
	if (g_hModule == NULL) {
		*phIcon = LoadIcon(0, IDI_EXCLAMATION);
		return S_OK;
	}

	SKKMode mode = g_currentMode;
	KanaMode kanaMode = g_CurrentKanaMode;
	int iconnum =IDI_MODEICON_L16;

	if (mode == SKKMode::Hankaku) {
		iconnum = IDI_MODEICON_L16;
	}
	else {
		if (kanaMode == KanaMode::Hiragana) {
			iconnum = IDI_MODEICON_J16;
		}
		else if (kanaMode == KanaMode::Katakana) {
			iconnum = IDI_MODEICON_Q16;
		}
	}
	*phIcon = (HICON)LoadImage(g_hModule, MAKEINTRESOURCE(iconnum), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

	return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::GetText(BSTR* pbstrText)
{
	if (pbstrText == nullptr)
	{
		return E_INVALIDARG;
	}
	*pbstrText = SysAllocString(gtettySKK_IME_NAME);
	return S_OK;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie)
{
	if (punk == nullptr || pdwCookie == nullptr)
	{
		return E_INVALIDARG;
	}

	if (!IsEqualIID(riid, IID_ITfLangBarItemSink))
	{
		return CONNECT_E_CANNOTCONNECT;
	}

	if (_pLangBarItemSink != nullptr)
	{
		return CONNECT_E_ADVISELIMIT;
	}

	if (SUCCEEDED(punk->QueryInterface(IID_PPV_ARGS(&_pLangBarItemSink))) && _pLangBarItemSink != nullptr)
	{
		*pdwCookie = SKKIME_CLANGBAR_SINK_COOKIE;
		return S_OK;
	}

	_pLangBarItemSink.Release();
	return E_NOINTERFACE;
}

STDMETHODIMP_(HRESULT __stdcall) CLangBarItemButton::UnadviseSink(DWORD dwCookie)
{
	if (dwCookie != SKKIME_CLANGBAR_SINK_COOKIE)
	{
		return CONNECT_E_NOCONNECTION;
	}
	if (_pLangBarItemSink == nullptr)
	{
		return CONNECT_E_NOCONNECTION;
	}
	_pLangBarItemSink.Release();
	return S_OK;
}

HRESULT CLangBarItemButton::_UpdateIcon()
{
	if (_pLangBarItemSink)
	{
		return _pLangBarItemSink->OnUpdate(TF_LBI_ICON | TF_LBI_STATUS);
	}
	return S_OK;
}

HRESULT CLangBarItemButton::_Init()
{
	ITfLangBarItemMgr* pLangBarItemMgr = nullptr;
	ITfThreadMgr* pThreadMgr = nullptr;

	if (_pSkkIme == nullptr) {
		return E_FAIL;
	}
	_pSkkIme->__GetThreadMgr(&pThreadMgr);
	if (pThreadMgr == nullptr) {
		return E_FAIL;
	}
	if (SUCCEEDED(pThreadMgr->QueryInterface(IID_PPV_ARGS(&pLangBarItemMgr))) && pLangBarItemMgr != nullptr) {
		//自分自身を登録
		if (FAILED(pLangBarItemMgr->AddItem(this))) {
			pLangBarItemMgr->Release();
			pThreadMgr->Release();
			pThreadMgr = nullptr;
			return E_FAIL;
		}
		pLangBarItemMgr->Release();
		pThreadMgr->Release();
		pThreadMgr = nullptr;
		return S_OK;
	}
	pThreadMgr->Release();
	pThreadMgr = nullptr;
	return E_FAIL;
}

HRESULT CLangBarItemButton::_Uninit()
{
	ITfThreadMgr* pThreadMgr = nullptr;
	if (_pSkkIme == nullptr) {
		return S_OK;
	}
	// Deactivate済みならpThreadMgrはnullptrのままになる(デストラクタからも呼ばれるため)
	_pSkkIme->__GetThreadMgr(&pThreadMgr);

	if (pThreadMgr) {
		ITfLangBarItemMgr* pLangBarItemMgr = nullptr;
		if (SUCCEEDED(pThreadMgr->QueryInterface(IID_PPV_ARGS(&pLangBarItemMgr))) && pLangBarItemMgr != nullptr) {
			pLangBarItemMgr->RemoveItem(this);
			pLangBarItemMgr->Release();
		}
		pThreadMgr->Release();
		pThreadMgr = nullptr;
	}

	return S_OK;
}
