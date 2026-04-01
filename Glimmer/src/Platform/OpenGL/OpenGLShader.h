#pragma once
#include "Glimmer/Renderer/Shader.h"

typedef unsigned int GLenum;

namespace gl {
    class OpenGLShader : public Shader {
    public:
		OpenGLShader(const std::string& filepath);
		OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
        virtual ~OpenGLShader();

        virtual void Bind() const override;
        virtual void Unbind() const override;

        // 上传 Uniform 的接口
        void UploadUniformInt(const std::string& name, int value);

        void UploadUniformFloat(const std::string& name, float value) override;
        void UploadUniformFloat2(const std::string& name, const glm::vec2& value);
        void UploadUniformFloat3(const std::string& name, const glm::vec3& value);
        void UploadUniformFloat4(const std::string& name, const glm::vec4& value);

        void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) override;

		virtual const std::string& GetName() const override { return m_Name; }
    private:
		std::string ReadFile(const std::string& filepath);
		std::unordered_map<GLenum, std::string> PreProcess(const std::string& source);
		void Compile(const std::unordered_map<GLenum, std::string>& shaderSources);

        uint32_t m_RendererID;
		std::string m_Name; // 用于 Shader 库标识
    };
}
