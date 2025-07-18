#include "window_manager.h"
#include <iostream>

WindowManager::WindowManager() : m_targetWindow(nullptr), m_windowTitle(nullptr) { }

bool WindowManager::FindTargetWindow(const char *windowTitle)
{
	m_windowTitle  = windowTitle;
	m_targetWindow = FindWindowA("SDL_App", windowTitle);

	if (!m_targetWindow)
	{
		std::cerr << "Could not find window: " << windowTitle << '\n';
		return false;
	}

	return true;
}

bool WindowManager::GetWindowBounds(int &x, int &y, int &width, int &height) const
{
	if (!IsWindowValid())
		return false;

	RECT rect;
	if (!GetWindowRect(m_targetWindow, &rect))
	{
		std::cerr << "Failed to get window rectangle" << '\n';
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
