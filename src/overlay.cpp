#if defined(_WIN32)
// To avoid conflicting windows.h symbols with raylib, some flags are defined
// WARNING: Those flags avoid inclusion of some Win32 headers that could be required
// by user at some point and won't be included...
//-------------------------------------------------------------------------------------

// If defined, the following flags inhibit definition of the indicated items.
#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // VK_*
#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS      // SM_*
#define NOMENUS           // MF_*
#define NOICONS           // IDI_*
#define NOKEYSTATES       // MK_*
#define NOSYSCOMMANDS     // SC_*
#define NORASTEROPS       // Binary and Tertiary raster ops
#define NOSHOWWINDOW      // SW_*
#define OEMRESOURCE       // OEM Resource values
#define NOATOM            // Atom Manager routines
#define NOCLIPBOARD       // Clipboard routines
#define NOCOLOR           // Screen colors
#define NOCTLMGR          // Control and Dialog routines
#define NODRAWTEXT        // DrawText() and DT_*
#define NOGDI             // All GDI defines and routines
#define NOKERNEL          // All KERNEL defines and routines
#define NOUSER            // All USER defines and routines
#define NONLS             // All NLS defines and routines
#define NOMB              // MB_* and MessageBox()
#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // typedef METAFILEPICT
#define NOMINMAX          // Macros min(a,b) and max(a,b)
#define NOMSG             // typedef MSG and associated routines
#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL          // SB_* and scrolling routines
#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // Sound driver routines
#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
#define NOWH              // SetWindowsHook and WH_*
#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
#define NOCOMM            // COMM driver routines
#define NOKANJI           // Kanji support stuff.
#define NOHELP            // Help engine interface.
#define NOPROFILER        // Profiler interface.
#define NODEFERWINDOWPOS  // DeferWindowPos routines
#define NOMCX             // Modem Configuration Extensions
#define NOPLAYSOUND

// Type required before windows.h inclusion
typedef struct tagMSG *LPMSG;

#include <windows.h>

#undef PlaySound // Avoid conflict with PlaySound() function defined in windows.h

