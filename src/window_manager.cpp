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

	RECT rect;
	if (!GetWindowRect(m_targetWindow, &rect))
	{
		DEV_LOG_ERROR("Failed to get window rectangle");
		return false;
	}

	x      = rect.left;
	y      = rect.top;
	width  = rect.right - rect.left;
	height = rect.bottom - rect.top;

	return true;
}

bool WindowManager::IsWindowValid() const
{
	return m_targetWindow != nullptr;
}
