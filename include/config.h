#pragma once

namespace Config
{
	// Application settings
	constexpr int  TARGET_FPS           = 144;
	constexpr auto OVERLAY_WINDOW_TITLE = "DebugOverlay";
	constexpr auto TARGET_WINDOW_TITLE  = "Counter-Strike 2";

	// Camera settings
	constexpr float DEFAULT_FOV = 75.0f;

	// Rendering settings
	constexpr size_t MAX_DRAW_COMMANDS = 20000;
	constexpr int    DEBUG_TEXT_SIZE   = 14;

	// Debug geometry settings
	constexpr float DEBUG_CYLINDER_RADIUS = 20.0f;
	constexpr float DEBUG_CYLINDER_HEIGHT = 100.0f;
	constexpr int   DEBUG_CYLINDER_SLICES = 10;
}
