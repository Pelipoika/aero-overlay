#pragma once

#include "win32_minimal.h"
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include "packets.h"
#include "rlFPSCamera.h"

class PipeClient
{
public:
	PipeClient();
	~PipeClient();

	bool Start(std::atomic<bool> &running, rlFPCamera &camera);
	void Stop();

	std::vector<DrawCommandPacket> GetDrawCommands();
	void                           ClearDrawCommands();

private:
	void PacketThreadWorker(std::atomic<bool> &running, rlFPCamera &camera);
	void ProcessPacket(const Packet *packet, rlFPCamera &camera);

	std::thread                    m_packetThread;
	std::vector<DrawCommandPacket> m_drawCommands;
	std::mutex                     m_drawMutex;

	static constexpr size_t MAX_DRAW_COMMANDS = 200;
};
