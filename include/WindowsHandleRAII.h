#pragma once

#include "win32_minimal.h"

// RAII wrapper for Windows handles to ensure proper cleanup
class WindowsHandleRAII
{
public:
	// Default constructor creates invalid handle
	WindowsHandleRAII() noexcept : m_handle(nullptr) { }

	// Constructor taking ownership of a handle
	explicit WindowsHandleRAII(const HANDLE handle) noexcept : m_handle(handle) { }

	// Move constructor
	WindowsHandleRAII(WindowsHandleRAII &&other) noexcept : m_handle(other.m_handle)
	{
		other.m_handle = nullptr;
	}

	// Move assignment operator
	WindowsHandleRAII &operator=(WindowsHandleRAII &&other) noexcept
	{
		if (this != &other)
		{
			reset();
			m_handle       = other.m_handle;
			other.m_handle = nullptr;
		}
		return *this;
	}

	// Destructor automatically closes handle
	~WindowsHandleRAII() noexcept
	{
		reset();
	}

	// Delete copy operations to prevent accidental duplication
	WindowsHandleRAII(const WindowsHandleRAII &)            = delete;
	WindowsHandleRAII &operator=(const WindowsHandleRAII &) = delete;

	// Reset the handle (closes current and optionally assigns new)
	void reset(const HANDLE newHandle = nullptr) noexcept
	{
		if (m_handle && m_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_handle);
		}
		m_handle = newHandle;
	}

	// Release ownership of the handle without closing it
	HANDLE release() noexcept
	{
		const HANDLE temp = m_handle;
		m_handle    = nullptr;
		return temp;
	}

	// Get the handle value
	HANDLE get() const noexcept
	{
		return m_handle;
	}

	// Check if handle is valid
	bool is_valid() const noexcept
	{
		return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
	}

	// Implicit conversion to HANDLE for API calls
	operator HANDLE() const noexcept
	{
		return m_handle;
	}

	// Boolean conversion for validity checks
	explicit operator bool() const noexcept
	{
		return is_valid();
	}

private:
	HANDLE m_handle;
};

// Specialized RAII wrapper for shared memory view
class SharedMemoryViewRAII
{
public:
	SharedMemoryViewRAII() noexcept : m_ptr(nullptr) { }

	explicit SharedMemoryViewRAII(void *ptr) noexcept : m_ptr(ptr) { }

	SharedMemoryViewRAII(SharedMemoryViewRAII &&other) noexcept : m_ptr(other.m_ptr)
	{
		other.m_ptr = nullptr;
	}

	SharedMemoryViewRAII &operator=(SharedMemoryViewRAII &&other) noexcept
	{
		if (this != &other)
		{
			reset();
			m_ptr       = other.m_ptr;
			other.m_ptr = nullptr;
		}
		return *this;
	}

	~SharedMemoryViewRAII() noexcept
	{
		reset();
	}

	// Delete copy operations
	SharedMemoryViewRAII(const SharedMemoryViewRAII &)            = delete;
	SharedMemoryViewRAII &operator=(const SharedMemoryViewRAII &) = delete;

	void reset(void *newPtr = nullptr) noexcept
	{
		if (m_ptr)
		{
			UnmapViewOfFile(m_ptr);
		}
		m_ptr = newPtr;
	}

	void *release() noexcept
	{
		void *temp = m_ptr;
		m_ptr      = nullptr;
		return temp;
	}

	void *get() const noexcept
	{
		return m_ptr;
	}

	bool is_valid() const noexcept
	{
		return m_ptr != nullptr;
	}

	template <typename T>
	T *as() const noexcept
	{
		return static_cast<T*>(m_ptr);
	}

	explicit operator bool() const noexcept
	{
		return is_valid();
	}

private:
	void *m_ptr;
};
