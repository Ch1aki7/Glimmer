#include "ShaderPanel.h"

#include <algorithm>
#include <imgui.h>

namespace gl {

	void ShaderPanel::OnUpdate()
	{
		if (m_AutoReload && m_Library)
			RecordResults(m_Library->ReloadChanged());
	}

	void ShaderPanel::RecordResults(
		const std::vector<std::pair<std::string, ShaderReloadResult>>& results)
	{
		for (const auto& [name, result] : results)
		{
			m_LastEventSucceeded = result.Success;
			m_LastEvent = name + ": " + result.Message;
		}
	}

	void ShaderPanel::OnImGuiRender()
	{
		ImGui::Begin("Shaders");

		ImGui::Checkbox("Auto Reload", &m_AutoReload);
		ImGui::SameLine();
		if (ImGui::Button("Reload All") && m_Library)
			RecordResults(m_Library->ReloadAll());

		const ImVec4 statusColor = m_LastEventSucceeded
			? ImVec4(0.35f, 0.85f, 0.45f, 1.0f)
			: ImVec4(0.95f, 0.35f, 0.30f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, statusColor);
		ImGui::TextWrapped("%s", m_LastEvent.c_str());
		ImGui::PopStyleColor();

		ImGui::Separator();
		if (!m_Library)
		{
			ImGui::TextDisabled("No ShaderLibrary is assigned.");
			ImGui::End();
			return;
		}

		std::vector<std::pair<std::string, Ref<Shader>>> shaders;
		shaders.reserve(m_Library->GetAll().size());
		for (const auto& entry : m_Library->GetAll())
			shaders.push_back(entry);
		std::sort(shaders.begin(), shaders.end(),
			[](const auto& left, const auto& right) {
				return left.first < right.first;
			});

		for (const auto& [name, shader] : shaders)
		{
			ImGui::PushID(shader.get());
			if (ImGui::TreeNode(name.c_str()))
			{
				ImGui::Text("Version: %llu",
					static_cast<unsigned long long>(shader->GetVersion()));
				ImGui::TextWrapped("File: %s",
					shader->IsFileBacked()
						? shader->GetFilePath().string().c_str()
						: "<memory>");

				const ShaderReloadResult& lastResult = shader->GetLastReloadResult();
				if (lastResult.Attempted)
				ImGui::TextWrapped("Status: %s", lastResult.Message.c_str());

				if (shader->IsFileBacked() && ImGui::Button("Reload"))
					RecordResults({ { name, shader->Reload() } });

				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		ImGui::End();
	}

}
