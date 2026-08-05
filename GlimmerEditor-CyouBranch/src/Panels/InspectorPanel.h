#pragma once

#include "Glimmer.h"
#include "SelectionContext.h"

#include "../Editor/EditorCommand.h"
#include <optional>
#include <type_traits>

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

		template<typename T>
		void AddComponent(Entity entity, const char* name, const T& component = T{})
		{
			if (entity.HasComponent<T>())
				return;

			if (m_CommandHistory && m_Context)
			{
				const Ref<Scene> scene = m_Context;
				const UUID uuid = entity.GetUUID();
				m_CommandHistory->Execute(std::make_unique<LambdaEditorCommand>(
					std::string("Add ") + name,
					[scene, uuid, component]() {
						Entity target = scene->FindEntityByUUID(uuid);
						if (target && !target.HasComponent<T>())
							target.AddComponent<T>(component);
					},
					[scene, uuid]() {
						Entity target = scene->FindEntityByUUID(uuid);
						if (target && target.HasComponent<T>())
							target.RemoveComponent<T>();
					}));
			}
			else
				entity.AddComponent<T>(component);
		}

		template<typename T, typename UIFunction>
		void DrawComponent(const char* name, Entity entity, UIFunction drawUI,
			bool removable = true, bool resettable = true)
		{
			if (!entity.HasComponent<T>())
				return;

			ImGui::PushID(static_cast<int>(typeid(T).hash_code()));
			const bool open = ImGui::TreeNodeEx(
				"##Component",
				ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
				"%s", name);
			bool removeComponent = false;
			if (ImGui::BeginPopupContextItem("ComponentSettings"))
			{
				if (resettable && ImGui::MenuItem("Reset"))
				{
					const T before = entity.GetComponent<T>();
					const T after{};
					if (m_CommandHistory && m_Context)
					{
						const Ref<Scene> scene = m_Context;
						const UUID uuid = entity.GetUUID();
						m_CommandHistory->Execute(std::make_unique<LambdaEditorCommand>(
							std::string("Reset ") + name,
							[scene, uuid, after]() {
								Entity target = scene->FindEntityByUUID(uuid);
								if (target && target.HasComponent<T>())
									ReplaceComponentValue<T>(target, after);
							},
							[scene, uuid, before]() {
								Entity target = scene->FindEntityByUUID(uuid);
								if (target && target.HasComponent<T>())
									ReplaceComponentValue<T>(target, before);
							}));
					}
					else
						ReplaceComponentValue<T>(entity, after);
				}
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
			{
				const T removedComponent = entity.GetComponent<T>();
				if (m_CommandHistory && m_Context)
				{
					const Ref<Scene> scene = m_Context;
					const UUID uuid = entity.GetUUID();
					m_CommandHistory->Execute(std::make_unique<LambdaEditorCommand>(
						std::string("Remove ") + name,
						[scene, uuid]() {
							Entity target = scene->FindEntityByUUID(uuid);
							if (target && target.HasComponent<T>())
								target.RemoveComponent<T>();
						},
						[scene, uuid, removedComponent]() {
							Entity target = scene->FindEntityByUUID(uuid);
							if (target && !target.HasComponent<T>())
								target.AddComponent<T>(removedComponent);
						}));
				}
				else
					entity.RemoveComponent<T>();
			}
			ImGui::PopID();
		}

		template<typename T>
		static void ReplaceComponentValue(Entity entity, const T& value)
		{
			if constexpr (std::is_same_v<T, TransformComponent>)
			{
				entity.GetComponent<T>() = value;
			}
			else
			{
				entity.RemoveComponent<T>();
				entity.AddComponent<T>(value);
			}
		}

	private:
		Ref<Scene> m_Context;
		SelectionContext* m_Selection = nullptr;
		EditorCommandHistory* m_CommandHistory = nullptr;
		std::optional<TransformComponent> m_TransformBeforeEdit;
	};
}
