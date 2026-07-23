#include "glpch.h"
#include "Shader.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace gl {

	Ref<Shader> Shader::Create(const std::string& filepath)
	{
		return CreateRef<OpenGLShader>(filepath);
	}

	Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		return CreateRef<OpenGLShader>(name, vertexSrc, fragmentSrc);
	}

	Ref<Shader> Shader::CreateFromBinary(const std::string& name, const std::vector<uint32_t>& vertSPV, const std::vector<uint32_t>& fragSPV)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::OpenGL:
			GL_CORE_ASSERT(false, "OpenGL backend does not support SPIR-V. Use Vulkan.");
			return nullptr;
		case RendererAPI::API::Vulkan:
			GL_CORE_ASSERT(false, "Vulkan backend not yet implemented.");
			return nullptr;
		}
		return nullptr;
	}

	void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
	{
		GL_CORE_ASSERT(!Exists(name), "Shader already exists!");
		m_Shaders[name] = shader;
	}

	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		Add(shader->GetName(), shader);
	}

	Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);
		Add(shader);
		return shader;
	}

	Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);
		Add(name, shader);
		return shader;
	}

	Ref<Shader> ShaderLibrary::Get(const std::string& name)
	{
		GL_CORE_ASSERT(Exists(name), "Shader not found!");
		return m_Shaders[name];
	}

	bool ShaderLibrary::Exists(const std::string& name) const
	{
		return m_Shaders.find(name) != m_Shaders.end();
	}

	void ShaderLibrary::Remove(const std::string& name)
	{
		GL_CORE_ASSERT(Exists(name), "Shader not found for removal!");
		m_Shaders.erase(name);
	}

	std::vector<std::pair<std::string, ShaderReloadResult>> ShaderLibrary::ReloadChanged()
	{
		std::vector<std::pair<std::string, ShaderReloadResult>> results;
		for (auto& [name, shader] : m_Shaders)
		{
			ShaderReloadResult result = shader->ReloadIfChanged();
			if (result.Attempted)
				results.emplace_back(name, std::move(result));
		}
		return results;
	}

	std::vector<std::pair<std::string, ShaderReloadResult>> ShaderLibrary::ReloadAll()
	{
		std::vector<std::pair<std::string, ShaderReloadResult>> results;
		results.reserve(m_Shaders.size());
		for (auto& [name, shader] : m_Shaders)
			results.emplace_back(name, shader->Reload());
		return results;
	}

}