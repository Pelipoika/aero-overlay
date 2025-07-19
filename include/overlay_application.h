#pragma once

#include <atomic>
#include <memory>

#include "overlay_renderer.h"
#include "SharedMemoryClient.h"
#include "window_manager.h"

class OverlayApplication
{
public:
	OverlayApplication();
	~OverlayApplication();

	OverlayApplication(const OverlayApplication &other)                = delete;
	OverlayApplication(OverlayApplication &&other) noexcept            = delete;
	OverlayApplication &operator=(const OverlayApplication &other)     = delete;
	OverlayApplication &operator=(OverlayApplication &&other) noexcept = delete;

	int Run();

private:
	bool Initialize();
	void Shutdown();
	void MainLoop();

	std::unique_ptr<OverlayRenderer>    m_renderer;
	std::unique_ptr<SharedMemoryClient> m_memoryClient;
	std::unique_ptr<WindowManager>      m_windowManager;

	rlFPCamera        m_camera;
	std::atomic<bool> m_running;
};
