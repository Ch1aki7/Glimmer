#include "glpch.h"
#include "OpenGLComputeShader.h"

#include <fstream>
#include <glad/glad.h>

namespace gl {

	OpenGLComputeShader::OpenGLComputeShader(const std::string& filepath)
	{
		std::string source = ReadFile(filepath);
		Compile(source);

		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);
	}

	OpenGLComputeShader::~OpenGLComputeShader()
	{
		glDeleteProgram(m_RendererID);
	}

	std::string OpenGLComputeShader::ReadFile(const std::string& filepath)
	{
		std::string result;
		std::ifstream in(filepath, std::ios::in | std::ios::binary);
		if (in)
		{
			in.seekg(0, std::ios::end);
			result.resize(in.tellg());
			in.seekg(0, std::ios::beg);
			in.read(&result[0], result.size());
			in.close();
		}
		else
		{
			GL_CORE_ERROR("Could not open file '{0}'", filepath);
		}
		return result;
	}

	void OpenGLComputeShader::Compile(const std::string& source)
	{
		GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
		const GLchar* src = source.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);

		GLint compiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
		if (compiled == GL_FALSE)
		{
			GLint maxLen = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLen);
			std::vector<GLchar> log(maxLen);
			glGetShaderInfoLog(shader, maxLen, &maxLen, log.data());
			glDeleteShader(shader);
			GL_CORE_ERROR("{0}", log.data());
			GL_CORE_ASSERT(false, "Compute shader compilation failure!");
			return;
		}

		GLuint program = glCreateProgram();
		glAttachShader(program, shader);
		glLinkProgram(program);

		GLint linked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, &linked);
		if (linked == GL_FALSE)
		{
			GLint maxLen = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLen);
			std::vector<GLchar> log(maxLen);
			glGetProgramInfoLog(program, maxLen, &maxLen, log.data());
			glDeleteProgram(program);
			glDeleteShader(shader);
			GL_CORE_ERROR("{0}", log.data());
			GL_CORE_ASSERT(false, "Compute shader link failure!");
			return;
		}

		glDetachShader(program, shader);
		glDeleteShader(shader);
		m_RendererID = program;
	}

	void OpenGLComputeShader::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void OpenGLComputeShader::Dispatch(uint32_t x, uint32_t y, uint32_t z) const
	{
		glDispatchCompute(x, y, z);
	}

	void OpenGLComputeShader::BindImageTexture(uint32_t binding, uint32_t textureID, uint32_t level, ImageAccess access, ImageFormat format)
	{
		static GLenum glAccess[] = { GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE };
		static GLenum glFormat[] = { GL_RGBA8, GL_RGBA16F, GL_RGBA32F, GL_R32F };

		glBindImageTexture(binding, textureID, level, GL_FALSE, 0,
		                   glAccess[(int)access],
		                   glFormat[(int)format]);
	}

	void OpenGLComputeShader::Barrier()
	{
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
		              | GL_SHADER_STORAGE_BARRIER_BIT
		              | GL_TEXTURE_FETCH_BARRIER_BIT);
	}

}
