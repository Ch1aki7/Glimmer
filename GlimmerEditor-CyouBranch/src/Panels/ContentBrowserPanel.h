#pragma once
#include "Glimmer.h"
#include <functional>
#include <filesystem>

namespace gl {

	class ContentBrowserPanel {
	public:
		ContentBrowserPanel();

		void OnImGuiRender();

		std::function<void(AssetHandle)> OnAssetSelected;
		std::function<void(const std::string& path)> OnFileDoubleClicked;

	private:
		void DrawDirectoryTree(const std::filesystem::path& dir);
		void DrawCreateContextMenu();

		std::filesystem::path m_BaseDir;
		std::filesystem::path m_CurrentDir;
		std::string m_SelectedFile;
		float m_SplitPos = 200.0f;  // 左侧树宽度
	};

}
