#include "EditorCommand.h"

namespace gl
{
	namespace
	{
		template<typename T>
		void CaptureComponent(Entity entity, std::optional<T>& destination)
		{
			if (entity.HasComponent<T>())
				destination = entity.GetComponent<T>();
		}

		template<typename T>
		void RestoreComponent(Entity entity, const std::optional<T>& source)
		{
			if (source)
				entity.AddComponent<T>(*source);
		}
	}

	EntitySnapshot EntitySnapshot::Capture(Entity entity)
	{
		GL_CORE_ASSERT(entity && entity.HasComponent<IDComponent>(),
			"Cannot capture an invalid entity.");

		EntitySnapshot snapshot;
		snapshot.ID = entity.GetUUID();
		if (entity.HasComponent<TagComponent>())
			snapshot.Tag = entity.GetComponent<TagComponent>();
		if (entity.HasComponent<TransformComponent>())
			snapshot.Transform = entity.GetComponent<TransformComponent>();

		CaptureComponent(entity, snapshot.SpriteRenderer);
		CaptureComponent(entity, snapshot.ModelRenderer);
		CaptureComponent(entity, snapshot.Material);
		CaptureComponent(entity, snapshot.Terrain);
		CaptureComponent(entity, snapshot.DirectionalLight);
		CaptureComponent(entity, snapshot.PointLight);
		CaptureComponent(entity, snapshot.SkyLight);
		CaptureComponent(entity, snapshot.Camera);

		if (entity.HasComponent<NativeScriptComponent>())
		{
			const auto& script = entity.GetComponent<NativeScriptComponent>();
			snapshot.HasNativeScript = true;
			snapshot.InstantiateScript = script.InstantiateScript;
			snapshot.DestroyScript = script.DestroyScript;
		}
		return snapshot;
	}

	Entity EntitySnapshot::Restore(const Ref<Scene>& scene) const
	{
		if (!scene || static_cast<uint64_t>(ID) == 0)
			return {};
		if (Entity existing = scene->FindEntityByUUID(ID))
			return existing;

		Entity entity = scene->CreateEntityWithUUID(ID, Tag.Tag);
		entity.GetComponent<TagComponent>() = Tag;
		entity.GetComponent<TransformComponent>() = Transform;
		RestoreComponent(entity, SpriteRenderer);
		RestoreComponent(entity, ModelRenderer);
		RestoreComponent(entity, Material);
		RestoreComponent(entity, Terrain);
		RestoreComponent(entity, DirectionalLight);
		RestoreComponent(entity, PointLight);
		RestoreComponent(entity, SkyLight);
		RestoreComponent(entity, Camera);

		if (HasNativeScript)
		{
			auto& script = entity.AddComponent<NativeScriptComponent>();
			script.InstantiateScript = InstantiateScript;
			script.DestroyScript = DestroyScript;
		}
		return entity;
	}

	void EditorCommandHistory::Execute(std::unique_ptr<IEditorCommand> command)
	{
		if (!command)
			return;
		command->Execute();
		PushExecuted(std::move(command));
	}

	void EditorCommandHistory::PushExecuted(std::unique_ptr<IEditorCommand> command)
	{
		if (!command)
			return;
		m_UndoStack.emplace_back(std::move(command));
		m_RedoStack.clear();
	}

	bool EditorCommandHistory::Undo()
	{
		if (m_UndoStack.empty())
			return false;

		auto command = std::move(m_UndoStack.back());
		m_UndoStack.pop_back();
		command->Undo();
		m_RedoStack.emplace_back(std::move(command));
		return true;
	}

	bool EditorCommandHistory::Redo()
	{
		if (m_RedoStack.empty())
			return false;

		auto command = std::move(m_RedoStack.back());
		m_RedoStack.pop_back();
		command->Execute();
		m_UndoStack.emplace_back(std::move(command));
		return true;
	}

	void EditorCommandHistory::Clear()
	{
		m_UndoStack.clear();
		m_RedoStack.clear();
	}

	const char* EditorCommandHistory::GetUndoName() const
	{
		return CanUndo() ? m_UndoStack.back()->GetName() : "";
	}

	const char* EditorCommandHistory::GetRedoName() const
	{
		return CanRedo() ? m_RedoStack.back()->GetName() : "";
	}
}
