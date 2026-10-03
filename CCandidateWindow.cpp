#include "pch.h"
#include "CCandidateWindow.h"

CCandidateWindow::CCandidateWindow()
{
	m_hwnd = NULL;
	m_candidates = SKKCandidates();
	m_currentIndex = 0;
	m_drawmode = CandidateWindowDrawMode::Single;
	m_isRegisterMode = false;
	m_registerInput = L"";
	m_registerTarget = L"";
}

CCandidateWindow::~CCandidateWindow()
{
	Destroy();
}

bool CCandidateWindow::Create(HWND hparent,HINSTANCE hIns)
{
	constexpr LPCWSTR className = L"SKK_CandidateWindownewtettySKK";

	WNDCLASSEXW winc{sizeof(WNDCLASSEXW)};
	winc.style = CS_HREDRAW | CS_VREDRAW;
	winc.lpfnWndProc = CCandidateWindow::WndProc;
	winc.cbWndExtra = sizeof(CCandidateWindow*);
	winc.hInstance = hIns;
	winc.hCursor = LoadCursor(NULL, IDC_ARROW);
	winc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	winc.lpszClassName = className;

	RegisterClassEx(&winc);

	m_hwnd = CreateWindowEx(
		WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
		className, L"",
		WS_POPUP | WS_BORDER,
		0, 0, WINDOW_WIDTH, WINDOW_HEIGHT,
		hparent, NULL, winc.hInstance, this
	);

	if(m_hwnd) {
		SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
	}
	return m_hwnd != NULL;
}

void CCandidateWindow::Destroy()
{
	if (m_hwnd) {
		DestroyWindow(m_hwnd);
		m_hwnd = NULL;
	}
}

void CCandidateWindow::Show(int x, int y)
{
	if (m_hwnd) {
		SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, WINDOW_WIDTH, WINDOW_HEIGHT, SWP_NOACTIVATE | SWP_SHOWWINDOW);
		InvalidateRect(m_hwnd, NULL, TRUE);
	}
}

void CCandidateWindow::Hide()
{
	if (m_hwnd) {
		ShowWindow(m_hwnd, SW_HIDE);
	}
}

void CCandidateWindow::SetCandidates(const SKKCandidates & candidas)
{
	m_candidates = candidas;
}

// m_currentIndexから，multiで6個，singleで1個の候補を描画する。
void CCandidateWindow::SetShowIndex(int candidates_index, CandidateWindowDrawMode drawmode)
{
	m_drawmode = drawmode;
	m_currentIndex = candidates_index;
	if (m_hwnd) {
		InvalidateRect(m_hwnd, NULL, TRUE);
	}
}

void CCandidateWindow::SetRegisterInfo(bool isRegister, const std::wstring& target, const std::wstring& input)
{
	m_isRegisterMode = isRegister;
	m_registerTarget = target;
	m_registerInput = input;
	if (m_hwnd) {
		InvalidateRect(m_hwnd, NULL, TRUE);
	}
}

LRESULT CCandidateWindow::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CCandidateWindow* pThis = (CCandidateWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
	if (uMsg == WM_PAINT && pThis) {
		pThis->OnPaint(hwnd);
		return 0;
	}
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void CCandidateWindow::OnPaint(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	SetBkMode(hdc, TRANSPARENT);
	// 既定のDCフォントは日本語表示に不向きなためGUIフォントを使う(ストックオブジェクトなので解放不要)
	HGDIOBJ oldFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
	SetTextColor(hdc, COLOR_NORMAL);

	int yOffset = MARGIN_Y;

	if (m_isRegisterMode) {
		std::wstring text = L"[Reg] " + m_registerTarget + L" : " + m_registerInput;
		TextOutW(hdc, MARGIN_X, yOffset, text.c_str(), (int)text.length());
		yOffset += LINE_HEIGHT;
	}

	const int candCount = (int)m_candidates.size();
	if (m_drawmode == CandidateWindowDrawMode::Single && m_currentIndex >= 0 && m_currentIndex < candCount) {
		SetTextColor(hdc, COLOR_NORMAL);
		std::wstring candtext = m_candidates[m_currentIndex].candidate;
		TextOut(hdc,MARGIN_X, yOffset, candtext.c_str(), candtext.length());
		if (!m_candidates[m_currentIndex].annotation.empty()) {
			SIZE size;
			GetTextExtentPoint32W(hdc, candtext.c_str(), (int)candtext.length(), &size);
			SetTextColor(hdc, COLOR_DIM);
			std::wstring annotext = L" ;" + m_candidates[m_currentIndex].annotation ;
			TextOut(hdc, MARGIN_X + size.cx, yOffset, annotext.c_str(), annotext.length());
		}
	}
	else if(m_drawmode==CandidateWindowDrawMode::Multi) {
		int startIndex = m_currentIndex;
		for (int i = 0; i < SKK_CHOOSE_CANDIDATES_BIGSTR_LEN && (startIndex + i) < candCount; i++) {
			std::wstring displayText =std::wstring(1, SKK_CHOOSE_CANDIDATES_BIGSTR[i] )+ L". " + m_candidates[startIndex + i].candidate;
			TextOut(hdc, 5, yOffset, displayText.c_str(), displayText.length());
			
			if (!m_candidates[startIndex + i].annotation.empty()) {
				SIZE size;
				GetTextExtentPoint32W(hdc, displayText.c_str(), (int)displayText.length(), &size);
				SetTextColor(hdc, COLOR_DIM);
				std::wstring annotext = L" ;" + m_candidates[startIndex + i].annotation;
				TextOut(hdc, 5 + size.cx, yOffset, annotext.c_str(), annotext.length());
				SetTextColor(hdc, COLOR_NORMAL);
			}
			yOffset += LINE_HEIGHT;
		}
	}
	SelectObject(hdc, oldFont);
	EndPaint(hwnd, &ps);
}
