#include "SharedMemoryClient.h"
#include "developer_console.h"

SharedMemoryClient::~SharedMemoryClient()
{
	Stop();
}

bool SharedMemoryClient::Start(std::atomic<bool> &running, rlFPCamera &camera)
{
	// Reset the stop thread flag at the beginning
	m_stopThread = false;

	// Initialize the last world update time
	m_lastWorldUpdateTime = std::chrono::steady_clock::now();

	// Clear any existing draw commands from previous sessions
	ClearDrawCommands();

	// Reset current time
	m_currentTime = 0.0f;

	// 1. Open the event used for signaling.
	m_hEvent = OpenEventW(SYNCHRONIZE, FALSE, EVENT_NAME);
	if (m_hEvent == nullptr)
	{
		const DWORD error    = GetLastError();
		std::string errorMsg = "Client: OpenEvent failed, GLE=" + std::to_string(error);
		switch (error)
		{
			case ERROR_FILE_NOT_FOUND:
				errorMsg += " (Event not found - server may not be running)";
				break;
			case ERROR_ACCESS_DENIED:
				errorMsg += " (Access denied - insufficient permissions)";
				break;
			default:
				break;
		}
		errorMsg += ". Is the server running?";
		DEV_LOG_ERROR(errorMsg);
		return false;
	}

	// 2. Open the file mapping object.
	m_hMapFile = OpenFileMappingW(
	                              FILE_MAP_ALL_ACCESS, // Read-only access
	                              FALSE,               // Do not inherit the name
	                              SHARED_MEM_NAME      // Name of mapping object
	                             );

	if (m_hMapFile == nullptr)
	{
		const DWORD error    = GetLastError();
		std::string errorMsg = "Client: OpenFileMapping failed, GLE=" + std::to_string(error);
		switch (error)
		{
			case ERROR_FILE_NOT_FOUND:
				errorMsg += " (Shared memory not found - server may not be running)";
				break;
			case ERROR_ACCESS_DENIED:
				errorMsg += " (Access denied - insufficient permissions)";
				break;
			default:
				break;
		}
		DEV_LOG_ERROR(errorMsg);
		Stop();
		return false;
	}

	// 3. Map the view of the file.
	m_pSharedMem = static_cast<SharedMemoryLayout*>(MapViewOfFile(
	                                                              m_hMapFile,
	                                                              FILE_MAP_ALL_ACCESS,
	                                                              0,
	                                                              0,
	                                                              sizeof(SharedMemoryLayout)
	                                                             ));

	if (m_pSharedMem == nullptr)
	{
		const DWORD error    = GetLastError();
		std::string errorMsg = "Client: MapViewOfFile failed, GLE=" + std::to_string(error);
		switch (error)
		{
			case ERROR_ACCESS_DENIED:
				errorMsg += " (Access denied - permission mismatch or insufficient privileges)";
				break;
			case ERROR_INVALID_PARAMETER:
				errorMsg += " (Invalid parameter - size or offset issue)";
				break;
			case ERROR_NOT_ENOUGH_MEMORY:
				errorMsg += " (Not enough memory)";
				break;
			default:
				break;
		}
		DEV_LOG_ERROR(errorMsg);
		Stop();
		return false;
	}

	// 4. Initialize client buffer position to match server's tail
	// This ensures we start reading from the correct position
	if (m_pSharedMem)
	{
		// Validate buffer state first
		size_t currentHead = m_pSharedMem->head;
		size_t currentTail = m_pSharedMem->tail;

		// Check if head/tail positions are valid
		if (currentHead >= SHARED_MEM_BUFFER_SIZE || currentTail >= SHARED_MEM_BUFFER_SIZE)
		{
			DEV_LOG_WARNING("Client: Invalid buffer positions detected (head: " + std::to_string(currentHead) +
			                ", tail: " + std::to_string(currentTail) + "). Resetting to 0.");
			m_pSharedMem->head = 0;
			m_pSharedMem->tail = 0;
		}
		else
		{
			// Set our tail to the server's current head to start fresh
			m_pSharedMem->tail = currentHead;
			DEV_LOG_INFO("Client: Synchronized buffer position - head: " + std::to_string(currentHead) +
			             ", tail: " + std::to_string(currentHead) + " (synchronized)");
		}
	}

	// 5. Start the worker thread.
	try
	{
		m_clientThread = std::thread(&SharedMemoryClient::ClientThreadWorker, this, std::ref(running), std::ref(camera));
	}
	catch (const std::exception &e)
	{
		DEV_LOG_ERROR("Failed to start client thread: " + std::string(e.what()));
		Stop();
		return false;
	}

	DEV_LOG_INFO("Client: Connected to shared memory successfully");
	return true;
}

