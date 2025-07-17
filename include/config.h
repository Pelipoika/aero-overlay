#pragma once

namespace Config
{
	// Application settings
	constexpr int  TARGET_FPS           = 144;
	constexpr auto OVERLAY_WINDOW_TITLE = "DebugOverlay";
	constexpr auto TARGET_WINDOW_TITLE  = "Counter-Strike 2";

	// Camera settings
	constexpr float DEFAULT_FOV = 90.0f;

	// Pipe communication settings
	constexpr auto PIPE_NAME                = R"(\\.\pipe\CS2DebugOverlay)";
	constexpr int  PIPE_RETRY_DELAY_SECONDS = 2;
	constexpr int  PIPE_READ_DELAY_MS       = 2;

	// Rendering settings
	constexpr size_t MAX_DRAW_COMMANDS = 200;
	constexpr int    DEBUG_TEXT_SIZE   = 20;

	// Debug geometry settings
	constexpr float DEBUG_CYLINDER_RADIUS = 20.0f;
	constexpr float DEBUG_CYLINDER_HEIGHT = 100.0f;
	constexpr int   DEBUG_CYLINDER_SLICES = 10;
}
