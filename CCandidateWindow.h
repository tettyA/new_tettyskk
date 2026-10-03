#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>
#include <string>
#include "settings_value.h"
#include "SKKDictionaryProvider.h"

enum class CandidateWindowDrawMode {
	Single,
	Multi
};

struct ColorfulText {
	std::wstring text;
	COLORREF color;
};

class CCandidateWindow
{
public:
	CCandidateWindow();
	~CCandidateWindow();

	bool Create(HWND hparent,HINSTANCE hIns);
	void Destroy();
	void Show(int x, int y);
	void Hide();
	void SetCandidates(const SKKCandidates& candidas);
	void SetShowIndex(int candidates_index, CandidateWindowDrawMode drawmode);

	void SetRegisterInfo(bool isRegister, const std::wstring& target, const std::wstring& input);


	inline HWND GetHwnd()const { return m_hwnd; }

private:
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	void OnPaint(HWND hwnd);
	void _RenderContent(HDC hdc, int availableWidth, bool draw, int& outWidth, int& outHeight);
	int _DrawColorfulText(HDC hdc, int beginx, int beginy, int beginidx, int endidx, const std::vector<ColorfulText>& texts);
	int _GetTextSize(HDC hdc,int beginidx,int endidx, const std::vector<ColorfulText>& texts);
	HWND m_hwnd;
	SKKCandidates m_candidates;
	CandidateWindowDrawMode m_drawmode;
	int m_currentIndex;

	bool m_isRegisterMode;
	std::wstring m_registerTarget;
	std::wstring m_registerInput;


	HFONT m_hFont;
	int m_desktop_dpi_x;
	int window_width = 250;
	int window_height = 200;
	static constexpr int MARGIN_X = 5;
	static constexpr int MARGIN_Y = 5;
	int LINE_HEIGHT = 20;
};

