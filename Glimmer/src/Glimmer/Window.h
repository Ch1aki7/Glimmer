#pragma once

#include "glpch.h"
#include "Glimmer/Core.h"
#include "Glimmer/Events/Event.h"

namespace gl {

    struct WindowProps {
        std::string Title;
        unsigned int Width;
        unsigned int Height;

        WindowProps(const std::string& title = "Glimmer Engine",
            unsigned int width = 1280,
            unsigned int height = 720)
            : Title(title), Width(width), Height(height) {}
    };

    // 窗口接口：由不同平台（Windows, Linux, Mac）去具体实现
    class Window {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~Window() {}

        virtual void OnUpdate() = 0;

        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;

        // 设置事件回调函数（非常关键：用于把 GLFW 事件传回 Application）
        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void SetVSync(bool enabled) = 0;
        virtual bool IsVSync() const = 0;

        // 静态工厂方法：在不同平台下创建对应的窗口实例
        static Window* Create(const WindowProps& props = WindowProps());

        virtual void* GetNativeWindow() const = 0; // 返回底层窗口句柄
    };
}