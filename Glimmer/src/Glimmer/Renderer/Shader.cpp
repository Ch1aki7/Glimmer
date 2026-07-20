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

	// --- ShaderLibrary 实现 ---

	void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
	{
		GL_CORE_ASSERT(!Exists(name), "Shader already exists!");
		m_Shaders[name] = shader;
	}

	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		auto& name = shader->GetName();
		Add(name, shader);
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
}
