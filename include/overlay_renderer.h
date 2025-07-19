#pragma once

#include <vector>

#include "SharedDefs.h"
#include "Raylib/rlFPSCamera.h"

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

	void BeginFrame() const;
	void EndFrame() const;

	void RenderCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera) const;

	static void RenderDebugInfo();
	static bool ShouldClose();

private:
	static void Render3DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera);
	static void Render2DCommands(const std::vector<DrawCommandPacket> &commands, const rlFPCamera &camera);

	bool m_initialized;
};
