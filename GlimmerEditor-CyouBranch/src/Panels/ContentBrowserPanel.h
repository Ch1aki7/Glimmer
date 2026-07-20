#pragma once
#include "Glimmer.h"
#include <functional>
#include <filesystem>

namespace gl {

	class ContentBrowserPanel {
	public:
		ContentBrowserPanel();

		void OnImGuiRender();

		std::function<void(const std::string& path)> OnFileDoubleClicked;

	private:
		std::string GetFileIcon(const std::string& ext) const;

		std::filesystem::path m_BaseDir;
		std::filesystem::path m_CurrentDir;
		std::string m_SelectedFile;
	};

}
