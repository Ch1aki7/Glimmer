#pragma once
#include "Glimmer.h"

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

	Ref<VertexArray> m_VertexArray;
	Ref<VertexArray> m_bg_vortexVertexArray;

	Ref<Shader> m_TextureShader;
	Ref<Shader> m_bg_vortexShader;
	Ref<Shader> m_TunnelShader;

	Ref<Texture2D> m_Texture;
	Ref<Texture2D> m_STSTexture;
	Ref<Texture2D> m_HenryTexture;
	Ref<Texture2D> m_NoiseSource;
	OrthographicCameraController m_CameraController;

	Ref<Shader> m_3DShader;
	Ref<Shader> m_PhoneShader;
	Ref<Shader> m_ToonShader;
	Ref<Shader> m_BlinnPhongShader;
	Ref<Shader> m_HologramShader;
	int m_SelectedShaderIndex = 0;
	const char* m_ShaderNames[7] = { "Phong", "Toon", "Blinn-Phong", "Hologram", "Normal", "CrossHatch", "InkOutline" };
	Ref<Model> m_MeshModel;
	Ref<Model> m_ChairModel;
	Ref<Model> m_GirlModel;
	Ref<Texture2D> m_TestTexture;
	Ref<Texture2D> m_GirlTexture;

	Ref<Framebuffer> m_Framebuffer;
	Ref<Framebuffer> m_PostProcessFB;

	Ref<Scene> m_ActiveScene;
	Entity m_SquareEntity;
	Entity m_CameraEntity;
	Entity m_SecondCamera;

	bool m_PrimaryCamera = true;

	bool m_PostProcessEnabled = false;
	uint32_t m_FinalSceneTexture = 0;
	bool m_ViewportFocused = false, m_ViewportHovered = false;
	glm::vec2 m_ViewportSize = { 0.0f, 0.0f };

	// 设置灯光参数
	glm::vec3 m_LightPos = { 2.0f, 2.0f, 2.0f };
	glm::vec4 m_SquareColor = { 0.1f, 0.1f, 0.1f, 1 };
};

}
