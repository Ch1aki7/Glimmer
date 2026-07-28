#pragma once

#include "Glimmer.h"
#include "SelectionContext.h"

#include <functional>

namespace gl
{
	class InspectorPanel
	{
	public:
		using EntityDrawer = std::function<void(Entity)>;

		void SetContext(const Ref<Scene>& scene) { m_Context = scene; }
		void SetSelectionContext(SelectionContext* selection) { m_Selection = selection; }
		void SetEntityDrawer(EntityDrawer drawer) { m_EntityDrawer = std::move(drawer); }

		void OnImGuiRender();

	private:
		void DrawAssetInspector(AssetHandle handle);

	private:
		Ref<Scene> m_Context;
		SelectionContext* m_Selection = nullptr;
		EntityDrawer m_EntityDrawer;
	};
}
