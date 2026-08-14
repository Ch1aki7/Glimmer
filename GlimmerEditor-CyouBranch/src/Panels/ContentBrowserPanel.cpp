#include "ContentBrowserPanel.h"
#include "../Utils/EditorAssetFactory.h"
#include <imgui.h>

#define ICON_FA_FOLDER  "\xef\x81\xbb"
#define ICON_FA_CODE    "\xef\x87\x89"
#define ICON_FA_CUBE    "\xef\x86\xb2"
#define ICON_FA_IMAGE   "\xef\x80\xbe"
#define ICON_FA_GLOBE   "\xef\x82\xac"
#define ICON_FA_FILE    "\xef\x85\x9b"

namespace gl {

	ContentBrowserPanel::ContentBrowserPanel() = default;

	static void LazyInit(std::filesystem::path& base, std::filesystem::path& cur)
	{
		if (base.empty())
		{
			base = std::filesystem::absolute("assets");
			cur  = base;
		}
	}

	static const char* GetIcon(const std::filesystem::path& path)
	{
		if (std::filesystem::is_directory(path)) return ICON_FA_FOLDER;
		auto ext = path.extension().string();
		if (ext == ".glsl")      return ICON_FA_CODE;
		if (ext == ".glimmer" || ext == ".glsky" || ext == ".hdr")
			return ICON_FA_GLOBE;
		if (ext == ".obj" || ext == ".fbx") return ICON_FA_CUBE;
		if (ext == ".png" || ext == ".jpg" || ext == ".hdr")
			return ICON_FA_IMAGE;
		return ICON_FA_FILE;
	}

	// ============================================================
	// 目录树 — 左侧面板
	// ============================================================

