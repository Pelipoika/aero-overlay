#include "overlay_renderer.h"

#include "config.h"
#include "developer_console.h"

// Static font members
Font OverlayRenderer::s_consolasFont = {};
bool OverlayRenderer::s_fontLoaded   = false;

OverlayRenderer::OverlayRenderer() : m_initialized(false), m_console(nullptr) { }

OverlayRenderer::~OverlayRenderer()
{
	Shutdown();
}

bool OverlayRenderer::Initialize(const int width, const int height, const int x, const int y)
{
	if (m_initialized)
		return true;

	SetTraceLogLevel(LOG_ERROR);

	SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_MOUSE_PASSTHROUGH |
	               FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST |
	               FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_ALWAYS_RUN);

	InitWindow(width, height, Config::OVERLAY_WINDOW_TITLE);

	if (!IsWindowReady())
	{
		DEV_LOG_ERROR("Failed to initialize raylib window");
		return false;
	}

	SetWindowSize(width, height);
	SetWindowPosition(x, y);
	SetTargetFPS(Config::TARGET_FPS);

	// Load custom font
	if (!LoadCustomFont())
	{
		DEV_LOG_WARNING("Failed to load Consolas font, using default font");
	}

	if (IsFontLoaded())
	{
		SetTextureFilter(GetConsolasFont().texture, TEXTURE_FILTER_POINT);
	}

	// Initialize developer console
	m_console = &DeveloperConsole::GetInstance();
	m_console->SetMaxMessages(Config::CONSOLE_MAX_MESSAGES);
	m_console->SetMessageLifetime(Config::CONSOLE_MESSAGE_LIFETIME);
	m_console->SetFadeOutTime(Config::CONSOLE_FADE_TIME);

	m_initialized = true;

	DEV_LOG_INFO("Overlay renderer initialized successfully");
	return true;
}

void OverlayRenderer::Shutdown()
{
	if (m_initialized)
	{
		DEV_LOG_INFO("Shutting down overlay renderer");
		UnloadCustomFont();
		CloseWindow();
		m_initialized = false;
		m_console     = nullptr;
	}
}

bool OverlayRenderer::LoadCustomFont()
{
	// Try to load Consolas font from system
	const char *fontPaths[] = {
		"C:/Windows/Fonts/consola.ttf",  // Windows system font path
		"./fonts/consola.ttf",           // Local font path
		"./consola.ttf"                  // Fallback local path
	};

	for (const char *path : fontPaths)
	{
		if (FileExists(path))
		{
			s_consolasFont = LoadFontEx(path, Config::CONSOLE_FONT_SIZE, nullptr, 0);
			if (s_consolasFont.texture.id > 0)
			{
				s_fontLoaded = true;
				DEV_LOG_INFO("Loaded Consolas font from: " + std::string(path));

				return true;
			}

			DEV_LOG_WARNING("Found font file but failed to load: " + std::string(path));
		}
	}

	// Try loading from the default font with a monospace preference
	int fontChars = 95; // Standard ASCII printable characters

	// Generate default character set
	int *codepoints = LoadCodepoints(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~", &fontChars);

	if (codepoints != nullptr)
	{
		s_consolasFont = LoadFontEx("", Config::CONSOLE_FONT_SIZE, codepoints, fontChars);
		UnloadCodepoints(codepoints);

		if (s_consolasFont.texture.id > 0)
		{
			s_fontLoaded = true;
			DEV_LOG_WARNING("Using default font as Consolas fallback");
			return true;
		}
	}

	// Final fallback - use default font
	s_consolasFont = GetFontDefault();
	s_fontLoaded   = false;
	DEV_LOG_ERROR("Failed to load any custom font, using system default");
	return false;
}

void OverlayRenderer::UnloadCustomFont()
{
	if (s_fontLoaded)
	{
		UnloadFont(s_consolasFont);
		s_fontLoaded = false;
		DEV_LOG_INFO("Unloaded custom font");
	}
}

void OverlayRenderer::BeginFrame() const
{
	if (!m_initialized)
		return;

	// Update console with delta time
	if (m_console)
	{
		m_console->Update();
	}

	BeginDrawing();
	ClearBackground(BLANK);
}

void OverlayRenderer::EndFrame() const
{
	if (!m_initialized)
		return;

	// Render developer console last so it appears on top
	if (m_console)
	{
		m_console->Render();
	}

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
		switch (cmd.type)
		{
			case DrawCommandType::LINE:
			{
				auto [start, end] = cmd.line;
				DrawLine3D(start.ToRayLib(), end.ToRayLib(), cmd.color);
				break;
			}
			case DrawCommandType::TRIANGLE:
			{
				auto [p1, p2, p3] = cmd.triangle;
				DrawTriangle3D(p1.ToRayLib(), p2.ToRayLib(), p3.ToRayLib(), cmd.color);
				break;
			}
			case DrawCommandType::SPHERE:
			{
				auto [center, radius] = cmd.sphere;
				DrawSphereWires(center.ToRayLib(), radius, Config::DEBUG_CYLINDER_SLICES, Config::DEBUG_CYLINDER_SLICES, cmd.color);
				break;
			}
			case DrawCommandType::CIRCLE:
			{
				auto [center, xAxis, yAxis, radius] = cmd.circle;
				DrawCircle3D(center.ToRayLib(), radius, Vector3{xAxis.x, xAxis.y, xAxis.z}, 90.f, cmd.color);
				break;
			}
			case DrawCommandType::BBOX:
			{
				auto [mins, maxs] = cmd.box;
				DrawBoundingBox({mins.ToRayLib(), maxs.ToRayLib()}, cmd.color);
				break;
			}
			case DrawCommandType::TEXT:
			{
				// Handled in Render2DCommands
				break;
			}
			default:  // NOLINT(clang-diagnostic-covered-switch-default)
			{
				DEV_LOG_WARNING("Unknown draw command type: " + std::to_string(static_cast<int>(cmd.type)));
				break;
			}
		}
	}

	rlFPCameraEndMode3D();
}

