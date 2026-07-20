#include "ContentBrowserPanel.h"
#include <imgui.h>

#define ICON_FA_FOLDER   "\xef\x81\xbb" // 
#define ICON_FA_CODE     "\xef\x87\x89" // 
#define ICON_FA_CUBE     "\xef\x86\xb2" // 
#define ICON_FA_IMAGE    "\xef\x80\xbe" // 
#define ICON_FA_GLOBE    "\xef\x82\xac" // 
#define ICON_FA_FILE     "\xef\x85\x9b" // 

namespace gl {

	ContentBrowserPanel::ContentBrowserPanel() = default;

	static void LazyInit(std::filesystem::path& base, std::filesystem::path& cur)
	{
		if (base.empty())
		{
			base = std::filesystem::absolute("assets");
			cur = base;
		}
	}

	static const char* GetIcon(const std::filesystem::path& path)
	{
		if (std::filesystem::is_directory(path)) return ICON_FA_FOLDER;
		auto ext = path.extension().string();
		if (ext == ".glsl")      return ICON_FA_CODE;
		if (ext == ".glimmer")   return ICON_FA_GLOBE;
		if (ext == ".obj")       return ICON_FA_CUBE;
		if (ext == ".png" || ext == ".jpg") return ICON_FA_IMAGE;
		return ICON_FA_FILE;
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		LazyInit(m_BaseDir, m_CurrentDir);

		ImGui::Begin("Content Browser");

		if (ImGui::Button(" " ICON_FA_FOLDER " ..") && m_CurrentDir != m_BaseDir)
			m_CurrentDir = m_CurrentDir.parent_path();

		ImGui::SameLine();
		auto rel = std::filesystem::relative(m_CurrentDir, m_BaseDir);
		ImGui::TextDisabled("assets/%s", rel.string().c_str());

		ImGui::Separator();

		float cellSize = 80.0f;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columns = std::max(1, (int)(panelWidth / cellSize));
		ImGui::Columns(columns, nullptr, false);

		for (auto& entry : std::filesystem::directory_iterator(m_CurrentDir))
		{
			const auto& path = entry.path();
			std::string name = path.filename().string();
			bool isDir = entry.is_directory();

			// 截断过长文件名
			std::string label = name;
			if (label.size() > 10) label = label.substr(0, 9) + "...";

			const char* icon = GetIcon(path);
			std::string display = std::string(icon) + " " + label;

			bool isSelected = (m_SelectedFile == path.string());
			ImGui::PushID(name.c_str());

			if (ImGui::Selectable(display.c_str(), &isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(cellSize, cellSize)))
			{
				m_SelectedFile = path.string();
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (isDir)
					{
						m_CurrentDir = path;
						ImGui::PopID();
						ImGui::Columns(1);
						ImGui::End();
						return;
					}
					else if (OnFileDoubleClicked)
					{
						OnFileDoubleClicked(path.string());
					}
				}
			}

			// 文件可拖拽到视口打开
			if (!isDir && ImGui::BeginDragDropSource())
			{
				std::string absPath = path.string();
				ImGui::SetDragDropPayload("SCENE_FILE", absPath.c_str(), absPath.size() + 1);
				ImGui::Text("Open %s", name.c_str());
				ImGui::EndDragDropSource();
			}

			ImGui::PopID();
			ImGui::NextColumn();
		}

		ImGui::Columns(1);
		ImGui::End();
	}

}
