#pragma once
#include "Glimmer/Renderer/Shader.h"

typedef unsigned int GLenum;
typedef int GLint;

namespace gl {
	class OpenGLShader : public Shader {
	public:
		OpenGLShader(const std::string& filepath);
		OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		virtual ~OpenGLShader();

		virtual void Bind() const override;
		virtual void Unbind() const override;

		void UploadUniformInt(const std::string& name, int value) override;
		void UploadUniformIntArray(const std::string& name, int* values, uint32_t count) override;

		void UploadUniformFloat(const std::string& name, float value) override;
		void UploadUniformFloat2(const std::string& name, const glm::vec2& value) override;
		void UploadUniformFloat3(const std::string& name, const glm::vec3& value) override;
		void UploadUniformFloat4(const std::string& name, const glm::vec4& value) override;

		void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) override;

		void BindTexture(const std::string& name, uint32_t slot, uint32_t textureID) override;

		virtual const std::string& GetName() const override { return m_Name; }
	private:
		GLint GetUniformLocation(const std::string& name) const;

		std::string ReadFile(const std::string& filepath);
		std::unordered_map<GLenum, std::string> PreProcess(const std::string& source);
		void Compile(const std::unordered_map<GLenum, std::string>& shaderSources);

		uint32_t m_RendererID;
		std::string m_Name;
		mutable std::unordered_map<std::string, GLint> m_UniformCache;
	};
}
