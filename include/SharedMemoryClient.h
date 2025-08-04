#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>

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

	// Check if the client is currently connected to the server
	bool IsConnected() const;

private:
	// Initialization Methods
	bool InitializeSharedMemoryResources();
	bool InitializeBufferSynchronization() const;
	bool StartWorkerThread(std::atomic<bool> &running, rlFPCamera &camera);

	// Cleanup Methods
	void CleanupResources() noexcept;
	void ResetTimingState() noexcept;

	// Worker Thread Methods
	void ClientThreadWorker(const std::atomic<bool> &running, rlFPCamera &camera);
	void ProcessAvailablePackets(rlFPCamera &camera);
	bool ProcessSinglePacket(size_t &tail, size_t head, rlFPCamera &camera);

	// Packet Processing Methods
	void ProcessPacket(const PacketHeader &header, const std::byte *data, rlFPCamera &camera);
	void ProcessDrawCommand(const PacketHeader &header, const std::byte *data);
	void ProcessWorldUpdate(const PacketHeader &header, const std::byte *data, rlFPCamera &camera);

	// Validation and Utility Methods
	bool ValidatePacketSize(uint32_t totalPacketSize, uint32_t dataSize) const;
	void UpdateCameraFromWorldState(const WorldUpdatePacket &worldUpdate, rlFPCamera &camera);

	// Draw Command Management
	void ExpireOldCommands();
	void ClearDrawCommands();
	void CheckWorldUpdateTimeout();

	// Buffer Operations
	void ReadFromBuffer(void *dest, size_t offset, size_t size) const;

	// Threading and synchronization
	std::thread        m_clientThread;
	std::atomic<bool>  m_stopThread = false;
	mutable std::mutex m_drawMutex;  // mutable for const methods

	// Handles for Windows objects
	HANDLE              m_hMapFile   = nullptr;
	HANDLE              m_hEvent     = nullptr;
	SharedMemoryLayout *m_pSharedMem = nullptr;

	// Local state
	std::vector<DrawCommandPacket>        m_drawCommands;
	float                                 m_currentTime = 0.0f;
	std::chrono::steady_clock::time_point m_lastWorldUpdateTime;

	// Configuration constants
	static constexpr size_t MAX_DRAW_COMMANDS    = 2000;
	static constexpr auto   WORLD_UPDATE_TIMEOUT = std::chrono::seconds(5);
};
