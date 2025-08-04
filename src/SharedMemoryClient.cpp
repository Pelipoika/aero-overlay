#include "SharedMemoryClient.h"
#include "developer_console.h"
#include "SharedMemoryUtils.h"
#include "WindowsHandleRAII.h"

#include <sstream>

using namespace SharedMemoryConstants;
using namespace SharedMemoryErrors;

namespace
{
	// Validate buffer positions to prevent corruption
	bool ValidateBufferPositions(const size_t head, const size_t tail, const size_t bufferSize)
	{
		return head < bufferSize && tail < bufferSize;
	}

	// Check if read operation would wrap around buffer
	bool WillWrapBuffer(const size_t offset, const size_t size, const size_t bufferSize)
	{
		return (offset + size) > bufferSize;
	}
}

SharedMemoryClient::~SharedMemoryClient()
{
	Stop();
}

bool SharedMemoryClient::Start(std::atomic<bool> &running, rlFPCamera &camera)
{
	// Reset state
	m_stopThread          = false;
	m_lastWorldUpdateTime = std::chrono::steady_clock::now();
	m_currentTime         = 0.0f;

	// Clear any existing draw commands from previous sessions
	ClearDrawCommands();

	if (!InitializeSharedMemoryResources())
	{
		return false;
	}

	if (!InitializeBufferSynchronization())
	{
		Stop();
		return false;
	}

	if (!StartWorkerThread(running, camera))
	{
		Stop();
		return false;
	}

	LogError(ErrorInfo(ErrorSeverity::Info, "Client: Connected to shared memory successfully"));
	return true;
}

bool SharedMemoryClient::InitializeSharedMemoryResources()
{
	// Open the event used for signaling
	WindowsHandleRAII eventHandle(OpenEventW(SYNCHRONIZE, FALSE, EVENT_NAME));
	if (!eventHandle)
	{
		const DWORD       error    = GetLastError();
		const std::string errorMsg = FormatConnectionError("Client: OpenEvent", error) + ". Is the server running?";
		LogError(ErrorInfo(ErrorSeverity::Error, errorMsg, error));
		return false;
	}

	// Open the file mapping object
	WindowsHandleRAII mapFileHandle(OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME));
	if (!mapFileHandle)
	{
		const DWORD       error    = GetLastError();
		const std::string errorMsg = FormatConnectionError("Client: OpenFileMapping", error);
		LogError(ErrorInfo(ErrorSeverity::Error, errorMsg, error));
		return false;
	}

	// Map the view of the file
	SharedMemoryViewRAII sharedMemView(MapViewOfFile(mapFileHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemoryLayout)));
	if (!sharedMemView)
	{
		const DWORD       error    = GetLastError();
		const std::string errorMsg = FormatConnectionError("Client: MapViewOfFile", error);
		LogError(ErrorInfo(ErrorSeverity::Error, errorMsg, error));
		return false;
	}

	// All resources successfully initialized, transfer ownership
	m_hEvent     = eventHandle.release();
	m_hMapFile   = mapFileHandle.release();
	m_pSharedMem = sharedMemView.as<SharedMemoryLayout>();
	sharedMemView.release(); // Don't auto-unmap, we'll manage it manually now

	return true;
}

bool SharedMemoryClient::InitializeBufferSynchronization() const
{
	if (!m_pSharedMem)
	{
		LogError(ErrorInfo(ErrorSeverity::Fatal, "Client: Shared memory not initialized"));
		return false;
	}

	// Use atomic operations for thread-safe buffer position reading
	const size_t currentHead = m_pSharedMem->head;
	const size_t currentTail = m_pSharedMem->tail;

	if (!ValidateBufferPositions(currentHead, currentTail, SHARED_MEM_BUFFER_SIZE))
	{
		const std::string errorMsg = FormatBufferError("Invalid buffer positions detected",
		                                               std::max(currentHead, currentTail));
		LogError(ErrorInfo(ErrorSeverity::Warning, errorMsg));

		// Reset buffer positions safely
		m_pSharedMem->head = 0;
		m_pSharedMem->tail = 0;
	}
	else
	{
		// Set our tail to the server's current head to start fresh
		m_pSharedMem->tail = currentHead;

		std::ostringstream oss;
		oss << "Client: Synchronized buffer position - head: " << currentHead << ", tail: " << currentHead << " (synchronized)";
		LogError(ErrorInfo(ErrorSeverity::Info, oss.str()));
	}

	return true;
}

