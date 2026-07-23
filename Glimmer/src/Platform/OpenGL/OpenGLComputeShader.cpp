#include "glpch.h"
#include "OpenGLComputeShader.h"

#include <algorithm>
#include <fstream>
#include <vector>
#include <glad/glad.h>

namespace gl {

	OpenGLComputeShader::OpenGLComputeShader(const std::string& filepath)
		: m_Name(std::filesystem::path(filepath).stem().string()),
		  m_FilePath(filepath),
		  m_FileWatcher(std::make_unique<FileWatcher>(m_FilePath))
	{
		const ShaderReloadResult result = Reload();
		GL_CORE_ASSERT(result.Success, "Initial compute shader compilation failed: {0}", result.Message);
	}

	OpenGLComputeShader::~OpenGLComputeShader()
	{
		if (m_RendererID != 0)
			glDeleteProgram(m_RendererID);
	}

	ShaderReloadResult OpenGLComputeShader::ReloadIfChanged()
	{
		if (!m_FileWatcher || !m_FileWatcher->Poll())
			return {};
		return Reload();
	}

	ShaderReloadResult OpenGLComputeShader::Reload()
	{
		ShaderReloadResult result;
		result.Attempted = true;

		std::string source;
		if (!ReadFile(source, result.Message))
		{
			m_LastReloadResult = result;
			return result;
		}

		uint32_t newProgram = 0;
		if (!BuildProgram(source, newProgram, result.Message))
		{
			m_LastReloadResult = result;
			GL_CORE_ERROR("Compute shader reload failed [{0}]: {1}", m_Name, result.Message);
			return result;
		}

		const uint32_t oldProgram = m_RendererID;
		m_RendererID = newProgram;
		m_UniformCache.clear();
		++m_Version;
		if (oldProgram != 0)
			glDeleteProgram(oldProgram);

		result.Success = true;
		result.Message = "Reloaded successfully.";
		m_LastReloadResult = result;
		GL_CORE_INFO("Compute shader reloaded: {0} (version {1})", m_Name, m_Version);
		return result;
	}

	bool OpenGLComputeShader::ReadFile(std::string& source, std::string& error) const
	{
		std::ifstream input(m_FilePath, std::ios::in | std::ios::binary);
		if (!input)
		{
			error = "Could not open file: " + m_FilePath.string();
			return false;
		}

		input.seekg(0, std::ios::end);
		const std::streampos size = input.tellg();
		if (size <= 0)
		{
			error = "Compute shader file is empty: " + m_FilePath.string();
			return false;
		}

		source.resize(static_cast<size_t>(size));
		input.seekg(0, std::ios::beg);
		input.read(source.data(), size);
		if (!input)
		{
			error = "Could not read file: " + m_FilePath.string();
			return false;
		}
		return true;
	}

	bool OpenGLComputeShader::BuildProgram(
		const std::string& source,
		uint32_t& program,
		std::string& error) const
	{
		const GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
		const GLchar* sourceData = source.c_str();
		glShaderSource(shader, 1, &sourceData, nullptr);
		glCompileShader(shader);

		GLint compiled = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE)
		{
			GLint logLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
			std::vector<GLchar> log(static_cast<size_t>(std::max(logLength, 1)));
			glGetShaderInfoLog(shader, logLength, nullptr, log.data());
			error = std::string("compute compile error:\n") + log.data();
			glDeleteShader(shader);
			return false;
		}

		const GLuint newProgram = glCreateProgram();
		glAttachShader(newProgram, shader);
		glLinkProgram(newProgram);

		GLint linked = GL_FALSE;
		glGetProgramiv(newProgram, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE)
		{
			GLint logLength = 0;
			glGetProgramiv(newProgram, GL_INFO_LOG_LENGTH, &logLength);
			std::vector<GLchar> log(static_cast<size_t>(std::max(logLength, 1)));
			glGetProgramInfoLog(newProgram, logLength, nullptr, log.data());
			error = std::string("compute program link error:\n") + log.data();
			glDeleteShader(shader);
			glDeleteProgram(newProgram);
			return false;
		}

		glDetachShader(newProgram, shader);
		glDeleteShader(shader);
		program = newProgram;
		return true;
	}

	void OpenGLComputeShader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void OpenGLComputeShader::Dispatch(uint32_t x, uint32_t y, uint32_t z) const
	{
		glDispatchCompute(x, y, z);
	}

	int OpenGLComputeShader::GetUniformLocation(const std::string& name) const
	{
		const auto cached = m_UniformCache.find(name);
		if (cached != m_UniformCache.end())
			return cached->second;
		const int location = glGetUniformLocation(m_RendererID, name.c_str());
		m_UniformCache[name] = location;
		return location;
	}

	void OpenGLComputeShader::UploadUniformInt(const std::string& name, int value)
	{
		glUniform1i(GetUniformLocation(name), value);
	}

	void OpenGLComputeShader::UploadUniformFloat(const std::string& name, float value)
	{
		glUniform1f(GetUniformLocation(name), value);
	}

	void OpenGLComputeShader::UploadUniformFloat2(const std::string& name, const glm::vec2& value)
	{
		glUniform2f(GetUniformLocation(name), value.x, value.y);
	}
	void OpenGLComputeShader::BindImageTexture(
		uint32_t binding,
		uint32_t textureID,
		uint32_t level,
		ImageAccess access,
		ImageFormat format)
	{
		static constexpr GLenum glAccess[] = { GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE };
		static constexpr GLenum glFormat[] = { GL_RGBA8, GL_RGBA16F, GL_RGBA32F, GL_R32F };

		glBindImageTexture(
			binding,
			textureID,
			static_cast<GLint>(level),
			GL_FALSE,
			0,
			glAccess[static_cast<size_t>(access)],
			glFormat[static_cast<size_t>(format)]);
	}

	void OpenGLComputeShader::Barrier()
	{
		glMemoryBarrier(
			GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
			GL_SHADER_STORAGE_BARRIER_BIT |
			GL_TEXTURE_FETCH_BARRIER_BIT);
	}

}