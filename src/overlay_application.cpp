#include "overlay_application.h"
#include "config.h"
#include <cstdio>
#include <print>

OverlayApplication::OverlayApplication() : m_camera(), m_running(false) { }

OverlayApplication::~OverlayApplication()
{
	Shutdown();
}

int OverlayApplication::Run()
{
	if (!Initialize())
	{
		std::println(stderr, "Failed to initialize overlay application");
		return -1;
	}

	MainLoop();
	Shutdown();
	return 0;
}

bool OverlayApplication::Initialize()
{
	// Initialize window manager and find target window
	m_windowManager = std::make_unique<WindowManager>();
	if (!m_windowManager->FindTargetWindow(Config::TARGET_WINDOW_TITLE))
	{
		return false;
	}

	// Get target window bounds
	int x, y, width, height;
	if (!m_windowManager->GetWindowBounds(x, y, width, height))
	{
		return false;
	}

	// Initialize renderer
	m_renderer = std::make_unique<OverlayRenderer>();
	if (!m_renderer->Initialize(width, height, x, y))
	{
		return false;
	}

	// Initialize camera
	rlFPCameraInit(&m_camera, Config::DEFAULT_FOV, {0, 0, 0});

	// Initialize shared memory client
	m_memoryClient = std::make_unique<SharedMemoryClient>();
	m_running      = true;

	if (!m_memoryClient->Start(m_running, m_camera))
	{
		return false;
	}

	std::println("Overlay application initialized successfully");
	return true;
}

void OverlayApplication::Shutdown()
{
	m_running = false;

	if (m_memoryClient)
	{
		m_memoryClient->Stop();
		m_memoryClient.reset();
	}

	if (m_renderer)
	{
		m_renderer->Shutdown();
		m_renderer.reset();
	}

	m_windowManager.reset();
}

void OverlayApplication::MainLoop()
{
	while (!OverlayRenderer::ShouldClose() && m_running)
	{
		// Update camera
		rlFPCameraUpdate(&m_camera);

		// Get draw commands from shared memory client
		std::vector<DrawCommandPacket> drawCommands;

		if (m_memoryClient)
		{
			drawCommands = m_memoryClient->GetDrawCommands();
		}

		// Render frame
		m_renderer->BeginFrame();
		m_renderer->RenderCommands(drawCommands, m_camera);
		m_renderer->EndFrame();
	}
}
