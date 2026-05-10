#pragma once
#include "Glimmer.h"

namespace gl {

class ExampleLayer : public Layer {
public:
	ExampleLayer();
	virtual ~ExampleLayer() = default;

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
	OrthographicCameraController m_CameraController;
};

}
