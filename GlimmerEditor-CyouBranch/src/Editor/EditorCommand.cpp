#include "EditorCommand.h"

namespace gl
{
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
