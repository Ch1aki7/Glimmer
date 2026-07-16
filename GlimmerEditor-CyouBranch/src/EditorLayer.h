#pragma once
#include "Glimmer.h"
#include "SceneHierarchyPanel.h"

namespace gl {

	class EditorLayer : public Layer {
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void OnUpdate(Timestep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& event) override;

	private:
		ShaderLibrary m_ShaderLib;

		Ref<Texture2D> m_Texture;
		Ref<Texture2D> m_STSTexture;
		Ref<Texture2D> m_HenryTexture;
		Ref<Texture2D> m_WhiteTexture; // 修复 DrawIndexed 解绑 slot 0 导致无贴图模型全黑
		OrthographicCameraController m_CameraController;

		Ref<Shader> m_3DShader;
		int m_SelectedShaderIndex = 0;
		const char* m_ShaderNames[4] = { "Phong", "Toon", "Blinn-Phong", "Hologram" };

		// 3D 模型
		std::vector<Ref<Model>> m_Models;
		std::vector<std::string> m_ModelNames;
		int m_SelectedModelIndex = 0;

		Ref<Framebuffer> m_Framebuffer;
		Ref<Framebuffer> m_PostProcessFB;
		bool m_PostProcessEnabled = false;
		uint32_t m_FinalSceneTexture = 0;

		// 场景 & 层级面板
		Ref<Scene> m_ActiveScene;
		SceneHierarchyPanel m_HierarchyPanel;

		// 视口
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];

		// 设置灯光参数
		glm::vec3 m_LightPos = { 2.0f, 2.0f, 2.0f };
		glm::vec4 m_SquareColor = { 0.1f, 0.1f, 0.1f, 1 };
	};

}
