#pragma once
#include "Glimmer.h"
#include "SelectionContext.h"
#include "../Editor/EditorCommand.h"
#include <optional>
#include <functional>

namespace gl {

	// 场景层级面板 —— 低耦合设计：
	//   - 仅依赖引擎公共接口 Scene / Entity
	//   - 通过回调通知外部，不直接耦合编辑器逻辑
	//   - 可独立实例化测试，只需 SetContext + OnImGuiRender
	class SceneHierarchyPanel {
	public:
		SceneHierarchyPanel() = default;

		// 绑定要展示的场景
		void SetSelectionContext(SelectionContext* selection) { m_SharedSelection = selection; }
		void SetCommandHistory(EditorCommandHistory* history) { m_CommandHistory = history; }
		void SetContext(const Ref<Scene>& scene) { m_Context = scene; }

		// 每帧在 ImGui 中绘制
		void OnImGuiRender();

		// --- 选中状态 ---
		Entity GetSelectedEntity() const { return m_SelectionContext; }
		void SetSelectedEntity(Entity entity);
		void DrawComponents(Entity entity);

		// --- 低耦合回调（外部注册） ---
		std::function<void(Entity)> OnEntitySelected;  // 选中实体变化
		std::function<void(Entity)> OnEntityDeleted;    // 实体被删除

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
		void DrawEntityNode(Entity entity, uint32_t& idCounter);
		void DrawAddComponentMenu(Entity entity);
		SelectionContext* m_SharedSelection = nullptr;
		EditorCommandHistory* m_CommandHistory = nullptr;
		std::optional<TransformComponent> m_TransformBeforeEdit;

		Ref<Scene> m_Context;
		Entity m_SelectionContext;

		// 右键菜单
		Entity m_RightClickedEntity;
		bool m_ShowDeletePopup = false;
	};

}
