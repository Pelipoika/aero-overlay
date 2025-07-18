#include "SharedMemoryClient.h"

#include <iostream>

SharedMemoryClient::SharedMemoryClient()
{
	// Constructor
}

SharedMemoryClient::~SharedMemoryClient()
{
	Stop();
}

bool SharedMemoryClient::Start(std::atomic<bool> &running, rlFPCamera &camera)
{
	// 1. Open the event used for signaling.
	m_hEvent = OpenEventW(SYNCHRONIZE, FALSE, EVENT_NAME);
	if (m_hEvent == nullptr)
	{
		const DWORD error = GetLastError();
		std::cerr << "Client: OpenEvent failed, GLE=" << error;
		switch (error)
		{
			case ERROR_FILE_NOT_FOUND:
				std::cerr << " (Event not found - server may not be running)";
				break;
			case ERROR_ACCESS_DENIED:
				std::cerr << " (Access denied - insufficient permissions)";
				break;
			default:
				break;
		}
		std::cerr << ". Is the server running?\n";
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
		const DWORD error = GetLastError();
		std::cerr << "Client: OpenFileMapping failed, GLE=" << error;
		switch (error)
		{
			case ERROR_FILE_NOT_FOUND:
				std::cerr << " (Shared memory not found - server may not be running)";
				break;
			case ERROR_ACCESS_DENIED:
				std::cerr << " (Access denied - insufficient permissions)";
				break;
			default:
				break;
		}
		std::cerr << "\n";
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
		const DWORD error = GetLastError();
		std::cerr << "Client: MapViewOfFile failed, GLE=" << error;
		switch (error)
		{
			case ERROR_ACCESS_DENIED:
				std::cerr << " (Access denied - permission mismatch or insufficient privileges)";
				break;
			case ERROR_INVALID_PARAMETER:
				std::cerr << " (Invalid parameter - size or offset issue)";
				break;
			case ERROR_NOT_ENOUGH_MEMORY:
				std::cerr << " (Not enough memory)";
				break;
			default:
				break;
		}
		std::cerr << "\n";
		Stop();
		return false;
	}

	// 4. Start the worker thread.
	try
	{
		m_clientThread = std::thread(&SharedMemoryClient::ClientThreadWorker, this, std::ref(running), std::ref(camera));
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to start client thread: " << e.what() << '\n';
		Stop();
		return false;
	}

	std::cout << "Client: Connected to shared memory successfully.\n";
	return true;
}

void SharedMemoryClient::Stop()
{
	m_stopThread = true;

	// Signal the event to make sure the worker thread is not stuck waiting.
	if (m_hEvent)
	{
		SetEvent(m_hEvent);
	}

	if (m_clientThread.joinable())
	{
		m_clientThread.join();
	}

	if (m_pSharedMem != nullptr)
	{
		UnmapViewOfFile(m_pSharedMem);
		m_pSharedMem = nullptr;
	}

	if (m_hMapFile != nullptr)
	{
		CloseHandle(m_hMapFile);
		m_hMapFile = nullptr;
	}

	if (m_hEvent != nullptr)
	{
		CloseHandle(m_hEvent);
		m_hEvent = nullptr;
	}
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
	while (running && !m_stopThread && m_pSharedMem)
	{
		// Wait for the server to signal that new data is available.
		const DWORD waitResult = WaitForSingleObject(m_hEvent, 30); // 30ms timeout

		if (waitResult != WAIT_OBJECT_0)
			continue; // Timeout or error, loop again.

		// Use a memory barrier to ensure we see the head value that was set *after*
		// the server finished writing the data.
		_ReadBarrier();

		size_t head = m_pSharedMem->head;
		size_t tail = m_pSharedMem->tail;

		while (tail != head)
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
				std::cerr << "Client: Corrupted packet detected (size too large). Flushing buffer.\n";
				m_pSharedMem->tail = head; // Skip all pending data.
				break;
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
	std::cout << "Client worker thread finished.\n";
}

void SharedMemoryClient::ProcessPacket(const PacketHeader &header, const std::byte *data, rlFPCamera &camera)
{
	switch (header.type)
	{
		case PacketType::DRAW_COMMAND:
		{
			if (header.size != sizeof(DrawCommandPacket))
			{
				std::cerr << "Client: Received DRAW_COMMAND with incorrect size. Expected "
						<< sizeof(DrawCommandPacket) << ", got " << header.size << ".\n";
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
				std::cerr << "Client: Received WORLD_UPDATE with incorrect size. Expected "
						<< sizeof(WorldUpdatePacket) << ", got " << header.size << ".\n";
				break;
			}

			const auto &worldUpdate = *reinterpret_cast<const WorldUpdatePacket*>(data);

			if (worldUpdate.curtime < m_currentTime)
			{
				// Server has restarted (Got a lower time then we had previously), clear all previous commands.
				ClearDrawCommands();
			}

			m_currentTime = worldUpdate.curtime;

			ExpireOldCommands();

			rlFPCameraSetPosition(&camera, worldUpdate.origin.ToRayLib());
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
			std::cerr << "Client: Unknown packet type " << static_cast<std::uint8_t>(header.type) << '\n';
			break;
		}
	}
}

std::vector<DrawCommandPacket> SharedMemoryClient::GetDrawCommands()
{
	std::lock_guard lock(m_drawMutex);
	return m_drawCommands;
}

void SharedMemoryClient::ClearDrawCommands()
{
	std::lock_guard lock(m_drawMutex);
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
		              return m_currentTime > cmd.drawEndTime;
	              });
}