bool SharedMemoryClient::StartWorkerThread(std::atomic<bool> &running, rlFPCamera &camera)
{
	try
	{
		m_clientThread = std::thread(&SharedMemoryClient::ClientThreadWorker, this, std::ref(running), std::ref(camera));
		return true;
	}
	catch (const std::exception &e)
	{
		const std::string errorMsg = "Failed to start client thread: " + std::string(e.what());
		LogError(ErrorInfo(ErrorSeverity::Fatal, errorMsg));
		return false;
	}
}

void SharedMemoryClient::Stop()
{
	// Set stop flag first to signal worker thread to exit
	m_stopThread = true;

	// Signal the event to make sure the worker thread is not stuck waiting
	if (m_hEvent)
	{
		SetEvent(m_hEvent);
	}

	// Wait for worker thread to finish
	if (m_clientThread.joinable())
	{
		m_clientThread.join();
	}

	// Clean up resources using RAII for exception safety
	CleanupResources();

	// Clear state
	ClearDrawCommands();
	ResetTimingState();
}

void SharedMemoryClient::CleanupResources() noexcept
{
	// Clean up shared memory mapping
	if (m_pSharedMem)
	{
		UnmapViewOfFile(m_pSharedMem);
		m_pSharedMem = nullptr;
	}

	// Clean up handles
	if (m_hMapFile)
	{
		CloseHandle(m_hMapFile);
		m_hMapFile = nullptr;
	}

	if (m_hEvent)
	{
		CloseHandle(m_hEvent);
		m_hEvent = nullptr;
	}
}

void SharedMemoryClient::ResetTimingState() noexcept
{
	m_currentTime         = 0.0f;
	m_lastWorldUpdateTime = std::chrono::steady_clock::now();
}

void SharedMemoryClient::ReadFromBuffer(void *dest, const size_t offset, const size_t size) const
{
	if (!dest || !m_pSharedMem || size == 0)
	{
		return;
	}

	auto *dst = static_cast<std::byte*>(dest);

	if (WillWrapBuffer(offset, size, SHARED_MEM_BUFFER_SIZE))
	{
		// Data wraps around the buffer, requiring two copies
		const size_t firstPartSize = SHARED_MEM_BUFFER_SIZE - offset;
		std::memcpy(dst, m_pSharedMem->buffer + offset, firstPartSize);
		std::memcpy(dst + firstPartSize, m_pSharedMem->buffer, size - firstPartSize);
	}
	else
	{
		// Data is contiguous and can be read in a single copy
		std::memcpy(dst, m_pSharedMem->buffer + offset, size);
	}
}

void SharedMemoryClient::ClientThreadWorker(const std::atomic<bool> &running, rlFPCamera &camera)
{
	LogError(ErrorInfo(ErrorSeverity::Info, "Client worker thread started"));

	while (running && !m_stopThread && m_pSharedMem)
	{
		// Wait for the server to signal that new data is available
		const DWORD waitResult = WaitForSingleObject(m_hEvent, WAIT_TIMEOUT_MS);

		if (waitResult != WAIT_OBJECT_0)
		{
			CheckWorldUpdateTimeout();
			continue;
		}

		ProcessAvailablePackets(camera);
	}

	LogError(ErrorInfo(ErrorSeverity::Info, "Client worker thread finished"));
}

