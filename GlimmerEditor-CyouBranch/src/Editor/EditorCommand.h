#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gl
{
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
