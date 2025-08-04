#include "window_manager.h"
#include "developer_console.h"

WindowManager::WindowManager() : m_targetWindow(nullptr), m_windowTitle(nullptr) { }

bool WindowManager::FindTargetWindow(const char *windowTitle)
{
	m_windowTitle  = windowTitle;
	m_targetWindow = FindWindowA("SDL_App", windowTitle);

	if (!m_targetWindow)
	{
		return false;
	}

	DEV_LOG_INFO("Found target window: " + std::string(windowTitle));
	return true;
}

bool WindowManager::GetWindowBounds(int &x, int &y, int &width, int &height) const
{
	if (!IsWindowValid())
		return false;

	RECT clientRect;
	if (!GetClientRect(m_targetWindow, &clientRect))
	{
		DEV_LOG_ERROR("Failed to get client rectangle");
		return false;
	}

	POINT topLeft = {0, 0};
	if (!ClientToScreen(m_targetWindow, &topLeft))
	{
		DEV_LOG_ERROR("Failed to convert client coordinates to screen coordinates");
		return false;
	}

	x      = topLeft.x;
	y      = topLeft.y;
	width  = clientRect.right - clientRect.left;
	height = clientRect.bottom - clientRect.top;

	return true;
}

bool WindowManager::IsWindowValid() const
{
	return m_targetWindow != nullptr;
}
