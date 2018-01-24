#pragma once

#include <thread>

// FWD-decl
typedef struct HWND__ *HWND;

class Win32Window
{
public:
	Win32Window();
	~Win32Window();

	HWND GetWindowHandle() { return m_hWnd; }
	void Show(bool show = true);

private:
	HWND m_hWnd;
};
