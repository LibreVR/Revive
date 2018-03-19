#include "Win32Window.h"

#include <Windows.h>
#include <thread>

extern HMODULE revModule;

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

Win32Window::Win32Window()
{
	WNDCLASSW wc;
	memset(&wc, 0, sizeof(wc));
	wc.hInstance = revModule;
	wc.lpszClassName = L"Remixed";
	wc.lpfnWndProc = WindowProc;
	RegisterClassW(&wc);

	m_hWnd = CreateWindowW(wc.lpszClassName, L"Remixed", 0, 0, 0, 0, 0, 0, 0, revModule, 0);
}

Win32Window::~Win32Window()
{
	DestroyWindow(m_hWnd);
	UnregisterClassW(L"Remixed", revModule);
}

void Win32Window::Show(bool show)
{
	ShowWindow(m_hWnd, show ? SW_SHOWNORMAL : SW_HIDE);
	UpdateWindow(m_hWnd);
}