void SharedMemoryClient::Stop()
{
	// Set stop flag first to signal worker thread to exit
	m_stopThread = true;

	// Signal the event to make sure the worker thread is not stuck waiting.
	if (m_hEvent)
	{
		SetEvent(m_hEvent);
	}

	// Wait for worker thread to finish
	if (m_clientThread.joinable())
	{
		m_clientThread.join();
	}

	// Clean up shared memory mapping
	if (m_pSharedMem != nullptr)
	{
		UnmapViewOfFile(m_pSharedMem);
		m_pSharedMem = nullptr;
	}

	// Clean up file mapping handle
	if (m_hMapFile != nullptr)
	{
		CloseHandle(m_hMapFile);
		m_hMapFile = nullptr;
	}

	// Clean up event handle
	if (m_hEvent != nullptr)
	{
		CloseHandle(m_hEvent);
		m_hEvent = nullptr;
	}

	// Clear any remaining draw commands
	ClearDrawCommands();

	// Reset timing state
	m_currentTime         = 0.0f;
	m_lastWorldUpdateTime = std::chrono::steady_clock::now();

	// Note: m_stopThread will be reset to false in Start() when reconnecting
}

/**
 * \brief A safe helper function to read data from the circular buffer, handling wrapping correctly.
 * \param dest A pointer to the destination buffer.
 * \param offset The starting position in the shared buffer to read from.
 * \param size The number of bytes to read.
 */
void SharedMemoryClient::ReadFromBuffer(void *dest, const size_t offset, const size_t size) const
{
	const size_t endPos = offset + size;
	auto *       dst    = static_cast<std::byte*>(dest);

	if (endPos > SHARED_MEM_BUFFER_SIZE)
	{
		// Data wraps around the buffer, requiring two copies.
		const size_t firstPartSize = SHARED_MEM_BUFFER_SIZE - offset;
		memcpy(dst, m_pSharedMem->buffer + offset, firstPartSize);
		memcpy(dst + firstPartSize, m_pSharedMem->buffer, size - firstPartSize);
	}
	else
	{
		// Data is contiguous and can be read in a single copy.
		memcpy(dst, m_pSharedMem->buffer + offset, size);
	}
}

void SharedMemoryClient::ClientThreadWorker(const std::atomic<bool> &running, rlFPCamera &camera)
{
	DEV_LOG_INFO("Client worker thread started");

	while (running && !m_stopThread && m_pSharedMem)
	{
		// Wait for the server to signal that new data is available.
		const DWORD waitResult = WaitForSingleObject(m_hEvent, 30); // 30ms timeout

		if (waitResult != WAIT_OBJECT_0)
		{
			// Check if we haven't received a world update in 5 seconds
			CheckWorldUpdateTimeout();
			continue; // Timeout or error, loop again.
		}

		// Use a memory barrier to ensure we see the head value that was set *after*
		// the server finished writing the data.
		_ReadBarrier();

		size_t head = m_pSharedMem->head;
		size_t tail = m_pSharedMem->tail;

		// Process all available packets
		while (tail != head && !m_stopThread)
		{
			// Read packet header using the safe helper function. This prevents a buffer
			// over-read if the header itself wraps around the end of the buffer.
			PacketHeader header;
			ReadFromBuffer(&header, tail, sizeof(header));

			const uint32_t totalPacketSize = sizeof(PacketHeader) + header.size;

			// If the packet size is nonsensical,
			// the buffer is likely corrupted. We can try to recover by skipping all data.
			if (totalPacketSize > SHARED_MEM_BUFFER_SIZE)
			{
				DEV_LOG_ERROR("Client: Corrupted packet detected (size too large: " + std::to_string(totalPacketSize) +
				              "). Flushing buffer.");
				m_pSharedMem->tail = head; // Skip all pending data.
				break;
			}

			// Additional safety check for packet header validity
			if (header.size > SHARED_MEM_BUFFER_SIZE)
			{
				DEV_LOG_ERROR("Client: Invalid packet header size: " + std::to_string(header.size) +
				              ". Skipping packet.");
				// Skip just this packet
				tail               = (tail + sizeof(PacketHeader)) & (SHARED_MEM_BUFFER_SIZE - 1);
				m_pSharedMem->tail = tail;
				continue;
			}

			const size_t dataStart = (tail + sizeof(PacketHeader)) & (SHARED_MEM_BUFFER_SIZE - 1);

			// Read packet data
			std::vector<std::byte> dataBuffer(header.size);
			if (header.size > 0)
			{
				ReadFromBuffer(dataBuffer.data(), dataStart, header.size);
			}

			// Process the packet we just read
			ProcessPacket(header, dataBuffer.data(), camera);

			// Atomically update the tail to release the space back to the server.
			tail               = (tail + totalPacketSize) & (SHARED_MEM_BUFFER_SIZE - 1);
			m_pSharedMem->tail = tail;

			// Re-read head for the next iteration in case the server wrote more data
			// while we were processing this packet.
			_ReadBarrier();
			head = m_pSharedMem->head;
		}
	}

	DEV_LOG_INFO("Client worker thread finished");
}

