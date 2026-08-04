#pragma once

#include "Glimmer.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gl
{
	struct EntitySnapshot
	{
		UUID ID{ 0 };
		TagComponent Tag;
		TransformComponent Transform;
		std::optional<SpriteRendererComponent> SpriteRenderer;
		std::optional<ModelRendererComponent> ModelRenderer;
		std::optional<MaterialComponent> Material;
		std::optional<TerrainComponent> Terrain;
		std::optional<DirectionalLightComponent> DirectionalLight;
		std::optional<PointLightComponent> PointLight;
		std::optional<SkyLightComponent> SkyLight;
		std::optional<CameraComponent> Camera;
		bool HasNativeScript = false;
		ScriptableEntity* (*InstantiateScript)() = nullptr;
		void (*DestroyScript)(NativeScriptComponent*) = nullptr;

		static EntitySnapshot Capture(Entity entity);
		Entity Restore(const Ref<Scene>& scene) const;
	};

	class IEditorCommand
	{
	public:
		virtual ~IEditorCommand() = default;
		virtual void Execute() = 0;
		virtual void Undo() = 0;
		virtual const char* GetName() const = 0;
	};

	class LambdaEditorCommand final : public IEditorCommand
	{
	public:
		LambdaEditorCommand(std::string name,
			std::function<void()> execute,
			std::function<void()> undo)
			: m_Name(std::move(name)),
			  m_Execute(std::move(execute)),
			  m_Undo(std::move(undo))
		{
		}

		void Execute() override { m_Execute(); }
		void Undo() override { m_Undo(); }
		const char* GetName() const override { return m_Name.c_str(); }

	private:
		std::string m_Name;
		std::function<void()> m_Execute;
		std::function<void()> m_Undo;
	};

	class EditorCommandHistory
	{
	public:
		void Execute(std::unique_ptr<IEditorCommand> command);
		void PushExecuted(std::unique_ptr<IEditorCommand> command);
		bool Undo();
		bool Redo();
		void Clear();

		bool CanUndo() const { return !m_UndoStack.empty(); }
		bool CanRedo() const { return !m_RedoStack.empty(); }
		const char* GetUndoName() const;
		const char* GetRedoName() const;

	private:
		std::vector<std::unique_ptr<IEditorCommand>> m_UndoStack;
		std::vector<std::unique_ptr<IEditorCommand>> m_RedoStack;
	};
}
