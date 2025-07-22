#pragma once

#include <vector>

#include "SharedDefs.h"
#include "Raylib/rlFPSCamera.h"
#include "developer_console.h"

class OverlayRenderer
{
public:
	OverlayRenderer();
	~OverlayRenderer();

	OverlayRenderer(const OverlayRenderer &other)                = delete;
	OverlayRenderer(OverlayRenderer &&other) noexcept            = delete;
	OverlayRenderer &operator=(const OverlayRenderer &other)     = delete;
	OverlayRenderer &operator=(OverlayRenderer &&other) noexcept = delete;

	bool Initialize(int width, int height, int x, int y);
	void Shutdown();

	void BeginFrame();
	void EndFrame();

	void RenderCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera) const;

	// Font access
	static Font GetConsolasFont() { return s_consolasFont; }
	static bool IsFontLoaded() { return s_fontLoaded; }

	static void RenderDebugInfo();
	static bool ShouldClose();

private:
	static void Render3DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera);
	static void Render2DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera);

	bool              m_initialized;
	DeveloperConsole *m_console;

	// Font management
	static Font s_consolasFont;
	static bool s_fontLoaded;
	bool        LoadCustomFont();
	void        UnloadCustomFont();
};
