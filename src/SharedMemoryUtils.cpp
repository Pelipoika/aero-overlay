#include "SharedMemoryUtils.h"
#include "developer_console.h"
#include <sstream>

namespace SharedMemoryErrors
{
	std::string FormatConnectionError(const std::string &operation, const unsigned long errorCode)
	{
		std::ostringstream oss;
		oss << operation << " failed with error " << errorCode;

		switch (errorCode)
		{
			case 2: // ERROR_FILE_NOT_FOUND
				oss << " (Resource not found - server may not be running)";
				break;
			case 5: // ERROR_ACCESS_DENIED
				oss << " (Access denied - insufficient permissions)";
				break;
			case 87: // ERROR_INVALID_PARAMETER
				oss << " (Invalid parameter)";
				break;
			case 8: // ERROR_NOT_ENOUGH_MEMORY
				oss << " (Not enough memory)";
				break;
			case 183: // ERROR_ALREADY_EXISTS
				oss << " (Resource already exists)";
				break;
			case 122: // ERROR_INSUFFICIENT_BUFFER
				oss << " (Insufficient buffer size)";
				break;
			default:
				oss << " (Unknown system error)";
				break;
		}

		return oss.str();
	}

	std::string FormatBufferError(const std::string &context, const size_t value)
	{
		std::ostringstream oss;
		oss << context << " - Invalid value: " << value << " (max allowed: " << SHARED_MEM_BUFFER_SIZE << ")";
		return oss.str();
	}

	std::string FormatTimeoutError(const std::string &context, const std::chrono::milliseconds timeout)
	{
		std::ostringstream oss;
		oss << context << " - Timeout after " << timeout.count() << "ms";
		return oss.str();
	}

	void LogError(const ErrorInfo &error)
	{
		switch (error.severity)
		{
			case ErrorSeverity::Info:
				DEV_LOG_INFO(error.message);
				break;
			case ErrorSeverity::Warning:
				DEV_LOG_WARNING(error.message);
				break;
			case ErrorSeverity::Error:
			case ErrorSeverity::Fatal:
				DEV_LOG_ERROR(error.message);
				break;
		}
	}
}
