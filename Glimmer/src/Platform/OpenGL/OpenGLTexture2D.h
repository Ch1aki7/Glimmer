#pragma once
#include "Glimmer/Renderer/Texture.h"

#include <glad/glad.h>

namespace gl {

	class OpenGLTexture2D : public Texture2D {
	public:
		OpenGLTexture2D(const std::string& path,
			TextureColorSpace colorSpace = TextureColorSpace::SRGB);
		explicit OpenGLTexture2D(const TextureSpecification& specification);
		~OpenGLTexture2D() override;

		const TextureSpecification& GetSpecification() const override { return m_Specification; }
		uint32_t GetWidth() const override { return m_Specification.Width; }
		uint32_t GetHeight() const override { return m_Specification.Height; }
		TextureFormat GetFormat() const override { return m_Specification.Format; }

		void SetData(const void* data, uint32_t size) override;
		void GetImageData(void* buffer, uint32_t size) const override;
		void Clear(const glm::vec4& value) override;
		void Bind(uint32_t slot = 0) const override;

		bool operator==(const Texture& other) const override
		{
			return m_RendererID == other.GetRendererID();
		}

		uint32_t GetRendererID() const override { return m_RendererID; }

	private:
		void CreateStorage();
		uint32_t GetTransferSize() const;

		TextureSpecification m_Specification;
		std::string m_Path;
		uint32_t m_RendererID = 0;
		GLenum m_InternalFormat = GL_NONE;
		GLenum m_DataFormat = GL_NONE;
		GLenum m_DataType = GL_NONE;
	};

}