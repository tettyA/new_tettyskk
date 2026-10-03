#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vector>
#include <string>
#include "SKKDictionaryProvider.h"

enum class CandidateWindowDrawMode {
	Single,
	Multi
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

	HWND m_hwnd;
	SKKCandidates m_candidates;
	CandidateWindowDrawMode m_drawmode;
	int m_currentIndex;

	bool m_isRegisterMode;
	std::wstring m_registerTarget;
	std::wstring m_registerInput;

	static constexpr int WINDOW_WIDTH = 250;
	static constexpr int WINDOW_HEIGHT = 200;
	static constexpr int MARGIN_X = 5;
	static constexpr int MARGIN_Y = 5;
	static constexpr int LINE_HEIGHT = 20;
	static constexpr COLORREF COLOR_NORMAL = RGB(0, 0, 0);
	static constexpr COLORREF COLOR_DIM = RGB(128, 128, 128);
};