// Type required by some unused function...
typedef struct tagBITMAPINFOHEADER
{
	DWORD biSize;
	LONG  biWidth;
	LONG  biHeight;
	WORD  biPlanes;
	WORD  biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG  biXPelsPerMeter;
	LONG  biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

#include <objbase.h>

// Some required types defined for MSVC/TinyC compiler
#if defined(_MSC_VER) || defined(__TINYC__)
#endif
#endif

#include <cstdint>
#include <mutex>
#include <vector>
#include "rlFPSCamera.h"

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
	Color           color; // Raylib color
	float           drawDuration;

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

// Windows.h RaySlop...
extern "C" HWND WINAPI FindWindowA(LPCSTR lpClassName, LPCSTR lpWindowName);
extern "C" BOOL WINAPI GetWindowRect(HWND hWnd, LPRECT lpRect);

// Global list of commands to draw and a mutex to protect it.
static inline std::vector<DrawCommandPacket> g_drawCommands;
static inline std::mutex                     g_drawMutex;

int main()
{
	const HWND CS2_Window = FindWindowA(nullptr, "Counter-Strike 2");

	if (!CS2_Window)
		return -1;

	RECT cs2_rect;
	if (!GetWindowRect(CS2_Window, &cs2_rect))
		return -2;

	const int overlay_w = cs2_rect.right - cs2_rect.left;
	const int overlay_h = cs2_rect.bottom - cs2_rect.top;

	SetConfigFlags(FLAG_WINDOW_TRANSPARENT | FLAG_WINDOW_MOUSE_PASSTHROUGH | FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST | FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_ALWAYS_RUN);
	InitWindow(overlay_w, overlay_h, "DebugOverlay");

	SetWindowSize(overlay_w, overlay_h);
	SetWindowPosition(cs2_rect.left, cs2_rect.top);

	SetTargetFPS(144);

	rlFPCamera cam;
	rlFPCameraInit(&cam, 90, {0, 0, 0});

	std::atomic<bool> running{true};

	std::thread packetThread([&running, &cam](){
		std::vector<uint8_t> buffer(PIPE_BUFFER_SIZE);

		while (running)
		{
			const HANDLE hPipe = CreateFileA(
			                                 PIPE_NAME,
			                                 GENERIC_READ,
			                                 0,
			                                 nullptr,
			                                 OPEN_EXISTING,
			                                 0,
			                                 nullptr
			                                );

			if (hPipe != INVALID_HANDLE_VALUE)
			{
				printf("Client: Sucacessfully connected to the pipe.\n");
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

						const Packet *pkt = reinterpret_cast<const Packet*>(buffer.data() + offset);

						switch (pkt->type)
						{
							case PacketType::DRAW_COMMAND:
							{
								if (dwRead - offset < sizeof(Packet))
									break;

								std::lock_guard lock(g_drawMutex);
								if (g_drawCommands.size() > 200)
									g_drawCommands.pop_back();

								g_drawCommands.push_back(pkt->drawCommand);
								break;
							}
							case PacketType::WORLD_UPDATE:
							{
								if (dwRead - offset < sizeof(Packet))
									break;

								const WorldUpdatePacket &worldUpdate = pkt->worldUpdate;
								printf("viewAngles %.2f %.2f origin %.2f %.2f %.2f\n",
								       worldUpdate.viewAngles.x, worldUpdate.viewAngles.y,
								       worldUpdate.origin.x, worldUpdate.origin.y, worldUpdate.origin.z);

								rlFPCameraSetPosition(&cam, worldUpdate.origin.ToRayLib());

								cam.ViewAngles = {
									(-worldUpdate.viewAngles.y) * DEG2RAD,
									worldUpdate.viewAngles.x * DEG2RAD,
								};

								//camera
								break;
							}
							case PacketType::CLEAR_ALL_DRAWINGS:
							{
								std::lock_guard lock(g_drawMutex);
								g_drawCommands.clear();
								break;
							}
							default:
							{
								fprintf(stderr, "Client: Unknown packet type %u\n", static_cast<std::uint8_t>(pkt->type));
								break;
							}
						}

						offset += sizeof(Packet);
					}

					std::this_thread::sleep_for(std::chrono::milliseconds(2));
				}
				CloseHandle(hPipe);
			}
			else
			{
				fprintf(stderr, "Client: Could not open pipe, GLE=%lu. Retrying in 2 seconds...\n", GetLastError());
				std::this_thread::sleep_for(std::chrono::seconds(2));
			}
		}
	});

	while (!WindowShouldClose())
	{
		rlFPCameraUpdate(&cam);

		BeginDrawing();

		ClearBackground(BLANK);

		rlFPCameraBeginMode3D(&cam);
		{
			// Draw everything in the command list.
			std::lock_guard lock(g_drawMutex);
			for (const auto &cmd : g_drawCommands)
			{
				if (cmd.type == DrawCommandType::LINE)
				{
					DrawLine3D(cmd.line.start.ToRayLib(), cmd.line.end.ToRayLib(), cmd.color);
				}
				else if (cmd.type == DrawCommandType::TEXT)
				{
					// NOTE: raylib's DrawText is 2D. For 3D text, you'd use DrawText3D
					// which requires a Font. For simplicity, we'll use DrawBillboard.

					//GetWorldToScreen()

					if (cmd.text.onscreen)
					{
						DrawText("Congrats! You created your first window!", (int)cmd.text.position.x, (int)cmd.text.position.y, 20, LIGHTGRAY);
					}
					else
					{
						Vector2 screenPos = GetWorldToScreen(cmd.text.position.ToRayLib(), cam.ViewCamera);

						DrawText("Congrats! You created your first window!", (int)screenPos.x, (int)screenPos.y, 20, LIGHTGRAY);
					}
				}
			}

			DrawCylinder({-1114, -245, -1215}, 20, 20, 100, 10, GREEN);

			//DrawPlane({0, 0, 0}, {5000, 5000}, CLITERAL(Color){ 200, 200, 200, 55 });
			//DrawGrid(100, 1.0f); // Draw a grid for context
		}
		rlFPCameraEndMode3D();

		// Instead, draw a fully transparent rectangle over the window
		//DrawRectangleLines(1, 1, GetScreenWidth() - 2, GetScreenHeight() - 2, LIGHTGRAY);

		//WorldTo
		DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);

		DrawFPS(100, 100);

		EndDrawing();
	}

	CloseWindow();

	return 0;
}