void OverlayRenderer::Render2DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera)
{
	for (const auto &cmd : commands)
	{
		if (cmd.type != DrawCommandType::TEXT)
		{
			continue;
		}

		constexpr float fontSize = static_cast<float>(Config::DEBUG_TEXT_SIZE);

		Vector2 screenPos{};
		bool    shouldDraw = false;

		if (cmd.text.onscreen)
		{
			screenPos = {
				.x = cmd.text.position.x * static_cast<float>(GetScreenWidth()),
				.y = cmd.text.position.y * static_cast<float>(GetScreenHeight())
			};

			shouldDraw = true;
		}
		else
		{
			const Vector2 projectedPos = GetWorldToScreen(cmd.text.position.ToRayLib(), camera.ViewCamera);

			const bool onScreen = (projectedPos.x >= 0) && (projectedPos.x < static_cast<float>(GetScreenWidth())) &&
			                      (projectedPos.y >= 0) && (projectedPos.y < static_cast<float>(GetScreenHeight()));

			const Vector3 camForward = Vector3Subtract(camera.ViewCamera.target, camera.ViewCamera.position);
			const Vector3 toPoint    = Vector3Subtract(cmd.text.position.ToRayLib(), camera.ViewCamera.position);
			const bool    inFront    = Vector3DotProduct(camForward, toPoint) > 0;

			if (onScreen && inFront)
			{
				screenPos  = projectedPos;
				shouldDraw = true;
			}
		}

		if (!shouldDraw)
		{
			continue;
		}

		// Measure text dimensions
		Vector2 textSize;
		if (s_fontLoaded)
		{
			textSize = MeasureTextEx(s_consolasFont, cmd.text.text, fontSize, 1.0f);
		}
		else
		{
			textSize = {.x = static_cast<float>(MeasureText(cmd.text.text, static_cast<int>(fontSize))), .y = fontSize};
		}

		// Calculate horizontal alignment offset
		float horizontalOffset = 0.0f;
		switch (cmd.text.horizontalAlignment)
		{
			case TextHorizontalAlignment::LEFT:
				horizontalOffset = 0.0f;
				break;
			case TextHorizontalAlignment::CENTER:
				horizontalOffset = -textSize.x * 0.5f;
				break;
			case TextHorizontalAlignment::RIGHT:
				horizontalOffset = -textSize.x;
				break;
		}

		// Calculate vertical alignment offset
		float verticalOffset = 0.0f;
		switch (cmd.text.verticalAlignment)
		{
			case TextVerticalAlignment::TOP:
				verticalOffset = 0.0f;
				break;
			case TextVerticalAlignment::CENTER:
				verticalOffset = -textSize.y * 0.5f;
				break;
			case TextVerticalAlignment::BOTTOM:
				verticalOffset = -textSize.y;
				break;
		}

		// Apply alignment offsets
		const Vector2 drawPos = {
			screenPos.x + horizontalOffset,
			screenPos.y + verticalOffset
		};

		// Draw the text
		if (s_fontLoaded)
		{
			DrawTextEx(s_consolasFont, cmd.text.text, drawPos, fontSize, 1.0f, cmd.color);
		}
		else
		{
			DrawText(cmd.text.text, static_cast<int>(drawPos.x), static_cast<int>(drawPos.y), static_cast<int>(fontSize), cmd.color);
		}
	}
}

bool OverlayRenderer::ShouldClose()
{
	return WindowShouldClose();
}
