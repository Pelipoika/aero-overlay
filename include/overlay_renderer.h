#pragma once

#include "raylib.h"
#include "rlFPSCamera.h"
#include "packets.h"
#include <vector>

class OverlayRenderer
{
public:
	OverlayRenderer();
	~OverlayRenderer();

	bool Initialize(int width, int height, int x, int y);
	void Shutdown();

	void BeginFrame();
	void EndFrame();

	void RenderCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera);
	void RenderDebugInfo();

	bool ShouldClose() const;

private:
	void Render3DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera);
	void Render2DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera);

	bool                 m_initialized;
	static constexpr int TARGET_FPS = 144;
};
