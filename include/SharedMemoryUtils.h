#pragma once

#include <string>
#include <chrono>
#include "SharedDefs.h"

// Centralized constants for the shared memory client
namespace SharedMemoryConstants
{
	// Buffer and timing constants
	constexpr size_t        MAX_DRAW_COMMANDS    = 2000;
	constexpr auto          WORLD_UPDATE_TIMEOUT = std::chrono::seconds(5);
	constexpr unsigned long WAIT_TIMEOUT_MS      = 30;

	// Time jump threshold for session detection
	constexpr float TIME_JUMP_THRESHOLD = 1.0f;

	// Buffer validation constants
	constexpr size_t MAX_REASONABLE_PACKET_SIZE = SHARED_MEM_BUFFER_SIZE / 4; // Don't allow packets larger than 1/4 buffer
}

// Error handling utilities
namespace SharedMemoryErrors
{
	// Common error message formatting
	std::string FormatConnectionError(const std::string &operation, unsigned long errorCode);
	std::string FormatBufferError(const std::string &context, size_t value);
	std::string FormatTimeoutError(const std::string &context, std::chrono::milliseconds timeout);

	// Error classification
	enum class ErrorSeverity : std::uint8_t
	{
		Info,    // Informational, normal operation
		Warning, // Warning, recoverable error
		Error,   // Error, operation failed but can continue
		Fatal    // Fatal, unrecoverable error requiring restart
	};

	struct ErrorInfo
	{
		ErrorSeverity severity;
		std::string   message;
		unsigned long systemError;

		ErrorInfo(const ErrorSeverity sev, std::string msg, const unsigned long sysErr = 0) : severity(sev), message(std::move(msg)), systemError(sysErr) { }
	};

	// Log error with appropriate severity
	void LogError(const ErrorInfo &error);
}
