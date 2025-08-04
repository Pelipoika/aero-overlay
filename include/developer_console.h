#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#include "Raylib/raylib.h"

enum class LogLevel : std::uint8_t
{
	INFO,
	WARNING,
	ERROR
};

struct ConsoleMessage
{
	std::string                           text;
	LogLevel                              level;
	std::chrono::steady_clock::time_point timestamp;
	float                                 alpha;  // For fade animation

	ConsoleMessage(std::string msg, const LogLevel lvl) : text(std::move(msg)), level(lvl), timestamp(std::chrono::steady_clock::now()), alpha(1.0f) { }
};

class DeveloperConsole
{
public:
	DeveloperConsole();
	~DeveloperConsole() = default;

	DeveloperConsole(const DeveloperConsole &other)                = delete;
	DeveloperConsole(DeveloperConsole &&other) noexcept            = delete;
	DeveloperConsole &operator=(const DeveloperConsole &other)     = delete;
	DeveloperConsole &operator=(DeveloperConsole &&other) noexcept = delete;

	// Singleton access
	static DeveloperConsole &GetInstance();

	// Add messages to the console
	void AddMessage(const std::string &message, LogLevel level = LogLevel::INFO);
	void AddInfo(const std::string &message);
	void AddWarning(const std::string &message);
	void AddError(const std::string &message);

	// Update console state (call each frame)
	void Update();

	// Render the console overlay
	void Render();

	// Clear all messages
	void Clear();

	// Configuration
	void SetMaxMessages(const size_t max) { m_maxMessages = max; }
	void SetMessageLifetime(const float seconds) { m_messageLifetime = seconds; }
	void SetFadeOutTime(const float seconds) { m_fadeOutTime = seconds; }

private:
	std::vector<ConsoleMessage> m_messages;
	std::mutex                  m_messagesMutex;

	// Configuration
	size_t m_maxMessages     = 10;
	float  m_messageLifetime = 5.0f;  // How long messages stay at full opacity
	float  m_fadeOutTime     = 1.0f;      // How long the fade out animation takes

	int m_lineSpacing = 18;
	int m_marginX     = 20;
	int m_marginY     = 20;

	// Helper functions
	Color GetColorForLevel(LogLevel level, float alpha) const;
	Color GetAccentColorForLevel(LogLevel level, float alpha) const;
	void  RemoveExpiredMessages();
};

#define DEV_LOG_INFO(msg) DeveloperConsole::GetInstance().AddInfo(msg)
#define DEV_LOG_WARNING(msg) DeveloperConsole::GetInstance().AddWarning(msg)
#define DEV_LOG_ERROR(msg) DeveloperConsole::GetInstance().AddError(msg)
