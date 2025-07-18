#pragma once

#include <vector>

#include "SharedDefs.h"
#include "Raylib/rlFPSCamera.h"

class OverlayRenderer
{
public:
	OverlayRenderer();
	~OverlayRenderer();

	bool Initialize(int width, int height, int x, int y);
	void Shutdown();

	void BeginFrame() const;
	void EndFrame();

	void RenderCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera) const;
	void RenderDebugInfo();

	bool ShouldClose() const;

private:
	static void Render3DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera);
	static void Render2DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera);

	bool                 m_initialized;
	static constexpr int TARGET_FPS = 144;
};