void SharedMemoryClient::ProcessAvailablePackets(rlFPCamera &camera)
{
	// Use memory barrier to ensure we see the head value that was set after
	// the server finished writing the data
	_ReadBarrier();

	size_t head = m_pSharedMem->head;
	size_t tail = m_pSharedMem->tail;

	// Validate positions before processing
	if (!ValidateBufferPositions(head, tail, SHARED_MEM_BUFFER_SIZE))
	{
		LogError(ErrorInfo(ErrorSeverity::Error, "Invalid buffer positions detected during processing"));
		return;
	}

	// Process all available packets
	while (tail != head && !m_stopThread)
	{
		if (!ProcessSinglePacket(tail, head, camera))
		{
			break; // Error occurred or buffer corrupted
		}

		// Re-read head for the next iteration in case the server wrote more data
		_ReadBarrier();
		head = m_pSharedMem->head;
	}
}

bool SharedMemoryClient::ProcessSinglePacket(size_t &tail, size_t head, rlFPCamera &camera)
{
	// Read packet header using the safe helper function
	PacketHeader header;
	ReadFromBuffer(&header, tail, sizeof(header));

	const uint32_t totalPacketSize = sizeof(PacketHeader) + header.size;

	// Validate packet size
	if (!ValidatePacketSize(totalPacketSize, header.size))
	{
		return false; // Buffer corruption detected
	}

	// Pre-allocate buffer for better performance
	std::vector<std::byte> dataBuffer;
	dataBuffer.reserve(header.size);
	dataBuffer.resize(header.size);

	if (header.size > 0)
	{
		const size_t dataStart = (tail + sizeof(PacketHeader)) & (SHARED_MEM_BUFFER_SIZE - 1);
		ReadFromBuffer(dataBuffer.data(), dataStart, header.size);
	}

	// Process the packet
	ProcessPacket(header, dataBuffer.data(), camera);

	// Atomically update the tail to release the space back to the server
	tail               = (tail + totalPacketSize) & (SHARED_MEM_BUFFER_SIZE - 1);
	m_pSharedMem->tail = tail;

	return true;
}

bool SharedMemoryClient::ValidatePacketSize(const uint32_t totalPacketSize, const uint32_t dataSize) const
{
	if (totalPacketSize > SHARED_MEM_BUFFER_SIZE)
	{
		const std::string errorMsg = FormatBufferError("Corrupted packet detected (total size too large)", totalPacketSize);
		LogError(ErrorInfo(ErrorSeverity::Error, errorMsg));

		// Reset buffer to recover from corruption
		m_pSharedMem->tail = m_pSharedMem->head;
		return false;
	}

	if (dataSize > MAX_REASONABLE_PACKET_SIZE)
	{
		const std::string errorMsg = FormatBufferError("Invalid packet data size", dataSize);
		LogError(ErrorInfo(ErrorSeverity::Error, errorMsg));
		return false;
	}

	return true;
}

void SharedMemoryClient::ProcessPacket(const PacketHeader &header, const std::byte *data, rlFPCamera &camera)
{
	switch (header.type)
	{
		case PacketType::DRAW_COMMAND:
			ProcessDrawCommand(header, data);
			break;
		case PacketType::WORLD_UPDATE:
			ProcessWorldUpdate(header, data, camera);
			break;
		case PacketType::CLEAR_ALL_DRAWINGS:
			ClearDrawCommands();
			break;
		default:  // NOLINT(clang-diagnostic-covered-switch-default)
		{
			std::ostringstream oss;
			oss << "Client: Unknown packet type " << static_cast<std::uint8_t>(header.type);
			LogError(ErrorInfo(ErrorSeverity::Warning, oss.str()));
			break;
		}
	}
}

void SharedMemoryClient::ProcessDrawCommand(const PacketHeader &header, const std::byte *data)
{
	if (header.size != sizeof(DrawCommandPacket))
	{
		std::ostringstream oss;
		oss << "Client: Received DRAW_COMMAND with incorrect size. Expected " << sizeof(DrawCommandPacket) << ", got " << header.size;
		LogError(ErrorInfo(ErrorSeverity::Error, oss.str()));
		return;
	}

	std::lock_guard lock(m_drawMutex);

	// Prevent excessive memory usage with more efficient removal
	if (m_drawCommands.size() >= MAX_DRAW_COMMANDS)
	{
		// Remove multiple oldest commands to prevent frequent reallocations
		const size_t removeCount = std::min(static_cast<size_t>(100), m_drawCommands.size() / 4);
		m_drawCommands.erase(m_drawCommands.begin(), m_drawCommands.begin() + removeCount);
	}

	m_drawCommands.emplace_back(*reinterpret_cast<const DrawCommandPacket*>(data));
}

