#include "glpch.h"
#include "FileWatcher.h"

namespace gl {

	FileWatcher::FileWatcher(
		const std::filesystem::path& path,
		std::chrono::milliseconds debounce)
		: m_Path(path), m_Debounce(debounce)
	{
		Reset();
	}

	void FileWatcher::Reset()
	{
		std::error_code error;
		m_LastWriteTime = std::filesystem::last_write_time(m_Path, error);
		m_PendingWriteTime = {};
		m_HasPendingChange = false;
	}

	bool FileWatcher::Poll()
	{
		std::error_code error;
		const auto currentWriteTime = std::filesystem::last_write_time(m_Path, error);
		if (error)
			return false;

		if (currentWriteTime == m_LastWriteTime)
		{
			m_HasPendingChange = false;
			return false;
		}

		const auto now = std::chrono::steady_clock::now();
		if (!m_HasPendingChange || currentWriteTime != m_PendingWriteTime)
		{
			m_PendingWriteTime = currentWriteTime;
			m_PendingSince = now;
			m_HasPendingChange = true;
			return false;
		}

		if (now - m_PendingSince < m_Debounce)
			return false;

		m_LastWriteTime = currentWriteTime;
		m_HasPendingChange = false;
		return true;
	}

}
