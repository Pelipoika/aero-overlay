#include "overlay_application.h"
#include "config.h"
#include "developer_console.h"
#include <chrono>

OverlayApplication::OverlayApplication() : m_camera(), m_running(false), m_targetWindowFound(false) { }

OverlayApplication::~OverlayApplication()
{
	Shutdown();
}

int OverlayApplication::Run()
{
	// Initialize window first - this should always succeed
	if (!InitializeWindow())
	{
		DEV_LOG_ERROR("Failed to initialize overlay window - cannot continue");
		return -1;
	}

	// Initialize core components
	if (!Initialize())
	{
		DEV_LOG_ERROR("Failed to initialize overlay application components");
		return -1;
	}

	MainLoop();
	Shutdown();
	return 0;
}

bool OverlayApplication::InitializeWindow()
{
	DEV_LOG_INFO("Initializing overlay window...");

	// Initialize camera first
	rlFPCameraInit(&m_camera, Config::DEFAULT_FOV, {0, 0, 0});

	// Initialize window manager
	m_windowManager = std::make_unique<WindowManager>();

	// Try to find target window, but don't fail if not found
	m_targetWindowFound = m_windowManager->FindTargetWindow(Config::TARGET_WINDOW_TITLE);

	// Default window dimensions (fallback if target window not found)
	int x = 100, y = 100, width = 1920, height = 1080;

	if (m_targetWindowFound)
	{
		// Get target window bounds
		if (m_windowManager->GetWindowBounds(x, y, width, height))
		{
			DEV_LOG_INFO("Target window found: " + std::to_string(width) + "x" + std::to_string(height) +
			             " at (" + std::to_string(x) + ", " + std::to_string(y) + ")");
		}
		else
		{
			DEV_LOG_WARNING("Target window found but failed to get bounds, using defaults");
			m_targetWindowFound = false;
		}
	}
	else
	{
		DEV_LOG_WARNING("Target window not found - using default dimensions: " +
		                std::to_string(width) + "x" + std::to_string(height));
	}

	// Initialize renderer with current dimensions
	m_renderer = std::make_unique<OverlayRenderer>();
	if (!m_renderer->Initialize(width, height, x, y))
	{
		DEV_LOG_ERROR("Failed to initialize renderer");
		return false;
	}

	// Show font status
	if (OverlayRenderer::IsFontLoaded())
	{
		DEV_LOG_INFO("Custom font (Consolas) loaded successfully");
	}
	else
	{
		DEV_LOG_WARNING("Using default font - Consolas not available");
	}

	DEV_LOG_INFO("Overlay window initialized successfully");
	return true;
}

bool OverlayApplication::Initialize()
{
	DEV_LOG_INFO("Initializing overlay application components...");

	m_memoryClient = std::make_unique<SharedMemoryClient>();
	m_running      = true;

	if (!m_memoryClient->Start(m_running, m_camera))
	{
		DEV_LOG_WARNING("Failed to start shared memory client - will retry during runtime");
	}

	DEV_LOG_INFO("Debug overlay components initialized successfully");

	DEV_LOG_WARNING("Developer console active - messages will fade after " +
	                std::to_string(Config::CONSOLE_MESSAGE_LIFETIME) + " seconds");

	return true;
}

void OverlayApplication::UpdateTargetWindowBounds()
{
	if (!m_windowManager)
		return;

	// Check if we need to find the target window
	if (!m_targetWindowFound)
	{
		m_targetWindowFound = m_windowManager->FindTargetWindow(Config::TARGET_WINDOW_TITLE);
		if (m_targetWindowFound)
		{
			DEV_LOG_INFO("Target window detected: " + std::string(Config::TARGET_WINDOW_TITLE));
		}
		else
		{
			// Only check every few seconds to avoid spam
			static auto lastCheck = std::chrono::steady_clock::now();
			auto        now       = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() >= 5)
			{
				lastCheck = now;
				DEV_LOG_INFO("Still waiting for target window: " + std::string(Config::TARGET_WINDOW_TITLE));
			}
			return;
		}
	}

	// Update window bounds if target window exists
	int x, y, width, height;
	if (m_windowManager->GetWindowBounds(x, y, width, height))
	{
		// Only update if bounds have changed significantly
		static int    lastX     = -1, lastY = -1, lastWidth = -1, lastHeight = -1;
		constexpr int threshold = 10; // pixels

		if (abs(x - lastX) > threshold || abs(y - lastY) > threshold ||
		    abs(width - lastWidth) > threshold || abs(height - lastHeight) > threshold)
		{
			if (m_renderer)
			{
				// Update renderer window position and size
				SetWindowSize(width, height);
				SetWindowPosition(x, y);

				DEV_LOG_INFO("Updated overlay bounds: " + std::to_string(width) + "x" + std::to_string(height) +
				             " at (" + std::to_string(x) + ", " + std::to_string(y) + ")");
			}

			lastX      = x;
			lastY      = y;
			lastWidth  = width;
			lastHeight = height;
		}
	}
	else
	{
		// Target window might have been closed
		DEV_LOG_WARNING("Lost target window - will search for it again");
		m_targetWindowFound = false;
	}
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
		// Update target window bounds (find target window or resize to match)
		UpdateTargetWindowBounds();

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
