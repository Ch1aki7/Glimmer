#pragma once
#include <string>

namespace gl {

    class Shader {
    public:
        Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
        ~Shader();

        void Bind() const;   // 对应 glUseProgram(id)
        void Unbind() const; // 对应 glUseProgram(0)

    private:
        uint32_t m_RendererID; // 显卡返回的程序 ID
    };

}