void SharedMemoryClient::ProcessPacket(const PacketHeader &header, const std::byte *data, rlFPCamera &camera)
{
	switch (header.type)
	{
		case PacketType::DRAW_COMMAND:
		{
			if (header.size != sizeof(DrawCommandPacket))
			{
				DEV_LOG_ERROR("Client: Received DRAW_COMMAND with incorrect size. Expected " +
				              std::to_string(sizeof(DrawCommandPacket)) + ", got " + std::to_string(header.size));
				break;
			}

			std::lock_guard lock(m_drawMutex);
			if (m_drawCommands.size() > MAX_DRAW_COMMANDS)
			{
				m_drawCommands.erase(m_drawCommands.begin());
			}
			m_drawCommands.push_back(*reinterpret_cast<const DrawCommandPacket*>(data));
			break;
		}
		case PacketType::WORLD_UPDATE:
		{
			if (header.size != sizeof(WorldUpdatePacket))
			{
				DEV_LOG_ERROR("Client: Received WORLD_UPDATE with incorrect size. Expected " +
				              std::to_string(sizeof(WorldUpdatePacket)) + ", got " + std::to_string(header.size));
				break;
			}

			const auto &worldUpdate = *reinterpret_cast<const WorldUpdatePacket*>(data);

			// Update the last world update timestamp
			m_lastWorldUpdateTime = std::chrono::steady_clock::now();

			if (std::fabs(worldUpdate.curtime - m_currentTime) > 1.f)
			{
				// Got a large time jump, clear all commands.
				DEV_LOG_INFO("Client: Time jump detected (from " + std::to_string(m_currentTime) +
				             " to " + std::to_string(worldUpdate.curtime) + "), clearing draw commands");
				ClearDrawCommands();
			}

			m_currentTime = worldUpdate.curtime;

			ExpireOldCommands();

			rlFPCameraSetPosition(&camera, worldUpdate.origin.ToRayLib());
			rlFPCameraSetFOV(&camera, worldUpdate.fov);

			camera.ViewAngles = {
				.x = -worldUpdate.viewAngles.y * DEG2RAD,
				.y = worldUpdate.viewAngles.x * DEG2RAD,
			};
			break;
		}
		case PacketType::CLEAR_ALL_DRAWINGS:
		{
			ClearDrawCommands();
			break;
		}
		default:  // NOLINT(clang-diagnostic-covered-switch-default)
		{
			DEV_LOG_WARNING("Client: Unknown packet type " + std::to_string(static_cast<std::uint8_t>(header.type)));
			break;
		}
	}
}

std::vector<DrawCommandPacket> SharedMemoryClient::GetDrawCommands()
{
	std::lock_guard lock(m_drawMutex);
	return m_drawCommands;
}

bool SharedMemoryClient::IsConnected() const
{
	return m_hMapFile != nullptr && m_hEvent != nullptr && m_pSharedMem != nullptr && m_clientThread.joinable();
}

void SharedMemoryClient::ClearDrawCommands()
{
	std::lock_guard lock(m_drawMutex);

	if (!m_drawCommands.empty())
	{
		DEV_LOG_INFO("Client: Clearing " + std::to_string(m_drawCommands.size()) + " draw commands");
	}

	m_drawCommands.clear();
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
		// Haven't received a world update in 5 seconds, clear all draw commands
		ClearDrawCommands();

		// Reset the timer to prevent spamming clear operations
		m_lastWorldUpdateTime = now;
	}
}
