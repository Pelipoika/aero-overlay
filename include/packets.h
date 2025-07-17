#pragma once

#include <cstdint>
#include "raylib.h"

#pragma pack(push, 1)

enum class DrawCommandType : std::uint8_t
{
	LINE,
	TEXT,
};

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

struct Packet
{
	PacketType type;

	union
	{
		WorldUpdatePacket worldUpdate;
		DrawCommandPacket drawCommand;
	};
};

#pragma pack(pop)

constexpr auto PIPE_NAME        = R"(\\.\pipe\CS2DebugOverlay)";
constexpr auto PIPE_BUFFER_SIZE = sizeof(Packet);
