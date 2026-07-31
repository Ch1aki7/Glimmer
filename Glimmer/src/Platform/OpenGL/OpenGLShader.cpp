#include "glpch.h"
#include "OpenGLShader.h"

#include <fstream>
#include <vector>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace gl {

	namespace {

		GLenum ShaderTypeFromString(const std::string& type)
		{
			if (type == "vertex")
				return GL_VERTEX_SHADER;
			if (type == "fragment" || type == "pixel")
				return GL_FRAGMENT_SHADER;
			return 0;
		}

		const char* ShaderStageName(GLenum type)
		{
			switch (type)
			{
			case GL_VERTEX_SHADER: return "vertex";
			case GL_FRAGMENT_SHADER: return "fragment";
			default: return "unknown";
			}
		}

		std::string ProgramNameFromPath(const std::filesystem::path& path)
		{
			return path.stem().string();
		}

	}

	OpenGLShader::OpenGLShader(const std::string& filepath)
		: m_Name(ProgramNameFromPath(filepath)), m_FilePath(filepath),
		  m_FileWatcher(std::make_unique<FileWatcher>(m_FilePath))
	{
		GL_PROFILE_FUNCTION();
		const ShaderReloadResult result = Reload();
		GL_CORE_ASSERT(result.Success, "Initial shader compilation failed: {0}", result.Message);
	}

	OpenGLShader::OpenGLShader(
		const std::string& name,
		const std::string& vertexSrc,
		const std::string& fragmentSrc)
		: m_Name(name)
	{
		GL_PROFILE_FUNCTION();

		std::unordered_map<GLenum, std::string> sources;
		sources[GL_VERTEX_SHADER] = vertexSrc;
		sources[GL_FRAGMENT_SHADER] = fragmentSrc;

		uint32_t program = 0;
		std::string error;
		const bool success = BuildProgram(sources, program, error);
		m_LastReloadResult = { true, success, success ? "Compiled from source." : error };
		if (success)
		{
			m_RendererID = program;
			m_Version = 1;
		}
		GL_CORE_ASSERT(success, "Initial shader compilation failed: {0}", error);
	}

	OpenGLShader::~OpenGLShader()
	{
		GL_PROFILE_FUNCTION();
		if (m_RendererID != 0)
			glDeleteProgram(m_RendererID);
	}

	ShaderReloadResult OpenGLShader::ReloadIfChanged()
	{
		if (!m_FileWatcher || !m_FileWatcher->Poll())
			return {};
		return Reload();
	}

	ShaderReloadResult OpenGLShader::Reload()
	{
		ShaderReloadResult result;
		result.Attempted = true;

		if (!IsFileBacked())
		{
			result.Message = "Shader was created from memory and cannot be reloaded.";
			m_LastReloadResult = result;
			return result;
		}

		std::string source;
		if (!ReadFile(source, result.Message))
		{
			m_LastReloadResult = result;
			return result;
		}

		std::unordered_map<GLenum, std::string> shaderSources;
		if (!PreProcess(source, shaderSources, result.Message))
		{
			m_LastReloadResult = result;
			return result;
		}

		uint32_t newProgram = 0;
		if (!BuildProgram(shaderSources, newProgram, result.Message))
		{
			m_LastReloadResult = result;
			GL_CORE_ERROR("Shader reload failed [{0}]: {1}", m_Name, result.Message);
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
		GL_CORE_INFO("Shader reloaded: {0} (version {1})", m_Name, m_Version);
		return result;
	}

	bool OpenGLShader::ReadFile(std::string& source, std::string& error) const
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
			error = "Shader file is empty: " + m_FilePath.string();
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

		// Normalize UTF-8 shader files before preprocessing. A BOM is not
		// valid GLSL input and is rejected by some OpenGL drivers.
		if (source.size() >= 3 &&
			static_cast<unsigned char>(source[0]) == 0xEF &&
			static_cast<unsigned char>(source[1]) == 0xBB &&
			static_cast<unsigned char>(source[2]) == 0xBF)
		{
			source.erase(0, 3);
		}
		return true;
	}

	bool OpenGLShader::PreProcess(
		const std::string& source,
		std::unordered_map<GLenum, std::string>& shaderSources,
		std::string& error) const
	{
		constexpr const char* typeToken = "#type";
		constexpr size_t typeTokenLength = 5;
		size_t position = source.find(typeToken);
		if (position == std::string::npos)
		{
			error = "Missing #type declarations in " + m_FilePath.string();
			return false;
		}

		while (position != std::string::npos)
		{
			const size_t lineEnd = source.find_first_of("\r\n", position);
			if (lineEnd == std::string::npos)
			{
				error = "Invalid #type declaration.";
				return false;
			}

			const size_t typeBegin = position + typeTokenLength;
			const size_t firstCharacter = source.find_first_not_of(" \t", typeBegin);
			const std::string type = source.substr(firstCharacter, lineEnd - firstCharacter);
			const GLenum shaderType = ShaderTypeFromString(type);
			if (shaderType == 0)
			{
				error = "Unknown shader stage: " + type;
				return false;
			}

			const size_t sourceBegin = source.find_first_not_of("\r\n", lineEnd);
			if (sourceBegin == std::string::npos)
			{
				error = "Shader stage has no source: " + type;
				return false;
			}

			position = source.find(typeToken, sourceBegin);
			shaderSources[shaderType] = source.substr(
				sourceBegin,
				position == std::string::npos ? std::string::npos : position - sourceBegin);
		}

		return !shaderSources.empty();
	}

	bool OpenGLShader::BuildProgram(
		const std::unordered_map<GLenum, std::string>& shaderSources,
		uint32_t& program,
		std::string& error) const
	{
		if (shaderSources.empty())
		{
			error = "No shader stages were provided.";
			return false;
		}

		const GLuint newProgram = glCreateProgram();
		std::vector<GLuint> shaderIDs;
		shaderIDs.reserve(shaderSources.size());

		for (const auto& [type, source] : shaderSources)
		{
			const GLuint shader = glCreateShader(type);
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
				error = std::string(ShaderStageName(type)) + " compile error:\n" + log.data();
				glDeleteShader(shader);
				for (const GLuint id : shaderIDs)
					glDeleteShader(id);
				glDeleteProgram(newProgram);
				return false;
			}

			glAttachShader(newProgram, shader);
			shaderIDs.push_back(shader);
		}

		glLinkProgram(newProgram);
		GLint linked = GL_FALSE;
		glGetProgramiv(newProgram, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE)
		{
			GLint logLength = 0;
			glGetProgramiv(newProgram, GL_INFO_LOG_LENGTH, &logLength);
			std::vector<GLchar> log(static_cast<size_t>(std::max(logLength, 1)));
			glGetProgramInfoLog(newProgram, logLength, nullptr, log.data());
			error = std::string("program link error:\n") + log.data();
			for (const GLuint id : shaderIDs)
				glDeleteShader(id);
			glDeleteProgram(newProgram);
			return false;
		}

		for (const GLuint id : shaderIDs)
		{
			glDetachShader(newProgram, id);
			glDeleteShader(id);
		}

		program = newProgram;
		return true;
	}

	GLint OpenGLShader::GetUniformLocation(const std::string& name) const
	{
		const auto cached = m_UniformCache.find(name);
		if (cached != m_UniformCache.end())
			return cached->second;

		const GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		m_UniformCache[name] = location;
		return location;
	}

	void OpenGLShader::Bind() const
	{
		GL_PROFILE_FUNCTION();
		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const
	{
		GL_PROFILE_FUNCTION();
		glUseProgram(0);
	}

	void OpenGLShader::UploadUniformInt(const std::string& name, int value)
	{
		glUniform1i(GetUniformLocation(name), value);
	}

	void OpenGLShader::UploadUniformIntArray(const std::string& name, int* values, uint32_t count)
	{
		glUniform1iv(GetUniformLocation(name), count, values);
	}

	void OpenGLShader::UploadUniformFloat(const std::string& name, float value)
	{
		glUniform1f(GetUniformLocation(name), value);
	}

	void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& value)
	{
		glUniform2f(GetUniformLocation(name), value.x, value.y);
	}

	void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& value)
	{
		glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
	}

	void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& value)
	{
		glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
	}

	void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
	{
		glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(matrix));
	}

	void OpenGLShader::BindTexture(
		const std::string& name,
		uint32_t slot,
		uint32_t textureID)
	{
		UploadUniformInt(name, static_cast<int>(slot));
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, textureID);
	}

}
