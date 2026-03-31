#include "glpch.h"
#include "Shader.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace gl {
    Shader* Shader::Create(const std::string& vertexSrc, const std::string& fragmentSrc) {
        return new OpenGLShader(vertexSrc, fragmentSrc);
    }
}