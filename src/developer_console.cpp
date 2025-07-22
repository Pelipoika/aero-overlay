#include "developer_console.h"
#include "overlay_renderer.h"

#include <algorithm>

DeveloperConsole::DeveloperConsole()
{
	// Reserve space for messages to avoid frequent reallocations
	m_messages.reserve(m_maxMessages);
}

DeveloperConsole &DeveloperConsole::GetInstance()
{
	static DeveloperConsole instance;
	return instance;
}

void DeveloperConsole::AddMessage(const std::string &message, LogLevel level)
{
	std::lock_guard<std::mutex> lock(m_messagesMutex);

	// Add new message
	m_messages.emplace_back(message, level);

	// Remove oldest messages if we exceed the limit
	if (m_messages.size() > m_maxMessages)
	{
		m_messages.erase(m_messages.begin(), m_messages.begin() + (m_messages.size() - m_maxMessages));
	}
}

void DeveloperConsole::AddInfo(const std::string &message)
{
	AddMessage(message, LogLevel::INFO);
}

void DeveloperConsole::AddWarning(const std::string &message)
{
	AddMessage(message, LogLevel::WARNING);
}

void DeveloperConsole::AddError(const std::string &message)
{
	AddMessage(message, LogLevel::ERROR);
}

void DeveloperConsole::Update(float deltaTime)
{
	std::lock_guard<std::mutex> lock(m_messagesMutex);

	const auto now = std::chrono::steady_clock::now();

	for (auto &message : m_messages)
	{
		const auto elapsed = std::chrono::duration<float>(now - message.timestamp).count();

		if (elapsed > m_messageLifetime)
		{
			// Start fading out
			const float fadeElapsed = elapsed - m_messageLifetime;
			message.alpha           = std::max(0.0f, 1.0f - (fadeElapsed / m_fadeOutTime));
		}
	}

	// Remove expired messages
	RemoveExpiredMessages();
}

void DeveloperConsole::Render()
{
	std::lock_guard<std::mutex> lock(m_messagesMutex);

	int yOffset = m_marginY;

	for (const auto &message : m_messages)
	{
		if (message.alpha <= 0.0f)
			continue;

		const Color color = GetColorForLevel(message.level, message.alpha);

		// Measure text width using custom font if available
		int textWidth;
		if (OverlayRenderer::IsFontLoaded())
		{
			Vector2 textSize = MeasureTextEx(OverlayRenderer::GetConsolasFont(), message.text.c_str(), static_cast<float>(m_fontSize), 1.0f);
			textWidth        = static_cast<int>(textSize.x);
		}
		else
		{
			textWidth = MeasureText(message.text.c_str(), m_fontSize);
		}

		// Draw semi-transparent dark background with subtle border
		const Color bgColor = {0, 0, 0, static_cast<unsigned char>(255 * message.alpha)};

		// Background rectangle
		DrawRectangle(m_marginX - 4, yOffset - 2, textWidth + 8, m_fontSize + 4, bgColor);

		// Subtle left border (Source Engine style accent)
		const Color accentColor = GetAccentColorForLevel(message.level, message.alpha);
		DrawRectangle(m_marginX - 4, yOffset - 2, 2, m_fontSize + 4, accentColor);

		// Draw the text using custom font if available
		if (OverlayRenderer::IsFontLoaded())
		{
			DrawTextEx(OverlayRenderer::GetConsolasFont(), message.text.c_str(),
			           {static_cast<float>(m_marginX), static_cast<float>(yOffset)},
			           static_cast<float>(m_fontSize), 1.0f, color);
		}
		else
		{
			DrawText(message.text.c_str(), m_marginX, yOffset, m_fontSize, color);
		}

		yOffset += m_lineSpacing;
	}
}

void DeveloperConsole::Clear()
{
	std::lock_guard<std::mutex> lock(m_messagesMutex);
	m_messages.clear();
}

Color DeveloperConsole::GetColorForLevel(const LogLevel level, const float alpha) const
{
	const unsigned char alphaValue = static_cast<unsigned char>(255 * alpha);

	switch (level)
	{
		case LogLevel::INFO:
			return Color{220, 220, 220, alphaValue}; // Light gray (Source Engine style)
		case LogLevel::WARNING:
			return Color{255, 200, 50, alphaValue};  // Orange-yellow
		case LogLevel::ERROR:
			return Color{255, 80, 80, alphaValue};   // Light red
		default:
			return Color{220, 220, 220, alphaValue}; // Default to light gray
	}
}

Color DeveloperConsole::GetAccentColorForLevel(const LogLevel level, const float alpha) const
{
	const unsigned char alphaValue = static_cast<unsigned char>(255 * alpha);

	switch (level)
	{
		case LogLevel::INFO:
			return Color{100, 150, 255, alphaValue}; // Blue accent
		case LogLevel::WARNING:
			return Color{255, 150, 0, alphaValue};   // Orange accent
		case LogLevel::ERROR:
			return Color{255, 50, 50, alphaValue};   // Red accent
		default:
			return Color{100, 150, 255, alphaValue}; // Default blue
	}
}

void DeveloperConsole::RemoveExpiredMessages()
{
	std::erase_if(m_messages,
	              [this](const ConsoleMessage &msg){
		              const auto now     = std::chrono::steady_clock::now();
		              const auto elapsed = std::chrono::duration<float>(now - msg.timestamp).count();
		              return elapsed > (m_messageLifetime + m_fadeOutTime);
	              });
}
