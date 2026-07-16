#include "SceneHierarchyPanel.h"

namespace gl {

	void SceneHierarchyPanel::OnImGuiRender()
	{
		if (!m_Context) {
			ImGui::TextDisabled("No scene context");
			return;
		}

		ImGui::Begin("Scene Hierarchy");

		// --- 顶部工具栏 ---
		if (ImGui::Button("+ Create Entity"))
		{
			auto entity = m_Context->CreateEntity();
			m_SelectionContext = entity;
			if (OnEntitySelected) OnEntitySelected(entity);
		}


		ImGui::Separator();

		// --- 实体列表 ---
		uint32_t idCounter = 0;
		m_Context->m_Registry.view<entt::entity>().each([&](entt::entity handle) {
			Entity entity{ handle, m_Context.get() };
			if (entity.HasComponent<TagComponent>()) {
				DrawEntityNode(entity, idCounter);
			}
		});

		// --- 右键删除弹窗 ---
		if (m_ShowDeletePopup) {
			ImGui::OpenPopup("Delete Entity?");
			m_ShowDeletePopup = false;
		}

		if (ImGui::BeginPopupModal("Delete Entity?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (m_RightClickedEntity && m_RightClickedEntity.HasComponent<TagComponent>()) {
				ImGui::Text("Delete '%s'?", m_RightClickedEntity.GetComponent<TagComponent>().Tag.c_str());
			}
			ImGui::Separator();

			if (ImGui::Button("Yes", ImVec2(80, 0))) {
				if (m_SelectionContext == m_RightClickedEntity)
					m_SelectionContext = Entity{};

				if (OnEntityDeleted)
					OnEntityDeleted(m_RightClickedEntity);

				m_Context->DestroyEntity(m_RightClickedEntity);
				m_RightClickedEntity = Entity{};
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("No", ImVec2(80, 0))) {
				m_RightClickedEntity = Entity{};
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::End();
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity, uint32_t& idCounter)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		// --- 组件图标 ---
		std::string label;
		label += tag.empty() ? "Unnamed" : tag;

		// 拼接组件缩写标记
		std::string badges;
		if (entity.HasComponent<CameraComponent>())    badges += " [Cam]";
		if (entity.HasComponent<SpriteRendererComponent>()) badges += " [Spr]";
		if (entity.HasComponent<NativeScriptComponent>())   badges += " [Scr]";

		label += badges;

		// --- 选中高亮 ---
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_NoTreePushOnOpen;

		if (m_SelectionContext == entity)
			flags |= ImGuiTreeNodeFlags_Selected;

		// 基于 idCounter 让不同帧同一实体保持稳定 ID
		ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", label.c_str());

		// --- 左键选中 ---
		if (ImGui::IsItemClicked()) {
			m_SelectionContext = entity;
			if (OnEntitySelected) OnEntitySelected(entity);
		}

		// --- 右键菜单 ---
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Delete")) {
				m_RightClickedEntity = entity;
				m_ShowDeletePopup = true;
			}
			ImGui::EndPopup();
		}

		// 禁止未使用的参数警告
		(void)idCounter;
	}

}
