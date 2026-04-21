#pragma once
#pragma once
#include "Glimmer.h"
class ExampleLayer : public gl::Layer {
public:
	ExampleLayer();
	virtual ~ExampleLayer() = default;

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
};


