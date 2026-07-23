#pragma once

#include <chrono>
#include <filesystem>

namespace gl {

	class FileWatcher {
	public:
		explicit FileWatcher(
			const std::filesystem::path& path,
			std::chrono::milliseconds debounce = std::chrono::milliseconds(200));

		bool Poll();
		void Reset();

		const std::filesystem::path& GetPath() const { return m_Path; }

	private:
		std::filesystem::path m_Path;
		std::filesystem::file_time_type m_LastWriteTime{};
		std::filesystem::file_time_type m_PendingWriteTime{};
		std::chrono::steady_clock::time_point m_PendingSince{};
		std::chrono::milliseconds m_Debounce;
		bool m_HasPendingChange = false;
	};

}
