// SandboxApp.cpp
#include <Glimmer.h>
#include <iostream>

class ExampleLayer : public gl::Layer {
public:
    ExampleLayer() : Layer("Example") {}

    void OnUpdate() override {
         //GL_INFO("ExampleLayer::Update");
    }

    void OnEvent(gl::Event& event) override {
        GL_TRACE("{0}", event.ToString());
    }

    void OnImGuiRender() override {
        // 画一个经典的测试面板
        ImGui::Begin("Glimmer Test Window");
        ImGui::Text("Hello World! ImGui is Working!");
        ImGui::End();

        // 也可以直接调出 ImGui 自带的超级演示窗口，看看它有多强大：
        bool show_demo_window = true;
        ImGui::ShowDemoWindow(&show_demo_window);
    }
};

// 继承 Glimmer 的引擎基类
class Sandbox : public gl::Application {
public:
    Sandbox() {
        std::cout << "Glimmer Engine Initialized! Hello World!" << std::endl;
        PushLayer(new ExampleLayer());
    }
    ~Sandbox() {}
};

// 告诉引擎，我要启动这个沙盒游戏
gl::Application* gl::CreateApplication() {
    return new Sandbox();
}
