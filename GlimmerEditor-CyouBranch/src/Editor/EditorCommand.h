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
		virtual bool Execute() = 0;
		virtual bool Undo() = 0;
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

		bool Execute() override { m_Execute(); return true; }
		bool Undo() override { m_Undo(); return true; }
		const char* GetName() const override { return m_Name.c_str(); }

	private:
		std::string m_Name;
		std::function<void()> m_Execute;
		std::function<void()> m_Undo;
	};

	template<typename T>
	class ValueEditorCommand final : public IEditorCommand
	{
	public:
		using ApplyFunction = std::function<bool(const T&)>;

		ValueEditorCommand(std::string name, T before, T after,
			ApplyFunction apply)
			: m_Name(std::move(name)),
			  m_Before(std::move(before)),
			  m_After(std::move(after)),
			  m_Apply(std::move(apply))
		{
		}

		bool Execute() override { return m_Apply && m_Apply(m_After); }
		bool Undo() override { return m_Apply && m_Apply(m_Before); }
		const char* GetName() const override { return m_Name.c_str(); }

	private:
		std::string m_Name;
		T m_Before;
		T m_After;
		ApplyFunction m_Apply;
	};

	template<typename T>
	class EditorValueTransaction
	{
	public:
		void Begin(const T& value)
		{
			m_Before = value;
		}

		bool IsActive() const { return m_Before.has_value(); }
		const T& GetBefore() const { return *m_Before; }
		void Reset() { m_Before.reset(); }

	private:
		std::optional<T> m_Before;
	};

	class EditorCommandHistory
	{
	public:
		bool Execute(std::unique_ptr<IEditorCommand> command);
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
