// SandboxApp.cpp
#include <Glimmer.h>
#include "Glimmer/Core/EntryPoint.h"
#include "Platform/OpenGL/OpenGLShader.h"
#include <glm/gtc/type_ptr.hpp>
#include "Sandbox2D.h"
class ExampleLayer : public gl::Layer {
public:
    void OnUpdate(gl::Timestep ts) override {
    }

    void OnEvent(gl::Event& event) override {
    }

    void OnImGuiRender() override {
    }
private:
};

// 继承 Glimmer 的引擎基类
class Sandbox : public gl::Application {
public:
    Sandbox() 
    {
		PushLayer(new Sandbox2D());
    }
    ~Sandbox() {}
};

// 告诉引擎，我要启动这个沙盒游戏
gl::Application* gl::CreateApplication() 
{
    return new Sandbox();
}
