#pragma once

#include "Glimmer/Renderer/Shader.h"

namespace gl {

	class ShaderPanel {
	public:
		explicit ShaderPanel(ShaderLibrary* library = nullptr)
			: m_Library(library) {}

		void SetLibrary(ShaderLibrary* library) { m_Library = library; }
		void OnUpdate();
		void OnImGuiRender();

	private:
		void RecordResults(
			const std::vector<std::pair<std::string, ShaderReloadResult>>& results);

		ShaderLibrary* m_Library = nullptr;
		bool m_AutoReload = true;
		std::string m_LastEvent = "No reload has been attempted.";
		bool m_LastEventSucceeded = true;
	};

}