	void ContentBrowserPanel::DrawDirectoryTree(const std::filesystem::path& dir)
	{
		for (auto& entry : std::filesystem::directory_iterator(dir))
		{
			if (!entry.is_directory()) continue;

			const auto& path = entry.path();
			std::string name = path.filename().string();
			bool isCurrent = (m_CurrentDir == path);

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
			                         | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (isCurrent) flags |= ImGuiTreeNodeFlags_Selected;

			// 检查是否有子目录（决定是否可展开）
			bool hasSubDirs = false;
			for (auto& sub : std::filesystem::directory_iterator(path))
				if (sub.is_directory()) { hasSubDirs = true; break; }

			if (!hasSubDirs)
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

			std::string label = ICON_FA_FOLDER " " + name;
			bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

			// 单击选中 → 右侧切换到该目录
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				m_CurrentDir = path;
			}

			if (hasSubDirs && opened)
			{
				DrawDirectoryTree(path);
				ImGui::TreePop();
			}
		}
	}

	// ============================================================
	// 主渲染
	// ============================================================

	void ContentBrowserPanel::DrawCreateContextMenu()
	{
		if (!ImGui::BeginPopupContextWindow(
			"ContentBrowserCreate",
			ImGuiPopupFlags_MouseButtonRight
				| ImGuiPopupFlags_NoOpenOverItems))
			return;

		std::filesystem::path createdPath;
		if (ImGui::MenuItem("New Folder"))
			createdPath = EditorAssetFactory::CreateFolder(m_CurrentDir);

		if (ImGui::BeginMenu("Create Asset"))
		{
			if (ImGui::MenuItem("Material (.glmat)"))
				createdPath = EditorAssetFactory::CreateMaterial(m_CurrentDir);
			if (ImGui::MenuItem("Terrain Material (.glterrainmat)"))
				createdPath = EditorAssetFactory::CreateTerrainMaterial(m_CurrentDir);
			if (ImGui::MenuItem("Skybox (.glsky)"))
				createdPath = EditorAssetFactory::CreateSkybox(m_CurrentDir);
			if (ImGui::MenuItem("Scene (.glimmer)"))
				createdPath = EditorAssetFactory::CreateScene(m_CurrentDir);
			if (ImGui::MenuItem("Shader (.glsl)"))
				createdPath = EditorAssetFactory::CreateShader(m_CurrentDir);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Create Geometry"))
		{
			if (ImGui::MenuItem("Cube"))
				createdPath = EditorAssetFactory::CreateGeometry(
					m_CurrentDir, PrimitiveGeometry::Cube);
			if (ImGui::MenuItem("UV Sphere"))
				createdPath = EditorAssetFactory::CreateGeometry(
					m_CurrentDir, PrimitiveGeometry::UVSphere);
			if (ImGui::MenuItem("Plane"))
				createdPath = EditorAssetFactory::CreateGeometry(
					m_CurrentDir, PrimitiveGeometry::Plane);
			ImGui::EndMenu();
		}

		if (!createdPath.empty())
		{
			m_SelectedFile = createdPath.string();
			if (std::filesystem::is_regular_file(createdPath)
				&& createdPath.extension() != ".glimmer")
				AssetManager::ImportAsset(createdPath);
		}
		ImGui::EndPopup();
	}
	void ContentBrowserPanel::OnImGuiRender()
	{
		LazyInit(m_BaseDir, m_CurrentDir);

		ImGui::Begin("Content Browser");

		// --- 导航栏 ---
		if (ImGui::Button(" " ICON_FA_FOLDER " ..") && m_CurrentDir != m_BaseDir)
			m_CurrentDir = m_CurrentDir.parent_path();

		ImGui::SameLine();
		auto rel = std::filesystem::relative(m_CurrentDir, m_BaseDir);
		ImGui::TextDisabled("assets/%s", rel.string().c_str());

		ImGui::Separator();

		// --- 左栏：目录树 + 可拖分隔线 ---
		ImGui::BeginChild("TreePanel", ImVec2(m_SplitPos, 0), true);
		{
			// assets 根节点
			ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow
			                             | ImGuiTreeNodeFlags_SpanAvailWidth
			                             | ImGuiTreeNodeFlags_DefaultOpen;
			if (m_CurrentDir == m_BaseDir)
				rootFlags |= ImGuiTreeNodeFlags_Selected;

			bool rootOpen = ImGui::TreeNodeEx(ICON_FA_GLOBE " assets", rootFlags);

			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
				m_CurrentDir = m_BaseDir;

			if (rootOpen)
			{
				DrawDirectoryTree(m_BaseDir);
				ImGui::TreePop();
			}
		}
		ImGui::EndChild();

		// 可拖动分隔线
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::Button("##Splitter", ImVec2(4.0f, -1.0f));
		ImGui::PopStyleColor(2);

		if (ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		if (ImGui::IsItemActive())
			m_SplitPos += ImGui::GetIO().MouseDelta.x;

		m_SplitPos = glm::clamp(m_SplitPos, 120.0f, 500.0f);

		// --- 右栏：文件网格 ---
		ImGui::SameLine();
		ImGui::BeginChild("FilePanel", ImVec2(0, 0), true);
		{
			float cellSize = 80.0f;
			float panelWidth = ImGui::GetContentRegionAvail().x;
			int columns = std::max(1, (int)(panelWidth / cellSize));
			ImGui::Columns(columns, nullptr, false);

			for (auto& entry : std::filesystem::directory_iterator(m_CurrentDir))
			{
				const auto& path = entry.path();
				std::string name = path.filename().string();
				bool isDir = entry.is_directory();

				std::string label = name;
				if (label.size() > 10) label = label.substr(0, 9) + "...";

				const char* icon = GetIcon(path);
				std::string display = std::string(icon) + " " + label;

				bool isSelected = (m_SelectedFile == path.string());
				ImGui::PushID(name.c_str());

				if (ImGui::Selectable(display.c_str(), &isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(cellSize, cellSize)))
				{
					m_SelectedFile = path.string();
					if (!isDir && OnAssetSelected)
						OnAssetSelected(AssetManager::ImportAsset(path));
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						if (isDir)
						{
							m_CurrentDir = path;
							ImGui::PopID();
							ImGui::Columns(1);
							ImGui::EndChild();
							ImGui::End();
							return;
						}
						else if (OnFileDoubleClicked)
						{
							OnFileDoubleClicked(path.string());
						}
					}
				}

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
		}
		DrawCreateContextMenu();
		ImGui::EndChild();

		ImGui::End();
	}

}
