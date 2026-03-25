// SandboxApp.cpp
#include <Glimmer.h>
#include <iostream>

// 继承 Glimmer 的引擎基类
class Sandbox : public gl::Application {
public:
    Sandbox() {
        std::cout << "Glimmer Engine Initialized! Hello World!" << std::endl;
    }
    ~Sandbox() {}
};

// 告诉引擎，我要启动这个沙盒游戏
gl::Application* gl::CreateApplication() {
    return new Sandbox();
}
