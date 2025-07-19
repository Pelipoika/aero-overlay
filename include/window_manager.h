#pragma once

#include "win32_minimal.h"

class WindowManager
{
public:
	WindowManager();
	~WindowManager() = default;

	WindowManager(const WindowManager &other)                = delete;
	WindowManager(WindowManager &&other) noexcept            = delete;
	WindowManager &operator=(const WindowManager &other)     = delete;
	WindowManager &operator=(WindowManager &&other) noexcept = delete;

	bool FindTargetWindow(const char *windowTitle);
	bool GetWindowBounds(int &x, int &y, int &width, int &height) const;
	bool IsWindowValid() const;

private:
	HWND        m_targetWindow;
	const char *m_windowTitle;
};
