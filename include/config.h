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
	constexpr int    DEBUG_TEXT_SIZE   = 14; // Default text size when not specified in TextCommandData

	// Debug geometry settings
	constexpr float DEBUG_CYLINDER_RADIUS = 20.0f;
	constexpr float DEBUG_CYLINDER_HEIGHT = 100.0f;
	constexpr int   DEBUG_CYLINDER_SLICES = 10;

	// Developer console settings (Source Engine inspired)
	constexpr int   CONSOLE_FONT_SIZE        = 14;		// Slightly smaller for more messages
	constexpr int   CONSOLE_MAX_MESSAGES     = 20;		// More messages visible
	constexpr float CONSOLE_MESSAGE_LIFETIME = 10.0f;	// 4 seconds at full opacity
	constexpr float CONSOLE_FADE_TIME        = 1.5f;	// 1.5 second fade out
}
