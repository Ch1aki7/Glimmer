#pragma once
#include "Glimmer.h"
class EditorLayer : public gl::Layer {
public:
	EditorLayer();
	virtual ~EditorLayer() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

	virtual void OnUpdate(gl::Timestep ts) override;
	virtual void OnImGuiRender() override;
	virtual void OnEvent(gl::Event& event) override;

private:
	gl::ShaderLibrary m_ShaderLib;

	gl::Ref<gl::VertexArray> m_VertexArray;
	gl::Ref<gl::VertexArray> m_bg_vortexVertexArray;

	gl::Ref<gl::Shader> m_TextureShader;
	gl::Ref<gl::Shader> m_bg_vortexShader;
	gl::Ref<gl::Shader> m_TunnelShader;

	gl::Ref<gl::Texture2D> m_Texture;
	gl::Ref<gl::Texture2D> m_STSTexture;
	gl::Ref<gl::Texture2D> m_HenryTexture;
	gl::OrthographicCameraController m_CameraController;

	gl::Ref<gl::Shader> m_3DShader;
	gl::Ref<gl::Shader> m_PhoneShader;
	gl::Ref<gl::Shader> m_ToonShader;
	gl::Ref<gl::Shader> m_BlinnPhongShader;
	gl::Ref<gl::Shader> m_HologramShader;
	int m_SelectedShaderIndex = 0;
	const char* m_ShaderNames[7] = { "Phong", "Toon", "Blinn-Phong", "Hologram", "Normal", "CrossHatch", "InkOutline" };
	gl::Ref<gl::Model> m_MeshModel;
	gl::Ref<gl::Model> m_ChairModel;
	gl::Ref<gl::Model> m_GirlModel;
	gl::Ref<gl::Texture2D> m_TestTexture;
	gl::Ref<gl::Texture2D> m_GirlTexture;

	gl::Ref<gl::Framebuffer> m_Framebuffer;
	gl::Ref<gl::Framebuffer> m_PostProcessFB;

	gl::Ref<gl::Scene> m_ActiveScene;
	gl::Entity m_SquareEntity;
	gl::Entity m_CameraEntity;
	gl::Entity m_SecondCamera;

	bool m_PrimaryCamera = true;

	bool m_PostProcessEnabled = false;
	uint32_t m_FinalSceneTexture = 0;
	bool m_ViewportFocused = false, m_ViewportHovered = false;
	glm::vec2 m_ViewportSize = { 0.0f, 0.0f };

	// 设置灯光参数
	glm::vec3 m_LightPos = { 2.0f, 2.0f, 2.0f };
	glm::vec4 m_SquareColor = { 0.1f, 0.1f, 0.1f, 1 };
};


