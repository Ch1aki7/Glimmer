#pragma once
#include "Glimmer.h"
class Sandbox2D : public gl::Layer {
public:
	Sandbox2D();
	virtual ~Sandbox2D() = default;

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
	gl::OrthographicCameraController m_CameraController;
};


