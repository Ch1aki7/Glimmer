#pragma once
#include "Glimmer/Core/Core.h"
#include "Glimmer/Renderer/ShaderReload.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <glm/glm.hpp>

namespace gl {


	class Shader {
	public:
		virtual ~Shader() = default;

		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual void UploadUniformInt(const std::string& name, int value) = 0;
		virtual void UploadUniformIntArray(const std::string& name, int* values, uint32_t count) = 0;
		virtual void UploadUniformFloat(const std::string& name, float value) = 0;
		virtual void UploadUniformFloat2(const std::string& name, const glm::vec2& value) = 0;
		virtual void UploadUniformFloat3(const std::string& name, const glm::vec3& value) = 0;
		virtual void UploadUniformFloat4(const std::string& name, const glm::vec4& value) = 0;
		virtual void UploadUniformMat4(const std::string& name, const glm::mat4& matrix) = 0;
		virtual void BindTexture(const std::string& name, uint32_t slot, uint32_t textureID) = 0;

		virtual const std::string& GetName() const = 0;
		virtual const std::filesystem::path& GetFilePath() const = 0;
		virtual uint64_t GetVersion() const = 0;
		virtual const ShaderReloadResult& GetLastReloadResult() const = 0;
		virtual bool IsFileBacked() const = 0;
		virtual ShaderReloadResult Reload() = 0;
		virtual ShaderReloadResult ReloadIfChanged() = 0;

		static Ref<Shader> Create(const std::string& filepath);
		static Ref<Shader> Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		static Ref<Shader> CreateFromBinary(const std::string& name, const std::vector<uint32_t>& vertSPV, const std::vector<uint32_t>& fragSPV);
	};

	class ShaderLibrary {
	public:
		void Add(const std::string& name, const Ref<Shader>& shader);
		void Add(const Ref<Shader>& shader);

		Ref<Shader> Load(const std::string& filepath);
		Ref<Shader> Load(const std::string& name, const std::string& filepath);

		Ref<Shader> Get(const std::string& name);
		const std::unordered_map<std::string, Ref<Shader>>& GetAll() const { return m_Shaders; }

		void Remove(const std::string& name);
		bool Exists(const std::string& name) const;

		std::vector<std::pair<std::string, ShaderReloadResult>> ReloadChanged();
		std::vector<std::pair<std::string, ShaderReloadResult>> ReloadAll();

	private:
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	};

}