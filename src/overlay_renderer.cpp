#include "overlay_renderer.h"
#include <iostream>
#include "config.h"

OverlayRenderer::OverlayRenderer() : m_initialized(false) { }

OverlayRenderer::~OverlayRenderer()
{
	Shutdown();
}

bool OverlayRenderer::Initialize(int width, int height, int x, int y)
{
	if (m_initialized)
		return true;

	SetTraceLogLevel(LOG_NONE);

	SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_MOUSE_PASSTHROUGH |
	               FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST |
	               FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_ALWAYS_RUN);

	InitWindow(width, height, Config::OVERLAY_WINDOW_TITLE);

	if (!IsWindowReady())
	{
		std::cerr << "Failed to initialize raylib window" << '\n';
		return false;
	}

	SetWindowSize(width, height);
	SetWindowPosition(x, y);
	SetTargetFPS(Config::TARGET_FPS);

	m_initialized = true;
	return true;
}

void OverlayRenderer::Shutdown()
{
	if (m_initialized)
	{
		CloseWindow();
		m_initialized = false;
	}
}

void OverlayRenderer::BeginFrame() const
{
	if (!m_initialized)
		return;

	BeginDrawing();
	ClearBackground(BLANK);
}

void OverlayRenderer::EndFrame()
{
	if (!m_initialized)
		return;

	//RenderDebugInfo();
	EndDrawing();
}

void OverlayRenderer::RenderCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera) const
{
	if (!m_initialized)
		return;

	Render3DCommands(commands, camera);
	Render2DCommands(commands, camera);
}

void OverlayRenderer::Render3DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera)
{
	rlFPCameraBeginMode3D(&camera);

	for (const auto &cmd : commands)
	{
		if (cmd.type == DrawCommandType::LINE)
		{
			DrawLine3D(cmd.line.start.ToRayLib(), cmd.line.end.ToRayLib(), cmd.color);
		}
	}

	// Draw some debug geometry
	//DrawCapsuleWires({-1114, -245, -1215},
	//             Config::DEBUG_CYLINDER_RADIUS,
	//             Config::DEBUG_CYLINDER_RADIUS,
	//             Config::DEBUG_CYLINDER_HEIGHT,
	//             Config::DEBUG_CYLINDER_SLICES,
	//             GREEN);

	rlFPCameraEndMode3D();
}

void OverlayRenderer::Render2DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera)
{
	for (const auto &cmd : commands)
	{
		if (cmd.type == DrawCommandType::TEXT)
		{
			if (cmd.text.onscreen)
			{
				DrawText(cmd.text.text,
				         static_cast<int>(cmd.text.position.x),
				         static_cast<int>(cmd.text.position.y),
				         Config::DEBUG_TEXT_SIZE,
				         cmd.color);
			}
			else
			{
				const Vector2 screenPos = GetWorldToScreen(cmd.text.position.ToRayLib(), camera.ViewCamera);

				const bool onScreen = (screenPos.x >= 0) && (screenPos.x < static_cast<float>(GetScreenWidth())) &&
				                      (screenPos.y >= 0) && (screenPos.y < static_cast<float>(GetScreenHeight()));

				const Vector3 camForward = Vector3Subtract(camera.ViewCamera.target, camera.ViewCamera.position);
				const Vector3 toPoint    = Vector3Subtract(cmd.text.position.ToRayLib(), camera.ViewCamera.position);
				const bool    inFront    = Vector3DotProduct(camForward, toPoint) > 0;

				if (!onScreen || !inFront)
					continue;

				DrawText(cmd.text.text,
				         static_cast<int>(screenPos.x),
				         static_cast<int>(screenPos.y),
				         Config::DEBUG_TEXT_SIZE,
				         cmd.color);
			}
		}
	}
}

void OverlayRenderer::RenderDebugInfo()
{
	DrawText("Debug Overlay Active", 190, 200, Config::DEBUG_TEXT_SIZE, LIGHTGRAY);
	DrawFPS(100, 100);
}

bool OverlayRenderer::ShouldClose() const
{
	return WindowShouldClose();
}
