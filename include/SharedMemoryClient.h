#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "SharedDefs.h"
#include "Raylib/rlFPSCamera.h"

class SharedMemoryClient
{
public:
	SharedMemoryClient() = default;
	~SharedMemoryClient();

	SharedMemoryClient(const SharedMemoryClient &other)                = delete;
	SharedMemoryClient(SharedMemoryClient &&other) noexcept            = delete;
	SharedMemoryClient &operator=(const SharedMemoryClient &other)     = delete;
	SharedMemoryClient &operator=(SharedMemoryClient &&other) noexcept = delete;

	// Connects to the shared memory and starts the listening thread.
	bool Start(std::atomic<bool> &running, rlFPCamera &camera);

	// Stops the thread and disconnects from shared memory.
	void Stop();

	// Gets the latest draw commands for the rendering loop.
	std::vector<DrawCommandPacket> GetDrawCommands();

private:
	void ClientThreadWorker(const std::atomic<bool> &running, rlFPCamera &camera);

	void ProcessPacket(const PacketHeader &header, const std::byte *data, rlFPCamera &camera);

	void ExpireOldCommands();
	void ClearDrawCommands();

	void ReadFromBuffer(void *dest, size_t offset, size_t size) const;

	// Threading and synchronization
	std::thread       m_clientThread;
	std::atomic<bool> m_stopThread = false;
	std::mutex        m_drawMutex;

	// Handles for Windows objects
	HANDLE              m_hMapFile   = nullptr;
	HANDLE              m_hEvent     = nullptr;
	SharedMemoryLayout *m_pSharedMem = nullptr;

	// Local state
	std::vector<DrawCommandPacket> m_drawCommands;
	float                          m_currentTime = 0.0f;

	static constexpr size_t MAX_DRAW_COMMANDS = 2000;
};
