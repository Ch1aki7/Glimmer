#include "SceneHierarchyPanel.h"
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>

namespace gl {

	void SceneHierarchyPanel::OnImGuiRender()
	{
		if (!m_Context) {
			ImGui::TextDisabled("No scene context");
			return;
		}

		ImGui::Begin("Scene Hierarchy");

		if (ImGui::Button("+ Create Entity"))
		{
			auto entity = m_Context->CreateEntity();
			m_SelectionContext = entity;
			if (OnEntitySelected) OnEntitySelected(entity);
		}

		ImGui::Separator();

		uint32_t idCounter = 0;
		m_Context->m_Registry.view<entt::entity>().each([&](entt::entity handle) {
			Entity entity{ handle, m_Context.get() };
			if (entity.HasComponent<TagComponent>()) {
				DrawEntityNode(entity, idCounter);
			}
		});

		ImGui::Separator();
		if (m_SelectionContext && m_SelectionContext.HasComponent<TagComponent>())
		{
			DrawComponents(m_SelectionContext);
		}

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

		std::string label = tag.empty() ? "Unnamed" : tag;

		std::string badges;
		if (entity.HasComponent<CameraComponent>())          badges += " [Cam]";
		if (entity.HasComponent<SpriteRendererComponent>())  badges += " [Spr]";
		if (entity.HasComponent<NativeScriptComponent>())    badges += " [Scr]";
		label += badges;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf
			| ImGuiTreeNodeFlags_SpanAvailWidth
			| ImGuiTreeNodeFlags_NoTreePushOnOpen;

		if (m_SelectionContext == entity)
			flags |= ImGuiTreeNodeFlags_Selected;

		ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", label.c_str());

		if (ImGui::IsItemClicked()) {
			m_SelectionContext = entity;
			if (OnEntitySelected) OnEntitySelected(entity);
		}

		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Delete")) {
				m_RightClickedEntity = entity;
				m_ShowDeletePopup = true;
			}
			ImGui::EndPopup();
		}

		(void)idCounter;
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity)
	{
		// --- Tag ---
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);
			if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
			}
		}

		// --- Transform ---
		if (entity.HasComponent<TransformComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(TransformComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Transform"))
			{
				auto& tc = entity.GetComponent<TransformComponent>();
				ImGui::DragFloat3("Position", glm::value_ptr(tc.Translation), 0.1f);
				ImGui::DragFloat3("Rotation", glm::value_ptr(tc.Rotation), 1.0f);
				ImGui::DragFloat3("Scale",    glm::value_ptr(tc.Scale), 0.05f, 0.01f, 10.0f);
				ImGui::TreePop();
			}
		}

		// --- Camera ---
		if (entity.HasComponent<CameraComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(CameraComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Camera"))
			{
				auto& cameraComponent = entity.GetComponent<CameraComponent>();
				auto& camera = cameraComponent.Camera;

				ImGui::Checkbox("Primary", &cameraComponent.Primary);

				const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
				const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];
				if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
				{
					for (int i = 0; i < 2; i++)
					{
						bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
						if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
						{
							currentProjectionTypeString = projectionTypeStrings[i];
							camera.SetProjectionType((SceneCamera::ProjectionType)i);
						}
						if (isSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
				{
					float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
					if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov, 0.5f, 1.0f, 179.0f))
						camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

					float perspectiveNear = camera.GetPerspectiveNearClip();
					if (ImGui::DragFloat("Near", &perspectiveNear, 0.01f, 0.001f))
						camera.SetPerspectiveNearClip(perspectiveNear);

					float perspectiveFar = camera.GetPerspectiveFarClip();
					if (ImGui::DragFloat("Far", &perspectiveFar, 1.0f))
						camera.SetPerspectiveFarClip(perspectiveFar);
				}

				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float orthoSize = camera.GetOrthographicSize();
					if (ImGui::DragFloat("Size", &orthoSize, 0.1f, 0.1f))
						camera.SetOrthographicSize(orthoSize);

					float orthoNear = camera.GetOrthographicNearClip();
					if (ImGui::DragFloat("Near", &orthoNear, 0.1f))
						camera.SetOrthographicNearClip(orthoNear);

					float orthoFar = camera.GetOrthographicFarClip();
					if (ImGui::DragFloat("Far", &orthoFar, 0.1f))
						camera.SetOrthographicFarClip(orthoFar);

					ImGui::Checkbox("Fixed Aspect Ratio", &cameraComponent.FixedAspectRatio);
				}

				ImGui::TreePop();
			}
		}

		// --- Sprite Renderer ---
		if (entity.HasComponent<SpriteRendererComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(SpriteRendererComponent).hash_code(), ImGuiTreeNodeFlags_DefaultOpen, "Sprite Renderer"))
			{
				auto& src = entity.GetComponent<SpriteRendererComponent>();
				ImGui::ColorEdit4("Color", glm::value_ptr(src.Color));
				ImGui::DragFloat("Tiling", &src.TilingFactor, 0.1f, 0.1f, 10.0f);

				// 纹理状态 + 预览
				ImGui::Text("Texture: %s", src.Texture ? "Loaded" : "None (drag here)");
				ImGui::SameLine();
				if (src.Texture && ImGui::SmallButton("X"))
					src.Texture = nullptr;

				// 接收从 Content Browser 拖来的贴图文件
				if (ImGui::BeginDragDropTarget())
				{
					if (auto* payload = ImGui::AcceptDragDropPayload("SCENE_FILE"))
					{
						std::string path((const char*)payload->Data, payload->DataSize - 1);
						auto ext = std::filesystem::path(path).extension().string();
						if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
							src.Texture = Texture2D::Create(path);
					}
					ImGui::EndDragDropTarget();
				}

				ImGui::TreePop();
			}
		}
	}

}
