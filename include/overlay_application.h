#pragma once

#include "overlay_renderer.h"
#include "rlFPSCamera.h"
#include <atomic>
#include <memory>

#include "pipe_client.h"
#include "window_manager.h"

class OverlayApplication
{
public:
	OverlayApplication();
	~OverlayApplication();

	int Run();

private:
	bool Initialize();
	void Shutdown();
	void MainLoop();

	std::unique_ptr<OverlayRenderer> m_renderer;
	std::unique_ptr<PipeClient>      m_pipeClient;
	std::unique_ptr<WindowManager>   m_windowManager;

	rlFPCamera        m_camera;
	std::atomic<bool> m_running;

	static constexpr auto TARGET_WINDOW_TITLE  = "Counter-Strike 2";
	static constexpr auto OVERLAY_WINDOW_TITLE = "DebugOverlay";
};
