#include "overlay_application.h"
#include "config.h"
#include <iostream>
#include <cstdio>

OverlayApplication::OverlayApplication() : m_camera(), m_running(false) { }

OverlayApplication::~OverlayApplication()
{
	Shutdown();
}

int OverlayApplication::Run()
{
	if (!Initialize())
	{
		fprintf(stderr, "Failed to initialize overlay application\n");
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

	// Initialize pipe client
	m_pipeClient = std::make_unique<PipeClient>();
	m_running    = true;

	if (!m_pipeClient->Start(m_running, m_camera))
	{
		return false;
	}

	printf("Overlay application initialized successfully\n");
	return true;
}

void OverlayApplication::Shutdown()
{
	m_running = false;

	if (m_pipeClient)
	{
		m_pipeClient->Stop();
		m_pipeClient.reset();
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
	while (!m_renderer->ShouldClose() && m_running)
	{
		// Update camera
		rlFPCameraUpdate(&m_camera);

		// Get draw commands from pipe client
		std::vector<DrawCommandPacket> drawCommands;

		if (m_pipeClient)
		{
			drawCommands = m_pipeClient->GetDrawCommands();
		}

		// Render frame
		m_renderer->BeginFrame();
		m_renderer->RenderCommands(drawCommands, m_camera);
		m_renderer->EndFrame();
	}
}
