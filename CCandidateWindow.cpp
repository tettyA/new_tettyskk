#include "pch.h"
#include "CCandidateWindow.h"

CCandidateWindow::CCandidateWindow()
{
	m_hwnd = NULL;
	m_candidates = SKKCandidates();
	m_currentIndex = 0;
	m_drawmode = CandidateWindowDrawMode::Single;
	m_isRegisterMode = false;
	m_registerCommittedInput = L"";
	m_registerUnconfirmedInput = L"";
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
		HDC hdc = GetDC(m_hwnd);
		if (hdc) {
			SelectObject(hdc, m_hFont);
			int measuredWidth = window_width;
			int measuredHeight = window_height;
			int availableWidth = window_width;
			RECT workArea{ 0 };
			{
				POINT pt{ x, y };
				HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
				MONITORINFO mi{ sizeof(MONITORINFO) };
				if (GetMonitorInfoW(hMonitor, &mi)) {
					availableWidth = max(1, mi.rcWork.right - x);
					workArea = mi.rcWork;
				}
			}
			_RenderContent(hdc, availableWidth, false, measuredWidth, measuredHeight);
			ReleaseDC(m_hwnd, hdc);
			window_width = measuredWidth;
			window_height = measuredHeight;
			if (workArea.right > workArea.left) {
				const int maxX = workArea.right - window_width;
				if (x > maxX) {
					x = max(workArea.left, maxX);
				}
			}
			SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, window_width, window_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);
			InvalidateRect(m_hwnd, NULL, TRUE);
		}
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

void CCandidateWindow::SetRegisterInfo(bool isRegister, const std::wstring& target, const std::wstring& committedInput, const std::wstring& unconfirmedInput)
{
	m_isRegisterMode = isRegister;
	m_registerTarget = target;
	m_registerCommittedInput = committedInput;
	m_registerUnconfirmedInput = unconfirmedInput;
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

	RECT rc{};
	GetWindowRect(hwnd, &rc);
	int availableWidth = window_width;
	{
		HMONITOR hMonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi{ sizeof(MONITORINFO) };
		if (GetMonitorInfoW(hMonitor, &mi)) {
			availableWidth = max(1, mi.rcWork.right - rc.left);
		}
	}
	int renderedWidth = window_width;
	int renderedHeight = window_height;
	_RenderContent(hdc, availableWidth, true, renderedWidth, renderedHeight);
	window_width = renderedWidth;
	window_height = renderedHeight;

	EndPaint(hwnd, &ps);
}

void CCandidateWindow::_RenderContent(HDC hdc, int availableWidth, bool draw, int& outWidth, int& outHeight)
{
	SetTextColor(hdc, g_Setting.candidate_window_normal_font_color);
	{
		std::wstring testStr = L"あ";
		SIZE size;
		GetTextExtentPoint32W(hdc, testStr.c_str(), (int)testStr.length(), &size);
		LINE_HEIGHT = size.cy;
	}

	const int wrapLimit = max(MARGIN_X, availableWidth - MARGIN_X);
	int yOffset = MARGIN_Y;
	int maxX = MARGIN_X;

	auto finalizeLine = [&](const std::vector<ColorfulText>& line, int lineRight) {
		if (line.empty()) {
			return;
		}
		if (draw) {
			maxX = max(maxX, _DrawColorfulText(hdc, MARGIN_X, yOffset, 0, (int)line.size(), line));
		}
		else {
			maxX = max(maxX, lineRight);
		}
		yOffset += LINE_HEIGHT;
	};
	DebugTrace((L"RenderContent: availableWidth=" + std::to_wstring(availableWidth) + L", wrapLimit=" + std::to_wstring(wrapLimit)));
	if (m_isRegisterMode) {
		std::vector<ColorfulText> texts{
			{L"[登録] ", g_Setting.candidate_window_hilight_font_color},
			{m_registerTarget, g_Setting.candidate_window_normal_font_color},
			{L" : ", g_Setting.candidate_window_dim_font_color},
			{m_registerCommittedInput, g_Setting.candidate_window_normal_font_color},
			{m_registerUnconfirmedInput, g_Setting.candidate_window_unconfirmed_font_color}
		};
		DebugTrace((L"RegisterInfo: " + m_registerTarget + L" : " + m_registerCommittedInput + L" + " + m_registerUnconfirmedInput));
		finalizeLine(texts, MARGIN_X + _GetTextSize(hdc, 0, (int)texts.size(), texts));
	}

	const int candCount = (int)m_candidates.size();
	if (m_drawmode == CandidateWindowDrawMode::Single && m_currentIndex >= 0 && m_currentIndex < candCount) {
		std::vector<ColorfulText> texts{
			{m_candidates[m_currentIndex].candidate, g_Setting.candidate_window_normal_font_color},
			{(m_candidates[m_currentIndex].annotation.empty() ? L"" : L" ;" + m_candidates[m_currentIndex].annotation), g_Setting.candidate_window_dim_font_color}
		};
		finalizeLine(texts, MARGIN_X + _GetTextSize(hdc, 0, (int)texts.size(), texts));
	}
	else if (m_drawmode == CandidateWindowDrawMode::Multi) {
		std::vector<ColorfulText> lineTexts;
		int lineRight = MARGIN_X;

		auto flushLine = [&]() {
			finalizeLine(lineTexts, lineRight);
			lineTexts.clear();
			lineRight = MARGIN_X;
		};

		auto appendSegment = [&](const std::vector<ColorfulText>& segment) {
			const int segmentWidth = _GetTextSize(hdc, 0, (int)segment.size(), segment);
			if (!lineTexts.empty() && lineRight + segmentWidth > wrapLimit) {
				flushLine();
			}
			lineTexts.insert(lineTexts.end(), segment.begin(), segment.end());
			lineRight += segmentWidth;
		};

		for (int i = 0; i < SKK_CHOOSE_CANDIDATES_BIGSTR_LEN && (m_currentIndex + i) < candCount; i++) {
			std::vector<ColorfulText> segment{
				{L"[" + std::wstring(1, SKK_CHOOSE_CANDIDATES_BIGSTR[i]) + L"] ", g_Setting.candidate_window_asdfjkl_font_color},
				{m_candidates[m_currentIndex + i].candidate, g_Setting.candidate_window_normal_font_color}
			};
			if (!m_candidates[m_currentIndex + i].annotation.empty()) {
				segment.push_back({ L";" + m_candidates[m_currentIndex + i].annotation, g_Setting.candidate_window_dim_font_color });
			}
			appendSegment(segment);
		}

		appendSegment({ {L" 残:" + std::to_wstring(max(0, candCount - (m_currentIndex + SKK_CHOOSE_CANDIDATES_BIGSTR_LEN))), g_Setting.candidate_window_hilight_font_color} });
		flushLine();
	}

	outWidth = maxX + MARGIN_X;
	outHeight = yOffset + MARGIN_Y;
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
