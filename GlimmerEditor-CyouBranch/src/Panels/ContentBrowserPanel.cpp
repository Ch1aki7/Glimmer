#include "ContentBrowserPanel.h"
#include "../Utils/EditorAssetFactory.h"
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

#define ICON_FA_FOLDER  "\xef\x81\xbb"
#define ICON_FA_GLOBE   "\xef\x82\xac"

namespace gl {
	namespace {
		constexpr float kScaleMinimum = 0.0f;
		constexpr float kScaleMaximum = 1.0f;
		constexpr float kScaleWheelStep = 0.08f;
		constexpr float kGridItemMinimum = 72.0f;
		constexpr float kGridItemMaximum = 160.0f;

		enum class ContentIconKind
		{
			Folder,
			Shader,
			Model,
			Image,
			Scene,
			Material,
			TerrainMaterial,
			Skybox,
			File
		};

		struct ContentIconStyle
		{
			ContentIconKind Kind = ContentIconKind::File;
			ImU32 Color = 0;
		};

		std::string Lowercase(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
				return static_cast<char>(std::tolower(character));
				});
			return value;
		}

		ContentIconStyle GetIconStyle(const std::filesystem::path& path, bool isDirectory)
		{
			if (isDirectory)
				return { ContentIconKind::Folder, IM_COL32(224, 174, 65, 255) };

			const std::string extension = Lowercase(path.extension().string());
			if (extension == ".glsl" || extension == ".glslinc" || extension == ".comp")
				return { ContentIconKind::Shader, IM_COL32(94, 178, 255, 255) };
			if (extension == ".obj" || extension == ".fbx")
				return { ContentIconKind::Model, IM_COL32(186, 126, 255, 255) };
			if (extension == ".png" || extension == ".jpg" || extension == ".jpeg"
				|| extension == ".tga" || extension == ".bmp")
				return { ContentIconKind::Image, IM_COL32(91, 201, 132, 255) };
			if (extension == ".glimmer")
				return { ContentIconKind::Scene, IM_COL32(88, 190, 216, 255) };
			if (extension == ".glmat")
				return { ContentIconKind::Material, IM_COL32(235, 132, 91, 255) };
			if (extension == ".glterrainmat")
				return { ContentIconKind::TerrainMaterial, IM_COL32(117, 177, 91, 255) };
			if (extension == ".glsky" || extension == ".hdr")
				return { ContentIconKind::Skybox, IM_COL32(92, 157, 224, 255) };
			return { ContentIconKind::File, IM_COL32(166, 174, 188, 255) };
		}

		void DrawContentIcon(
			ImDrawList* drawList,
			const ImVec2& minimum,
			const ImVec2& maximum,
			const ContentIconStyle& style)
		{
			const float width = maximum.x - minimum.x;
			const float height = maximum.y - minimum.y;
			const float lineWidth = std::max(1.0f, width * 0.045f);
			const ImU32 outline = IM_COL32(35, 39, 47, 210);
			const ImU32 detail = IM_COL32(245, 247, 250, 225);

			if (style.Kind == ContentIconKind::Folder)
			{
				const ImVec2 bodyMin(minimum.x, minimum.y + height * 0.25f);
				drawList->AddRectFilled(
					ImVec2(minimum.x + width * 0.08f, minimum.y + height * 0.10f),
					ImVec2(minimum.x + width * 0.50f, minimum.y + height * 0.38f),
					style.Color,
					width * 0.08f);
				drawList->AddRectFilled(bodyMin, maximum, style.Color, width * 0.09f);
				drawList->AddRect(bodyMin, maximum, outline, width * 0.09f, 0, lineWidth);
				return;
			}

			const float fold = width * 0.22f;
			const std::array<ImVec2, 6> fileShape = {
				ImVec2(minimum.x, minimum.y),
				ImVec2(maximum.x - fold, minimum.y),
				ImVec2(maximum.x, minimum.y + fold),
				maximum,
				ImVec2(minimum.x, maximum.y),
				ImVec2(minimum.x, minimum.y)
			};
			drawList->AddConvexPolyFilled(fileShape.data(), 5, style.Color);
			drawList->AddPolyline(fileShape.data(), 6, outline, 0, lineWidth);
			drawList->AddTriangleFilled(
				ImVec2(maximum.x - fold, minimum.y),
				ImVec2(maximum.x - fold, minimum.y + fold),
				ImVec2(maximum.x, minimum.y + fold),
				IM_COL32(255, 255, 255, 105));

			const ImVec2 center(
				(minimum.x + maximum.x) * 0.5f,
				minimum.y + height * 0.60f);
			const float symbolWidth = width * 0.44f;
			const float symbolHeight = height * 0.26f;

			switch (style.Kind)
			{
			case ContentIconKind::Shader:
				drawList->AddPolyline(std::array<ImVec2, 3>{
					ImVec2(center.x - symbolWidth * 0.10f, center.y - symbolHeight * 0.50f),
						ImVec2(center.x - symbolWidth * 0.42f, center.y),
						ImVec2(center.x - symbolWidth * 0.10f, center.y + symbolHeight * 0.50f)
				}.data(), 3, detail, 0, lineWidth);
				drawList->AddPolyline(std::array<ImVec2, 3>{
					ImVec2(center.x + symbolWidth * 0.10f, center.y - symbolHeight * 0.50f),
						ImVec2(center.x + symbolWidth * 0.42f, center.y),
						ImVec2(center.x + symbolWidth * 0.10f, center.y + symbolHeight * 0.50f)
				}.data(), 3, detail, 0, lineWidth);
				break;
			case ContentIconKind::Model:
			{
				const ImVec2 top(center.x, center.y - symbolHeight * 0.58f);
				const ImVec2 left(center.x - symbolWidth * 0.44f, center.y - symbolHeight * 0.12f);
				const ImVec2 right(center.x + symbolWidth * 0.44f, center.y - symbolHeight * 0.12f);
				const ImVec2 bottom(center.x, center.y + symbolHeight * 0.58f);
				drawList->AddLine(top, left, detail, lineWidth);
				drawList->AddLine(top, right, detail, lineWidth);
				drawList->AddLine(left, bottom, detail, lineWidth);
				drawList->AddLine(right, bottom, detail, lineWidth);
				drawList->AddLine(top, center, detail, lineWidth);
				drawList->AddLine(center, bottom, detail, lineWidth);
				break;
			}
			case ContentIconKind::Image:
				drawList->AddCircleFilled(
					ImVec2(center.x + symbolWidth * 0.24f, center.y - symbolHeight * 0.35f),
					std::max(1.5f, width * 0.045f), detail);
				drawList->AddPolyline(std::array<ImVec2, 4>{
					ImVec2(center.x - symbolWidth * 0.46f, center.y + symbolHeight * 0.48f),
						ImVec2(center.x - symbolWidth * 0.13f, center.y - symbolHeight * 0.12f),
						ImVec2(center.x + symbolWidth * 0.08f, center.y + symbolHeight * 0.18f),
						ImVec2(center.x + symbolWidth * 0.46f, center.y - symbolHeight * 0.30f)
				}.data(), 4, detail, 0, lineWidth);
				break;
			case ContentIconKind::Scene:
			case ContentIconKind::Skybox:
			{
				const float radius = std::min(symbolWidth, symbolHeight) * 0.48f;
				drawList->AddCircle(center, radius, detail, 16, lineWidth);
				drawList->AddLine(
					ImVec2(center.x - radius, center.y),
					ImVec2(center.x + radius, center.y), detail, lineWidth);
				drawList->AddEllipse(center, ImVec2(radius * 0.42f, radius), detail, 0.0f, 16, lineWidth);
				break;
			}
			case ContentIconKind::Material:
				drawList->AddCircleFilled(center, std::min(symbolWidth, symbolHeight) * 0.50f, detail, 20);
				drawList->AddCircleFilled(
					ImVec2(center.x - symbolWidth * 0.10f, center.y - symbolHeight * 0.12f),
					std::min(symbolWidth, symbolHeight) * 0.16f,
					IM_COL32(255, 255, 255, 130), 12);
				break;
			case ContentIconKind::TerrainMaterial:
				drawList->AddPolyline(std::array<ImVec2, 4>{
					ImVec2(center.x - symbolWidth * 0.48f, center.y + symbolHeight * 0.42f),
						ImVec2(center.x - symbolWidth * 0.18f, center.y - symbolHeight * 0.34f),
						ImVec2(center.x + symbolWidth * 0.03f, center.y + symbolHeight * 0.04f),
						ImVec2(center.x + symbolWidth * 0.48f, center.y - symbolHeight * 0.50f)
				}.data(), 4, detail, 0, lineWidth);
				break;
			default:
				for (int line = -1; line <= 1; ++line)
					drawList->AddLine(
						ImVec2(center.x - symbolWidth * 0.38f, center.y + line * symbolHeight * 0.34f),
						ImVec2(center.x + symbolWidth * 0.38f, center.y + line * symbolHeight * 0.34f),
						detail, lineWidth);
				break;
			}
		}

		std::string EllipsizeFilename(const std::string& name, float maximumWidth)
		{
			if (ImGui::CalcTextSize(name.c_str()).x <= maximumWidth)
				return name;

			constexpr const char* ellipsis = "...";
			const float remainingWidth = maximumWidth - ImGui::CalcTextSize(ellipsis).x;
			if (remainingWidth <= 0.0f)
				return ellipsis;

			const char* remaining = name.c_str();
			ImGui::GetFont()->CalcTextSizeA(
				ImGui::GetFontSize(), remainingWidth, 0.0f,
				name.c_str(), nullptr, &remaining);
			return std::string(name.c_str(), remaining) + ellipsis;
		}
	}

	ContentBrowserPanel::ContentBrowserPanel() = default;

	static void LazyInit(std::filesystem::path& base, std::filesystem::path& cur)
	{
		if (base.empty())
		{
			base = std::filesystem::absolute("assets");
			cur = base;
		}
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

		// --- 右栏：可缩放文件列表/网格 ---
		ImGui::SameLine();
		ImGui::BeginChild(
			"FilePanel", ImVec2(0, 0), ImGuiChildFlags_Borders,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		{
			constexpr ImVec2 sliderFramePadding(4.0f, 0.0f);
			constexpr float sliderBottomInset = 2.0f;
			constexpr float fileItemsBottomGap = 0.0f;
			const float sliderHeight = ImGui::GetFontSize()
				+ sliderFramePadding.y * 2.0f;
			const float sliderTop = ImGui::GetWindowHeight()
				- sliderHeight - sliderBottomInset;
			const float fileItemsTop = ImGui::GetCursorPosY();
			const float fileItemsHeight = std::max(
				1.0f, sliderTop - fileItemsTop - fileItemsBottomGap);
			ImGui::BeginChild(
				"FileItems", ImVec2(0.0f, fileItemsHeight), ImGuiChildFlags_None);

			ImGuiIO& io = ImGui::GetIO();
			if (ImGui::IsWindowHovered() && io.KeyCtrl && io.MouseWheel != 0.0f)
				m_ItemScale = glm::clamp(
					m_ItemScale + io.MouseWheel * kScaleWheelStep,
					kScaleMinimum, kScaleMaximum);

			std::vector<std::filesystem::directory_entry> entries;
			for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDir))
				entries.push_back(entry);
			std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
				if (left.is_directory() != right.is_directory())
					return left.is_directory();
				return Lowercase(left.path().filename().string())
					< Lowercase(right.path().filename().string());
				});

			const bool compact = m_ItemScale <= kScaleMinimum;
			const float itemSize = glm::mix(kGridItemMinimum, kGridItemMaximum, m_ItemScale);
			const float panelWidth = ImGui::GetContentRegionAvail().x;
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const int columns = compact
				? 1
				: std::max(1, static_cast<int>((panelWidth + spacing) / (itemSize + spacing)));
			std::filesystem::path requestedDirectory;

			for (size_t index = 0; index < entries.size(); ++index)
			{
				const auto& entry = entries[index];
				const auto& path = entry.path();
				std::string name = path.filename().string();
				bool isDir = entry.is_directory();
				bool isSelected = (m_SelectedFile == path.string());
				ImGui::PushID(static_cast<int>(index));

				if (!compact && index % columns != 0)
					ImGui::SameLine();

				const ImVec2 itemPosition = ImGui::GetCursorScreenPos();
				const ImVec2 selectableSize(
					compact ? ImGui::GetContentRegionAvail().x : itemSize,
					compact ? ImGui::GetFrameHeight() : itemSize);
				const bool pressed = ImGui::Selectable(
					"##ContentEntry", isSelected,
					ImGuiSelectableFlags_AllowDoubleClick,
					selectableSize);
				const bool hovered = ImGui::IsItemHovered();
				const bool doubleClicked = hovered
					&& ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

				ImDrawList* drawList = ImGui::GetWindowDrawList();
				const ContentIconStyle iconStyle = GetIconStyle(path, isDir);
				if (compact)
				{
					const float iconSize = ImGui::GetFontSize();
					const ImVec2 iconMin(
						itemPosition.x + ImGui::GetStyle().FramePadding.x,
						itemPosition.y + (selectableSize.y - iconSize) * 0.5f);
					DrawContentIcon(drawList, iconMin,
						ImVec2(iconMin.x + iconSize, iconMin.y + iconSize), iconStyle);
					drawList->AddText(
						ImVec2(iconMin.x + iconSize + ImGui::GetStyle().ItemInnerSpacing.x,
							itemPosition.y + ImGui::GetStyle().FramePadding.y),
						ImGui::GetColorU32(ImGuiCol_Text),
						EllipsizeFilename(name,
							selectableSize.x - iconSize - ImGui::GetStyle().ItemInnerSpacing.x
							- ImGui::GetStyle().FramePadding.x * 2.0f).c_str());
				}
				else
				{
					const float labelHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
					const float iconExtent = std::min(itemSize * 0.62f, itemSize - labelHeight - 10.0f);
					const ImVec2 iconMin(
						itemPosition.x + (itemSize - iconExtent) * 0.5f,
						itemPosition.y + std::max(5.0f, (itemSize - labelHeight - iconExtent) * 0.5f));
					DrawContentIcon(drawList, iconMin,
						ImVec2(iconMin.x + iconExtent, iconMin.y + iconExtent), iconStyle);

					const std::string label = EllipsizeFilename(name, itemSize - 10.0f);
					const float labelWidth = ImGui::CalcTextSize(label.c_str()).x;
					drawList->AddText(
						ImVec2(itemPosition.x + std::max(5.0f, (itemSize - labelWidth) * 0.5f),
							itemPosition.y + itemSize - labelHeight + ImGui::GetStyle().FramePadding.y),
						ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
				}

				if (hovered)
					ImGui::SetTooltip("%s", name.c_str());

				if (pressed)
				{
					m_SelectedFile = path.string();
					if (!isDir && OnAssetSelected)
						OnAssetSelected(AssetManager::ImportAsset(path));
					if (doubleClicked)
					{
						if (isDir)
							requestedDirectory = path;
						else if (OnFileDoubleClicked)
							OnFileDoubleClicked(path.string());
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
			}

			if (!requestedDirectory.empty())
				m_CurrentDir = requestedDirectory;
			DrawCreateContextMenu();
			ImGui::EndChild();

			constexpr float sliderWidth = 112.0f;
			const float footerWidth = ImGui::GetContentRegionAvail().x;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, footerWidth - sliderWidth));
			ImGui::SetCursorPosY(sliderTop);
			ImGui::SetNextItemWidth(sliderWidth);

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, sliderFramePadding);
			ImGui::SliderFloat("##ContentBrowserScale", &m_ItemScale, kScaleMinimum, kScaleMaximum, "", ImGuiSliderFlags_AlwaysClamp);
			ImGui::PopStyleVar();

			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Item size (Ctrl + mouse wheel)");
		}
		ImGui::EndChild();

		ImGui::End();
	}

}
