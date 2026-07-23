#pragma once
#include "Glimmer/Core/FileWatcher.h"
#include "Glimmer/Renderer/ComputeShader.h"

#include <memory>

namespace gl {

	class OpenGLComputeShader : public ComputeShader {
	public:
		explicit OpenGLComputeShader(const std::string& filepath);
		~OpenGLComputeShader() override;

		void Bind() const override;
		void Dispatch(uint32_t x, uint32_t y, uint32_t z) const override;
		void UploadUniformInt(const std::string& name, int value) override;
		void UploadUniformFloat(const std::string& name, float value) override;
		void UploadUniformFloat2(const std::string& name, const glm::vec2& value) override;
		void BindImageTexture(
			uint32_t binding,
			uint32_t textureID,
			uint32_t level,
			ImageAccess access,
			ImageFormat format) override;

		const std::string& GetName() const override { return m_Name; }
		const std::filesystem::path& GetFilePath() const override { return m_FilePath; }
		uint64_t GetVersion() const override { return m_Version; }
		const ShaderReloadResult& GetLastReloadResult() const override { return m_LastReloadResult; }
		ShaderReloadResult Reload() override;
		ShaderReloadResult ReloadIfChanged() override;

		static void Barrier();

	private:
		int GetUniformLocation(const std::string& name) const;
		bool ReadFile(std::string& source, std::string& error) const;
		bool BuildProgram(const std::string& source, uint32_t& program, std::string& error) const;

		uint32_t m_RendererID = 0;
		std::string m_Name;
		std::filesystem::path m_FilePath;
		std::unique_ptr<FileWatcher> m_FileWatcher;
		uint64_t m_Version = 0;
		ShaderReloadResult m_LastReloadResult;
		mutable std::unordered_map<std::string, int> m_UniformCache;
	};

}