# Glimmer agent workflow

Before planning or changing this repository, read `Documents/PROJECT_STATUS.md`, `ARCHITECTURE.md`, and the relevant feature section in `README.md`.

Keep the project documentation synchronized as part of every task:

- Work from the single item under “当前主线” unless the user explicitly changes priority.
- Do not mark work complete until its acceptance criteria have been verified.
- At task completion, review all three documents even if only one needs a content change:
  - `Documents/PROJECT_STATUS.md`: update current status, completed work, verification evidence, priorities, and technical debt.
  - `ARCHITECTURE.md`: update only facts about implemented structure, ownership, dependencies, data flow, and known architectural boundaries. Never describe planned work as implemented.
  - `README.md`: update user/developer-facing behavior, workflows, implementation notes, and validation results. Put new feature sections immediately above `## KB` and follow the existing heading style.
- When a tracked document does not need textual changes, confirm that it was reviewed before declaring the task complete.
- Move completed planned work into “已完成里程碑”; do not leave duplicate active and completed copies. Record the completion date, verification evidence, and commit or “待提交”, then promote the next ready task.
- Reorganize documentation when it improves clarity, while preserving the distinct source-of-truth role of each file.
