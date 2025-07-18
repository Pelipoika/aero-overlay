#pragma once

#include <cstdint>

#include "raylib.h"
#include "win32_minimal.h"

// Not packed.
struct Vector
{
	float x, y, z;

	[[nodiscard]] Vector3 ToRayLib() const
	{
		return {y, z, x};
	}
};

struct QAngle
{
	float x, y, z;
};

// Names for the Windows synchronization and memory objects.
constexpr auto SHARED_MEM_NAME = L"CS2DebugOverlay_SharedMem";
constexpr auto EVENT_NAME      = L"CS2DebugOverlay_NewDataEvent";

// The size of the circular buffer in shared memory.
// Must be a power of 2 for efficient bitwise arithmetic on head/tail indices.
constexpr size_t SHARED_MEM_BUFFER_SIZE = 2048 * 2048; // 1MB

// --- Packet Definitions ---
#pragma pack(push, 1)

enum class DrawCommandType : std::uint8_t
{
	LINE,
	TEXT,
};

struct LineCommandData
{
	Vector start;
	Vector end;
};

struct TextCommandData
{
	Vector position;
	bool   onscreen;
	char   text[128];
};

struct DrawCommandPacket
{
	DrawCommandType type;
	Color           color;
	float           drawEndTime;

	union
	{
		LineCommandData line;
		TextCommandData text;
	};
};

struct WorldUpdatePacket
{
	QAngle viewAngles;
	Vector origin;
	float  curtime;
};

enum class PacketType : std::uint8_t
{
	WORLD_UPDATE,
	DRAW_COMMAND,
	CLEAR_ALL_DRAWINGS
};

// A header that precedes every packet in the buffer.
struct PacketHeader
{
	PacketType type;
	uint32_t   size; // The size of the data that follows this header
};

// --- Shared Memory Layout ---

// This is the structure that will be mapped into both processes.
// It contains the head/tail for the circular buffer and the buffer itself.
struct SharedMemoryLayout
{
	// The head is the index where the server will write the next packet.
	// It is only ever written to by the server.
	alignas(64) volatile long head;

	// The tail is the index where the client will read the next packet.
	// It is only ever written to by the client.
	alignas(64) volatile long tail;

	// The data buffer.
	BYTE buffer[SHARED_MEM_BUFFER_SIZE];
};

#pragma pack(pop)
