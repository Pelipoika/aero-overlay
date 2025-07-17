#include "pipe_client.h"
#include "config.h"
#include <iostream>
#include <chrono>
#include <cstdio>
#include <algorithm>

PipeClient::PipeClient() : m_currentTime(0.0f) { }

PipeClient::~PipeClient()
{
	Stop();
}

bool PipeClient::Start(std::atomic<bool> &running, rlFPCamera &camera)
{
	try
	{
		m_packetThread = std::thread(&PipeClient::PacketThreadWorker, this, std::ref(running), std::ref(camera));
		return true;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Failed to start packet thread: " << e.what() << '\n';
		return false;
	}
}

void PipeClient::Stop()
{
	if (m_packetThread.joinable())
	{
		m_packetThread.join();
	}
}

std::vector<DrawCommandPacket> PipeClient::GetDrawCommands()
{
	std::lock_guard lock(m_drawMutex);
	return m_drawCommands;
}

void PipeClient::ClearDrawCommands()
{
	std::lock_guard lock(m_drawMutex);
	m_drawCommands.clear();
}

void PipeClient::ExpireOldCommands()
{
	std::lock_guard lock(m_drawMutex);

	// Remove expired draw commands using std::remove_if and erase
	m_drawCommands.erase(std::remove_if(m_drawCommands.begin(), m_drawCommands.end(),
	                                    [this](const DrawCommandPacket &cmd){
		                                    return m_currentTime > cmd.drawEndTime;
	                                    }),
	                     m_drawCommands.end());
}

void PipeClient::PacketThreadWorker(const std::atomic<bool> &running, rlFPCamera &camera)
{
	std::vector<uint8_t> buffer(PIPE_BUFFER_SIZE);

	while (running)
	{
		const HANDLE hPipe = CreateFileA(
		                                 Config::PIPE_NAME,
		                                 GENERIC_READ,
		                                 0,
		                                 nullptr,
		                                 OPEN_EXISTING,
		                                 0,
		                                 nullptr
		                                );

		if (hPipe != INVALID_HANDLE_VALUE)
		{
			printf("Client: Successfully connected to the pipe.\n");
			printf("Client: Waiting to receive messages...\n");

			while (running)
			{
				DWORD      dwRead   = 0;
				const BOOL fSuccess = ReadFile(
				                               hPipe,
				                               buffer.data(),
				                               static_cast<DWORD>(buffer.size()),
				                               &dwRead,
				                               nullptr
				                              );

				if (!fSuccess || dwRead == 0)
				{
					if (GetLastError() == ERROR_BROKEN_PIPE)
					{
						printf("Client: Server disconnected. The pipe is broken.\n");
					}
					else
					{
						fprintf(stderr, "Client: ReadFile failed, GLE=%lu\n", GetLastError());
					}
					break;
				}

				size_t offset = 0;
				while (offset < dwRead)
				{
					if (dwRead - offset < sizeof(PacketType))
						break;

					auto pkt = reinterpret_cast<const Packet*>(buffer.data() + offset);
					ProcessPacket(pkt, camera);
					offset += sizeof(Packet);
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(Config::PIPE_READ_DELAY_MS));
			}
			CloseHandle(hPipe);
		}
		else
		{
			fprintf(stderr, "Client: Could not open pipe, GLE=%lu. Retrying in %d seconds...\n",
			        GetLastError(), Config::PIPE_RETRY_DELAY_SECONDS);
			std::this_thread::sleep_for(std::chrono::seconds(Config::PIPE_RETRY_DELAY_SECONDS));
		}
	}
}

void PipeClient::ProcessPacket(const Packet *packet, rlFPCamera &camera)
{
	switch (packet->type)
	{
		case PacketType::DRAW_COMMAND:
		{
			std::lock_guard lock(m_drawMutex);
			if (m_drawCommands.size() > Config::MAX_DRAW_COMMANDS)
				m_drawCommands.pop_back();

			m_drawCommands.push_back(packet->drawCommand);
			break;
		}
		case PacketType::WORLD_UPDATE:
		{
			const WorldUpdatePacket &worldUpdate = packet->worldUpdate;

			// Store the current time and expire old commands
			m_currentTime = worldUpdate.curtime;
			ExpireOldCommands();

			//printf("viewAngles %.2f %.2f origin %.2f %.2f %.2f\n",
			//worldUpdate.viewAngles.x, worldUpdate.viewAngles.y,
			//worldUpdate.origin.x, worldUpdate.origin.y, worldUpdate.origin.z);

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
			fprintf(stderr, "Client: Unknown packet type %u\n", static_cast<std::uint8_t>(packet->type));
			break;
		}
	}
}
