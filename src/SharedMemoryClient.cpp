#include "SharedMemoryClient.h"
#include <iostream>
#include <algorithm>
#include <chrono>

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
		DWORD error = GetLastError();
		std::cerr << "Client: OpenEvent failed, GLE=" << error;
		switch (error)
		{
			case ERROR_FILE_NOT_FOUND:
				std::cerr << " (Event not found - server may not be running)";
				break;
			case ERROR_ACCESS_DENIED:
				std::cerr << " (Access denied - insufficient permissions)";
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
		DWORD error = GetLastError();
		std::cerr << "Client: OpenFileMapping failed, GLE=" << error;
		switch (error)
		{
			case ERROR_FILE_NOT_FOUND:
				std::cerr << " (Shared memory not found - server may not be running)";
				break;
			case ERROR_ACCESS_DENIED:
				std::cerr << " (Access denied - insufficient permissions)";
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
	                                                              0
	                                                             ));

	if (m_pSharedMem == nullptr)
	{
		DWORD error = GetLastError();
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

void SharedMemoryClient::ClientThreadWorker(const std::atomic<bool> &running, rlFPCamera &camera)
{
	while (running && !m_stopThread)
	{
		// Wait for the server to signal that new data is available.
		// Use a timeout to periodically check the 'running' flag.
		DWORD waitResult = WaitForSingleObject(m_hEvent, 100); // 100ms timeout

		if (waitResult == WAIT_OBJECT_0)
		{
			// Event was signaled, process all available data.
			long head = m_pSharedMem->head;
			long tail = m_pSharedMem->tail;

			while (tail != head)
			{
				_ReadBarrier(); // Ensure we read the header after data is written.

				// Read packet header
				PacketHeader header;
				memcpy(&header, m_pSharedMem->buffer + tail, sizeof(PacketHeader));

				long dataStart = (tail + sizeof(PacketHeader)) & (SHARED_MEM_BUFFER_SIZE - 1);

				// Read packet data
				std::vector<BYTE> dataBuffer(header.size);
				if (header.size > 0)
				{
					if (dataStart + header.size > SHARED_MEM_BUFFER_SIZE)
					{
						// Data wraps around the buffer
						long firstPartSize = SHARED_MEM_BUFFER_SIZE - dataStart;
						memcpy(dataBuffer.data(), m_pSharedMem->buffer + dataStart, firstPartSize);
						memcpy(dataBuffer.data() + firstPartSize, m_pSharedMem->buffer, header.size - firstPartSize);
					}
					else
					{
						// Data is contiguous
						memcpy(dataBuffer.data(), m_pSharedMem->buffer + dataStart, header.size);
					}
				}

				// Process the packet we just read
				ProcessPacket(header.type, dataBuffer.data(), header.size, camera);

				// Atomically update the tail to release the space back to the server.
				tail               = (tail + sizeof(PacketHeader) + header.size) & (SHARED_MEM_BUFFER_SIZE - 1);
				m_pSharedMem->tail = tail;
			}
		}
	}
	std::cout << "Client worker thread finished.\n";
}

void SharedMemoryClient::ProcessPacket(PacketType type, const BYTE *data, uint32_t size, rlFPCamera &camera)
{
	switch (type)
	{
		case PacketType::DRAW_COMMAND:
		{
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
			const auto &worldUpdate = *reinterpret_cast<const WorldUpdatePacket*>(data);
			m_currentTime           = worldUpdate.curtime;
			ExpireOldCommands();

			rlFPCameraSetPosition(&camera, worldUpdate.origin.ToRayLib());
			camera.ViewAngles = {
				(-worldUpdate.viewAngles.y) * DEG2RAD,
				worldUpdate.viewAngles.x * DEG2RAD,
			};
			break;
		}
		case PacketType::CLEAR_ALL_DRAWINGS:
		{
			ClearDrawCommands();
			break;
		}
		default:
		{
			fprintf(stderr, "Client: Unknown packet type %u\n", static_cast<std::uint8_t>(type));
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
	m_drawCommands.erase(std::remove_if(m_drawCommands.begin(), m_drawCommands.end(),
	                                    [this](const DrawCommandPacket &cmd){
		                                    return m_currentTime > cmd.drawEndTime;
	                                    }),
	                     m_drawCommands.end());
}
