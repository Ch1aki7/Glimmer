#pragma once

#include "Glimmer.h"
#include "SelectionContext.h"

#include "../Editor/EditorCommand.h"
#include <optional>

namespace gl
{
	class InspectorPanel
	{
	public:
		void SetContext(const Ref<Scene>& scene) { m_Context = scene; }
		void SetSelectionContext(SelectionContext* selection) { m_Selection = selection; }
		void SetCommandHistory(EditorCommandHistory* history) { m_CommandHistory = history; }

		void OnImGuiRender();

	private:
		void DrawAssetInspector(AssetHandle handle);
		void DrawComponents(Entity entity);
		void DrawAddComponentMenu(Entity entity);

		template<typename T, typename UIFunction>
		void DrawComponent(const char* name, Entity entity, UIFunction drawUI,
			bool removable = true)
		{
			if (!entity.HasComponent<T>())
				return;

			ImGui::PushID(static_cast<int>(typeid(T).hash_code()));
			const bool open = ImGui::TreeNodeEx(
				"##Component", ImGuiTreeNodeFlags_DefaultOpen, "%s", name);
			bool removeComponent = false;
			if (ImGui::BeginPopupContextItem("ComponentSettings"))
			{
				if (ImGui::MenuItem("Reset"))
					entity.GetComponent<T>() = T{};
				if (removable && ImGui::MenuItem("Remove Component"))
					removeComponent = true;
				ImGui::EndPopup();
			}

			if (open)
			{
				drawUI(entity.GetComponent<T>());
				ImGui::TreePop();
			}
			if (removeComponent)
				entity.RemoveComponent<T>();
			ImGui::PopID();
		}


	private:
		Ref<Scene> m_Context;
		SelectionContext* m_Selection = nullptr;
		EditorCommandHistory* m_CommandHistory = nullptr;
		std::optional<TransformComponent> m_TransformBeforeEdit;
	};
}
