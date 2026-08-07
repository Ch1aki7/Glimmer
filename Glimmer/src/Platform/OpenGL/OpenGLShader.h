#pragma once
#include "Glimmer/Core/FileWatcher.h"
#include "Glimmer/Renderer/Shader.h"

#include <memory>

typedef unsigned int GLenum;
typedef int GLint;

namespace gl {

	class OpenGLShader : public Shader {
	public:
		explicit OpenGLShader(const std::string& filepath);
		OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		~OpenGLShader() override;

		void Bind() const override;
		void Unbind() const override;

		void UploadUniformInt(const std::string& name, int value) override;
		void UploadUniformIntArray(const std::string& name, int* values, uint32_t count) override;
		void UploadUniformFloat(const std::string& name, float value) override;
		void UploadUniformFloat2(const std::string& name, const glm::vec2& value) override;
		void UploadUniformFloat3(const std::string& name, const glm::vec3& value) override;
		void UploadUniformFloat4(const std::string& name, const glm::vec4& value) override;
		void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) override;
		void BindTexture(const std::string& name, uint32_t slot, uint32_t textureID) override;

		const std::string& GetName() const override { return m_Name; }
		const std::filesystem::path& GetFilePath() const override { return m_FilePath; }
		uint64_t GetVersion() const override { return m_Version; }
		bool SupportsInstancing() const override { return m_SupportsInstancing; }
		const ShaderReloadResult& GetLastReloadResult() const override { return m_LastReloadResult; }
		bool IsFileBacked() const override { return !m_FilePath.empty(); }
		ShaderReloadResult Reload() override;
		ShaderReloadResult ReloadIfChanged() override;

	private:
		GLint GetUniformLocation(const std::string& name) const;
		bool ReadFile(std::string& source, std::string& error) const;
		bool PreProcess(
			const std::string& source,
			std::unordered_map<GLenum, std::string>& shaderSources,
			std::string& error) const;
		bool BuildProgram(
			const std::unordered_map<GLenum, std::string>& shaderSources,
			uint32_t& program,
			std::string& error) const;
		void UpdateCapabilities();

		uint32_t m_RendererID = 0;
		std::string m_Name;
		std::filesystem::path m_FilePath;
		std::unique_ptr<FileWatcher> m_FileWatcher;
		uint64_t m_Version = 0;
		bool m_SupportsInstancing = false;
		ShaderReloadResult m_LastReloadResult;
		mutable std::unordered_map<std::string, GLint> m_UniformCache;
	};

}
