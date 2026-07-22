#pragma once
#include "Glimmer/Renderer/ComputeShader.h"
#include <glad/glad.h>

namespace gl {

	class OpenGLComputeShader : public ComputeShader {
	public:
		OpenGLComputeShader(const std::string& filepath);
		virtual ~OpenGLComputeShader();

		virtual void Bind() const override;
		virtual void Dispatch(uint32_t x, uint32_t y, uint32_t z) const override;
		virtual const std::string& GetName() const override { return m_Name; }
		virtual void BindImageTexture(uint32_t binding, uint32_t textureID, uint32_t level, ImageAccess access, ImageFormat format) override;

		static void Barrier();

	private:
		std::string ReadFile(const std::string& filepath);
		void Compile(const std::string& source);

		uint32_t m_RendererID = 0;
		std::string m_Name;
	};

}
