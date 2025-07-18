#pragma once

#include <cstdint>
#include <win32_minimal.h>

#include "Raylib/raylib.h"

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
constexpr size_t SHARED_MEM_BUFFER_SIZE = 2048 * 2048; // 4MB

// --- Packet Definitions ---
#pragma pack(push, 1)

enum class DrawCommandType : std::uint8_t
{
	LINE,
	BBOX,
	TEXT,
};

struct LineCommandData
{
	LineCommandData(const Vector &start, const Vector &end) : start(start), end(end) { }

	Vector start;
	Vector end;
};

struct BBoxCommandData
{
	BBoxCommandData(const Vector &mins, const Vector &maxs) : mins(mins), maxs(maxs) { }

	Vector mins;
	Vector maxs;
};

struct TextCommandData
{
	explicit TextCommandData(const Vector &position, const char *msg) : position(position), onscreen(false)
	{
		strncpy_s(text, msg, sizeof(text) - 1);
		text[sizeof(text) - 1] = '\0'; // Ensure null-termination
	}

	Vector position;
	bool   onscreen;
	char   text[128];
};

struct DrawCommandPacket
{
	// The server is now responsible for calculating the `drawEndTime`. This improves decoupling.
	DrawCommandPacket(DrawCommandType type, const Color &color, float drawEndTime, const LineCommandData &line) : type(type), color(color), drawEndTime(drawEndTime), line(line) { }
	DrawCommandPacket(DrawCommandType type, const Color &color, float drawEndTime, const BBoxCommandData &box) : type(type), color(color), drawEndTime(drawEndTime), box(box) { }
	DrawCommandPacket(DrawCommandType type, const Color &color, float drawEndTime, const TextCommandData &text) : type(type), color(color), drawEndTime(drawEndTime), text(text) { }

	DrawCommandType type;
	Color           color;
	float           drawEndTime;

	union
	{
		LineCommandData line;
		BBoxCommandData box;
		TextCommandData text;
	};
};

struct WorldUpdatePacket
{
	WorldUpdatePacket(const QAngle &view_angles, const Vector &origin, float curtime) : viewAngles(view_angles), origin(origin), curtime(curtime) { }

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
	alignas(64) volatile size_t head;

	// The tail is the index where the client will read the next packet.
	// It is only ever written to by the client.
	alignas(64) volatile size_t tail;

	// The data buffer.
	// Uses std::byte for type-safety when dealing with raw memory.
	std::byte buffer[SHARED_MEM_BUFFER_SIZE];
};

#pragma pack(pop)
