#include "overlay_application.h"
#include "config.h"
#include "developer_console.h"
#include <cstdio>
#include <print>

OverlayApplication::OverlayApplication() : m_camera(), m_running(false) { }

OverlayApplication::~OverlayApplication()
{
	Shutdown();
}

int OverlayApplication::Run()
{
	while (!Initialize())
	{
		DEV_LOG_ERROR("Failed to initialize overlay application, retrying in 2 seconds.");
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}

	MainLoop();
	Shutdown();
	return 0;
}

bool OverlayApplication::Initialize()
{
	DEV_LOG_INFO("Initializing debug overlay...");

	// Initialize window manager and find target window
	m_windowManager = std::make_unique<WindowManager>();
	if (!m_windowManager->FindTargetWindow(Config::TARGET_WINDOW_TITLE))
	{
		DEV_LOG_ERROR("Target window not found - waiting for " + std::string(Config::TARGET_WINDOW_TITLE));
		return false;
	}

	// Get target window bounds
	int x, y, width, height;
	if (!m_windowManager->GetWindowBounds(x, y, width, height))
	{
		DEV_LOG_ERROR("Failed to get target window bounds");
		return false;
	}

	DEV_LOG_INFO("Target window found: " + std::to_string(width) + "x" + std::to_string(height) +
	             " at (" + std::to_string(x) + ", " + std::to_string(y) + ")");

	// Initialize camera
	rlFPCameraInit(&m_camera, Config::DEFAULT_FOV, {0, 0, 0});

	// Initialize shared memory client
	m_memoryClient = std::make_unique<SharedMemoryClient>();
	m_running      = true;

	if (!m_memoryClient->Start(m_running, m_camera))
	{
		DEV_LOG_ERROR("Failed to start shared memory client");
		return false;
	}

	// Initialize renderer
	m_renderer = std::make_unique<OverlayRenderer>();
	if (!m_renderer->Initialize(width, height, x, y))
	{
		return false;
	}

	DEV_LOG_INFO("Debug overlay initialized successfully");

	// Show font status
	if (OverlayRenderer::IsFontLoaded())
	{
		DEV_LOG_INFO("Custom font (Consolas) loaded successfully");
	}
	else
	{
		DEV_LOG_WARNING("Using default font - Consolas not available");
	}

	DEV_LOG_WARNING("Developer console active - messages will fade after " +
	                std::to_string(Config::CONSOLE_MESSAGE_LIFETIME) + " seconds");

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
	DEV_LOG_INFO("Debug overlay shutdown complete");
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
