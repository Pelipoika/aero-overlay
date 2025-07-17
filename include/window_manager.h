#pragma once

#include "win32_minimal.h"

class WindowManager
{
public:
	WindowManager();
	~WindowManager() = default;

	bool FindTargetWindow(const char *windowTitle);
	bool GetWindowBounds(int &x, int &y, int &width, int &height) const;
	bool IsWindowValid() const;

private:
	HWND        m_targetWindow;
	const char *m_windowTitle;
};