void SharedMemoryClient::ProcessWorldUpdate(const PacketHeader &header, const std::byte *data, rlFPCamera &camera)
{
	if (header.size != sizeof(WorldUpdatePacket))
	{
		std::ostringstream oss;
		oss << "Client: Received WORLD_UPDATE with incorrect size. Expected " << sizeof(WorldUpdatePacket) << ", got " << header.size;
		LogError(ErrorInfo(ErrorSeverity::Error, oss.str()));
		return;
	}

	const auto &worldUpdate = *reinterpret_cast<const WorldUpdatePacket*>(data);

	// Update the last world update timestamp
	m_lastWorldUpdateTime = std::chrono::steady_clock::now();

	// Check for time jumps that indicate a new session or significant time skip
	if (std::abs(worldUpdate.curtime - m_currentTime) > TIME_JUMP_THRESHOLD)
	{
		std::ostringstream oss;
		oss << "Client: Time jump detected (from " << m_currentTime << " to " << worldUpdate.curtime << "), clearing draw commands";
		LogError(ErrorInfo(ErrorSeverity::Info, oss.str()));
		ClearDrawCommands();
	}

	m_currentTime = worldUpdate.curtime;

	// Remove expired commands based on new current time
	ExpireOldCommands();

	// Update camera state
	UpdateCameraFromWorldState(worldUpdate, camera);
}

void SharedMemoryClient::UpdateCameraFromWorldState(const WorldUpdatePacket &worldUpdate, rlFPCamera &camera)
{
	rlFPCameraSetPosition(&camera, worldUpdate.origin.ToRayLib());
	rlFPCameraSetFOV(&camera, worldUpdate.fov);

	camera.ViewAngles = {
		.x = -worldUpdate.viewAngles.y * DEG2RAD,
		.y = worldUpdate.viewAngles.x * DEG2RAD,
	};
}

std::vector<DrawCommandPacket> SharedMemoryClient::GetDrawCommands()
{
	std::lock_guard lock(m_drawMutex);
	return m_drawCommands;
}

bool SharedMemoryClient::IsConnected() const
{
	return m_hMapFile != nullptr &&
	       m_hEvent != nullptr &&
	       m_pSharedMem != nullptr &&
	       m_clientThread.joinable() &&
	       !m_stopThread;
}

void SharedMemoryClient::ClearDrawCommands()
{
	std::lock_guard lock(m_drawMutex);

	if (!m_drawCommands.empty())
	{
		std::ostringstream oss;
		oss << "Client: Clearing " << m_drawCommands.size() << " draw commands";
		LogError(ErrorInfo(ErrorSeverity::Info, oss.str()));
		m_drawCommands.clear();
	}
}

void SharedMemoryClient::ExpireOldCommands()
{
	std::lock_guard lock(m_drawMutex);
	std::erase_if(m_drawCommands,
	              [this](const DrawCommandPacket &cmd){
		              // A command with duration 0 should be rendered for one frame,
		              // so we check if its end time is 0 but we have a newer time.
		              if (cmd.drawEndTime <= 0.0f)
			              return m_currentTime > 0.0f;
		              return m_currentTime >= cmd.drawEndTime;
	              });
}

void SharedMemoryClient::CheckWorldUpdateTimeout()
{
	const auto now                 = std::chrono::steady_clock::now();
	const auto timeSinceLastUpdate = now - m_lastWorldUpdateTime;

	if (timeSinceLastUpdate >= WORLD_UPDATE_TIMEOUT)
	{
		// Haven't received a world update in the timeout period
		ClearDrawCommands();

		// Reset the timer to prevent spamming clear operations
		m_lastWorldUpdateTime = now;
	}
}
