// SandboxApp.cpp
#include <Glimmer.h>
#include "Glimmer/Core/EntryPoint.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include <glm/gtc/type_ptr.hpp>
#include "Sandbox2D.h"
#include "ExampleLayer.h"

// 继承 Glimmer 的引擎基类
class Sandbox : public gl::Application {
public:
    Sandbox() 
    {
		PushLayer(new Sandbox2D());
		//PushLayer(new ExampleLayer());
    }
    ~Sandbox() {}
};

// 告诉引擎，我要启动这个沙盒游戏
gl::Application* gl::CreateApplication() 
{
    return new Sandbox();
}
