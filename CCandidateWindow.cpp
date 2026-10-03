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

	m_desktop_dpi_x = GetSystemMetrics(SM_CXSCREEN);
	
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
		0, 0, window_width, window_height,
		hparent, NULL, winc.hInstance, this
	);

	if(m_hwnd) {
		SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);
	}

	m_hFont =
		CreateFont(
			g_Setting.candidate_window_font_size,
			0,
			0,
			0,
			FW_REGULAR,
			FALSE,
			FALSE,
			FALSE,
			SHIFTJIS_CHARSET,
			OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS,
			PROOF_QUALITY,
			FIXED_PITCH | FF_MODERN,
			g_Setting.candidate_window_font_name
		);
	
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
		SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, window_width, window_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
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

// m_currentIndex‚©‚çCmulti‚Å6ŒÂCsingle‚Å1ŒÂ‚ÌŒó•â‚ð•`‰æ‚·‚éB
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
	SelectObject(hdc, m_hFont);

	SetTextColor(hdc, g_Setting.candidate_window_font_color);
	{
		std::wstring testStr = L"‚ ";
		SIZE size;
		GetTextExtentPoint32W(hdc, testStr.c_str(), (int)testStr.length(), &size);
		LINE_HEIGHT = size.cy;
	}
	int yOffset = MARGIN_Y;
	int xMaxOffset = MARGIN_X;

	if (m_isRegisterMode) {

		xMaxOffset= _DrawColorfulText(hdc, MARGIN_X, yOffset, 0, 4, {
			{L"[“o˜^] ", g_Setting.candidate_window_asdfjkl_font_color},
			{m_registerTarget, g_Setting.candidate_window_font_color},
			{L" : ", g_Setting.candidate_window_dim_font_color},
			{m_registerInput, g_Setting.candidate_window_font_color}
			});
		yOffset += LINE_HEIGHT;
	}

	const int candCount = (int)m_candidates.size();
	if (m_drawmode == CandidateWindowDrawMode::Single && m_currentIndex >= 0 && m_currentIndex < candCount) {
		xMaxOffset = max(xMaxOffset, _DrawColorfulText(hdc, MARGIN_X, yOffset, 0, 3,
			{
		{L"[" + std::to_wstring(m_currentIndex + 1) + L" of " + std::to_wstring(candCount) + L"] ", g_Setting.candidate_window_dim_font_color},
		{ m_candidates[m_currentIndex].candidate,g_Setting.candidate_window_font_color},
		{
		(m_candidates[m_currentIndex].annotation.empty() ? L"" :
			L" ;" + m_candidates[m_currentIndex].annotation), g_Setting.candidate_window_dim_font_color
		}
			}));
		yOffset += LINE_HEIGHT;

		window_width = xMaxOffset + MARGIN_X;
		window_height = yOffset + MARGIN_Y;
	}
	else if (m_drawmode == CandidateWindowDrawMode::Multi) {
		std::vector<ColorfulText> texts;
		int nowWidth = MARGIN_X;
		int startIndex = m_currentIndex;
		int starttextsIdx = 0;
		int endtextsIdx = 0;
		for (int i = 0; i < SKK_CHOOSE_CANDIDATES_BIGSTR_LEN && (startIndex + i) < candCount; i++) {
			texts.push_back({ L"[" + std::wstring(1,SKK_CHOOSE_CANDIDATES_BIGSTR[i]) + L"] ", g_Setting.candidate_window_dim_font_color });
			texts.push_back({ m_candidates[startIndex + i].candidate,g_Setting.candidate_window_asdfjkl_font_color });
			if (!m_candidates[startIndex + i].annotation.empty()) {
				texts.push_back({ L";" + m_candidates[startIndex + i].annotation,g_Setting.candidate_window_dim_font_color });
			}

			nowWidth += _GetTextSize(hdc, starttextsIdx, (int)texts.size(), texts);
			if (nowWidth + MARGIN_X > window_width) {
				xMaxOffset = max(xMaxOffset, _DrawColorfulText(hdc, MARGIN_X, yOffset, starttextsIdx, endtextsIdx, texts));
				yOffset += LINE_HEIGHT;
				starttextsIdx = endtextsIdx;
				nowWidth = MARGIN_X;
			}
			else {
				endtextsIdx = (int)texts.size();
			}
		}

		texts.push_back({ L"Žc:" + std::to_wstring(max(0,candCount - (startIndex + SKK_CHOOSE_CANDIDATES_BIGSTR_LEN))), g_Setting.candidate_window_hilight_font_color });
		xMaxOffset = max(xMaxOffset, _DrawColorfulText(hdc, MARGIN_X, yOffset, endtextsIdx, texts.size(), texts));
		yOffset += LINE_HEIGHT;

		window_width = xMaxOffset + MARGIN_X;
		window_height = yOffset + MARGIN_Y;
	}
	EndPaint(hwnd, &ps);

}

int CCandidateWindow::_DrawColorfulText(HDC hdc, int beginx, int beginy,int beginidx,int endidx, const std::vector<ColorfulText>& texts)
{
	endidx = min(endidx, (int)texts.size());
	for (int i = beginidx; i < endidx; i++) {
		if (texts[i].text.empty()) continue;
		SetTextColor(hdc, texts[i].color);
		TextOutW(hdc, beginx, beginy, texts[i].text.c_str(), (int)texts[i].text.length());
		SIZE size;
		GetTextExtentPoint32W(hdc, texts[i].text.c_str(), (int)texts[i].text.length(), &size);
		beginx += size.cx;
	}
	return  beginx;
}

int CCandidateWindow::_GetTextSize(HDC hdc, int beginidx, int endidx, const std::vector<ColorfulText>& texts)
{
	int beginx = 0;
	int beginy = 0;
	endidx = min(endidx, (int)texts.size());
	for (int i = beginidx; i < endidx; i++) {
		SIZE size;
		GetTextExtentPoint32W(hdc, texts[i].text.c_str(), (int)texts[i].text.length(), &size);
		beginx += size.cx;
	}
	return  beginx;
}
