# Glimmer

目标技术栈

1. **核心语言**：Modern C++（大量使用 C++11/14/17 的智能指针、Lambda、模板元编程）。
2. **构建系统**：**Premake**（用于一键生成 Visual Studio 或 Xcode 的工程文件，比 CMake 更适合游戏引擎）。
3. **窗口与输入**：**GLFW**（跨平台的窗口创建库，处理键盘/鼠标/手柄输入，行业标配）。
4. **图形 API**：**OpenGL**（主要教学用，后期架构设计为随时可无缝切换到 Vulkan 或 DirectX 11/12）。
5. **数学库**：**GLM** (OpenGL Mathematics)（处理 Vector3、四元数、矩阵乘法，和 Unity 的 Mathf、Vector3 用法非常像）。
6. **UI 库（极度重要）**：**Dear ImGui**（C++ 界最著名的即时渲染 UI 库，Hazel Editor 的所有面板、按钮、节点连线全是用它画出来的）。
7. **日志系统**：**spdlog**（极速的多线程 C++ 日志库，用于在控制台输出带颜色的 Debug 信息）。
8. **事件系统**：自定义的宏定义与事件总线（Event Bus，处理窗口缩放、按键按下等底层事件）。

目标流程

阶段一：奠基（架构与核心层构建）

- **目标**：搭好骨架，让引擎能跨平台运行（Windows/Mac），并能弹出一个黑色的窗口。
- **关键内容**：搭建 Premake 构建系统（这是大型 C++ 项目的基础，不用手动去配置几百个包含目录）。设计引擎的“入口点（Entry Point）”，隐藏 C++ 原生的 main() 函数，让用户只需要继承 Application 类就能跑起游戏。引入 spdlog，打造一个带颜色、能分类过滤的核心日志系统。设计事件系统（Event System）：使用阻塞（Blocking）和分发（Dispatcher）机制，处理鼠标点击、窗口拖拽。集成 GLFW 库，成功弹出一个窗口！实现 Layer（图层）机制，分离游戏逻辑层和 UI 叠加层（Overlay）。

阶段二：点亮屏幕（图形学与渲染架构）

- **目标**：不仅能画出三角形，还要设计一套**与具体图形 API（OpenGL/Vulkan）解耦**的高级渲染架构。
- **关键内容**：集成 ImGui，终于能在黑色窗口上画出调试按钮和性能监控面板了。数学基础：引入 GLM，学习正交摄像机（Orthographic Camera）和透视摄像机（Perspective Camera）的矩阵推导（投影矩阵 * 视图矩阵 * 模型矩阵）。编写 Shader 类：从硬盘读取 .glsl 文件，编译并绑定到显卡。**架构神来之笔**：抽象出 RendererAPI、VertexArray、Buffer 等基类。这意味着上层写游戏逻辑时完全不知道底层是 OpenGL 还是 DX，这叫“渲染器后端解耦（Render Backend Agnostic）”。最终：在屏幕上成功画出一个贴着木箱子纹理的旋转正方形！

阶段三：起飞（2D 批处理渲染器 Batch Renderer）

- **目标**：性能优化！从“画一个正方形”进化到“一帧瞬间画出 10000 个精灵图且完全不卡”。
- **关键内容**：如果不做批处理，画 10000 棵草需要向显卡发送 10000 次渲染指令（Draw Call），游戏直接卡死。Cherno 手写了一个非常硬核的 **2D 批处理渲染器（Batch Renderer 2D）**。原理：在内存中把这 10000 棵草的顶点数据拼成一个超级巨大的数组（Buffer），然后只用 **1 次 Draw Call** 发送给显卡！这极大提升了引擎性能。实现纹理槽位（Texture Slots）管理：一次 Draw Call 最多绑定 32 张不同的贴图。

阶段四：灵魂注入（实体组件系统 ECS 与场景管理）

- **目标**：抛弃 Hardcode（硬编码），让引擎像 Unity 一样好用，拥有 GameObject 和挂载脚本的能力。

- **关键内容**：

  不再使用传统的面向对象继承（OOP），而是引入 **ECS（Entity-Component-System）** 架构。

  使用第三方神库 **EnTT** 作为底层的 ECS 管理器。实现 Scene（场景类）和 Entity（实体类）。

  实现各种核心组件：TransformComponent（位置、旋转、缩放）、SpriteRendererComponent（图片渲染）、CameraComponent（摄像机属性）。此时，引擎内部终于有了“往场景里添加一个物体，然后给它挂组件”的概念。

阶段五：惊艳亮相（Hazel Editor 可视化编辑器）

- **目标**：打造一个长得极像 Unity 的独立编辑器软件！

- **关键内容**：

  使用 ImGui 的高级功能（Docking 停靠分支），实现编辑器窗口的拖拽、吸附。

  **Viewport（视口）面板**：把 OpenGL 渲染出的游戏画面，映射到一个 UI 窗口里（Framebuffer 技术），并在上面覆盖网格线（Grid）。

  **Scene Hierarchy（层级面板）**：显示场景里所有的 Entity，点击可以选中。

  **Inspector（属性面板）**：选中 Entity 后，利用 C++ 的反射/宏魔法，在面板上自动生成 Transform 的 XYZ 滑动条、颜色选择器、添加组件按钮。

  **Gizmos（小部件）**：集成 ImGuizmo 库，实现像 Unity 那样的拖拽箭头移动/旋转/缩放物体的功能！

## Hello World!

在 Glimmer/src 文件夹下，右键新建一个类 Application.h 和 Application.cpp。

```
// Application.h
namespace gl { // 属于 Glimmer 引擎的命名空间
    class Application {
    public:
        Application();
        virtual ~Application();
        void Run();
    };
    // 提供给外部创建应用的接口
    Application* CreateApplication(); 
}
```

```
// Application.cpp
#include "Application.h"
#include <iostream>

namespace gl {
    Application::Application() {}
    Application::~Application() {}

    void Application::Run() {
        while (true) {
            // 这里将是未来游戏的心脏：Game Loop
        }
    }
}
```

在 Sandbox/src 下新建一个 SandboxApp.cpp。这是用引擎写的“第一款游戏”：

```
// SandboxApp.cpp
#include <Application.h>
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

// 真正的入口点！
int main() {
    gl::Application* app = gl::CreateApplication();
    app->Run();
    delete app;
    return 0;
}
```

![image-20260324181422163](README.assets/image-20260324181422163.png)

当我们按下 F5 时，C++ 编译器到底干了什么？

1. **编译引擎 (Build Glimmer)**：编译器首先来到 Glimmer 项目。它读取了 Application.cpp，把里面的 C++ 源码翻译成计算机认识的机器码。因为我们在 premake5.lua 中把 Glimmer 设置为了 kind "StaticLib"（静态库）。所以，编译器并没有生成一个可以双击运行的 .exe 程序，而是把所有机器码打包压缩成了一个 **Glimmer.lib** 文件（放在了隐藏的 bin 目录下）。*这就好比 Unity 官方写好了引擎的底层代码，打包成了一个巨大的 UnityEngine.dll 供你调用。*
2. **编译游戏 (Build Sandbox)**：接着，编译器来到 Sandbox 项目。它读取了 SandboxApp.cpp。当编译器看到 #include <Application.h> 时，它会去 Glimmer/src 目录下找到这个头文件（因为我们在 Premake 里配置了 includedirs）。头文件就像一本“说明书”，告诉 Sandbox：“Glimmer 引擎里确实有一个叫 Application 的类，你可以用它。”
3. **最终链接 (Linking)**：Sandbox 的代码编译完后，它只是知道了引擎长什么样，但**没有引擎的实际运行逻辑**。这时候，**链接器（Linker）**出场了！因为我们在 Premake 中写了 links { "Glimmer" }。链接器把刚刚生成的 Glimmer.lib（引擎的肉体）和 Sandbox 编译出的机器码死死地缝合在一起！最后，生成了一个完整的、包含引擎所有底层的 **Sandbox.exe**。你双击运行的，正是这个文件。

代码逻辑层面（控制权反转）

这是引擎开发中最精妙、最核心的架构设计思想：**控制反转（Inversion of Control, IoC）**。

```
// 在 SandboxApp.cpp 中 (游戏的入口点)

int main() {
    // 1. 游戏向引擎请求：给我创建一个应用实例！
    gl::Application* app = gl::CreateApplication(); 
    
    // 2. 游戏把控制权交还给引擎的 Run() 函数！
    app->Run(); 
    
    // 3. 游戏结束，清理内存
    delete app;
    return 0;
}
```

多态的威力（虚函数与指针）

```
// SandboxApp.cpp 中
gl::Application* gl::CreateApplication() {
    return new Sandbox(); // 返回的竟然是 Sandbox！
}
```

- CreateApplication 是定义在 Glimmer 引擎里的函数。它规定：**必须返回一个 gl::Application 类型的指针。**
- 但是，我们在 Sandbox 里实现这个函数时，new 出来的却是 Sandbox 类！
- **为什么可以这样？** 因为 class Sandbox : public gl::Application（Sandbox 继承了 Application）。
- 这就是 C++ 的多态性（Polymorphism）。引擎拿到这个指针后，以为自己在操作一个普通的 Application，但实际上它在操作你写的 Sandbox 游戏！这就允许引擎在未来去调用 Sandbox 重写的那些虚函数（比如 virtual void OnUpdate()）。

**引擎被打包成了库 -> 游戏链接了这个库 -> 游戏在** **main** **函数里把控制权上交给了引擎 -> 引擎开始死循环接管世界！**

## 入口点

目前的 main 函数写在 SandboxApp.cpp 里，这意味着用户（游戏开发者）必须知道怎么初始化引擎、怎么调用 Run()，但是这都是可以再次简化的，所以创建EntryPoint.h，这个文件将包含真正的 int main()。

```
#pragma once

#ifdef GL_PLATFORM_WINDOWS

extern gl::Application* gl::CreateApplication();

int main(int argc, char** argv)
{
    auto app = gl::CreateApplication();

    app->Run();

    delete app;
}

#endif
```

以及最重要的注入宏定义，不然等于没写，可以在visual studio每个项目加入预处理器定义，这里我采用在premake.lua脚本添加

```
workspace "GlimmerEngine"
    -- ... (前面的配置不变) ...

-- 1. 引擎项目
project "Glimmer"
    -- ... (前面的配置不变) ...

    -- 【新增】：告诉编译器，如果我们是在 Windows 上编译，就定义 GL_PLATFORM_WINDOWS
    filter "system:windows"
        systemversion "latest"
        defines {
            "GL_PLATFORM_WINDOWS",
            "GL_BUILD_DLL" -- 预留，虽然我们现在是静态库
        }

-- 2. 沙盒项目
project "Sandbox"
    -- ... (前面的配置不变) ...

    -- 【新增】：沙盒也需要知道自己是在 Windows 上
    filter "system:windows"
        systemversion "latest"
        defines {
            "GL_PLATFORM_WINDOWS"
        }
```

## 日志系统

首先引入子模块

```
git submodule add https://github.com/gabime/spdlog.git Glimmer/vendor/spdlog
```

然后告诉premake引擎现在需要包含这个新文件夹

```
project "Glimmer"
    -- ... 其他配置不变 ...
    includedirs {
        "%{prj.name}/src",
        "%{prj.name}/vendor/spdlog/include" -- 新增这一行
    }
    
        includedirs {
        "Glimmer/src", -- 沙盒需要引用引擎的代码
        "Glimmer/vendor/spdlog/include"
    }
```

新建 Log.h 和 Log.cpp

```
#pragma once
#include "spdlog/spdlog.h"
#include <memory>

namespace gl {
	class Log
	{
    public:
        static void Init();
        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
};

```

https://github.com/gabime/spdlog/wiki/Custom-formatting在wiki中可以查看格式设置

```
#include "Log.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace gl {
    std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
    std::shared_ptr<spdlog::logger> Log::s_ClientLogger;

    void Log::Init() {
        spdlog::set_pattern("%^[%T] %n: %v%$"); // 设置日志格式：时间-名称-内容
        s_CoreLogger = spdlog::stdout_color_mt("GLIMMER");
        s_CoreLogger->set_level(spdlog::level::trace);

        s_ClientLogger = spdlog::stdout_color_mt("APP");
        s_ClientLogger->set_level(spdlog::level::trace);
    }
}
```

出现'Unicode support requires compiling with /utf-8'报错

这是因为中文版 Windows 的 Visual Studio (MSVC) 使用的是 GBK（或者叫 System Codepage）编码。当 spdlog 发现你没有开启 UTF-8 支持时，它就会通过 static_assert 故意让编译失败，以防你的日志输出变成乱码。

修改 **premake5.lua**

```
workspace "GlimmerEngine"
    -- ... (之前的配置) ...

    -- 【新增】：为 MSVC 编译器开启 UTF-8 支持
    filter "system:windows"
        buildoptions { "/utf-8" } -- 这一行是解决报错的关键
        systemversion "latest"
        defines {
            "GL_PLATFORM_WINDOWS"
        }
```

测试

![image-20260325162630699](README.assets/image-20260325162630699.png)

在Log.h中定义新的宏

```
// 引擎层日志宏 (Core)
#define GL_CORE_ERROR(...)  ::gl::Log::GetCoreLogger()->error(__VA_ARGS__)
#define GL_CORE_WARN(...)   ::gl::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define GL_CORE_INFO(...)   ::gl::Log::GetCoreLogger()->info(__VA_ARGS__)

// 游戏层日志宏 (Client)
#define GL_ERROR(...)       ::gl::Log::GetClientLogger()->error(__VA_ARGS__)
#define GL_INFO(...)        ::gl::Log::GetClientLogger()->info(__VA_ARGS__)
```

在Log.cpp加入

```
        GL_CORE_INFO(R"(
 ****** ****** ****** ****** ****** ****** ****** ****** ****** ****** 
////// ////// ////// ////// ////// ////// ////// ////// ////// //////                                                                        
                                                                       
   ********  **       ** ****     **** ****     **** ******** *******  
  **//////**/**      /**/**/**   **/**/**/**   **/**/**///// /**////** 
 **      // /**      /**/**//** ** /**/**//** ** /**/**      /**   /** 
/**         /**      /**/** //***  /**/** //***  /**/******* /*******  
/**    *****/**      /**/**  //*   /**/**  //*   /**/**////  /**///**  
//**  ////**/**      /**/**   /    /**/**   /    /**/**      /**  //** 
 //******** /********/**/**        /**/**        /**/********/**   //**
  ////////  //////// // //         // //         // //////// //     //                                                                       
                                                                       
 ****** ****** ****** ****** ****** ****** ****** ****** ****** ****** 
////// ////// ////// ////// ////// ////// ////// ////// ////// ////// 
)");
```

这样就可以实现立体艺术字

<img src="README.assets/image-20260325164801944.png" alt="image-20260325164801944" style="zoom:67%;" />

##  事件系统

在游戏引擎架构中，**事件系统（Event System）** 被称为引擎的“神经系统”。

它的核心作用是**解耦（Decoupling）**：让底层的窗口（如 GLFW 窗口）在发生动作（点击、缩放）时，不需要知道谁在处理这些动作，只需要把“信号”发出去，让上层的游戏逻辑（Sandbox）去监听并执行。

###  核心组成部分

- **EventType (枚举)**：定义具体的事件类型（如 KeyPressed, MouseButtonPressed, WindowClose）。
- **EventCategory (位掩码枚举)**：将事件分类（如 Keyboard, Mouse, Input）。一个事件可以属于多个分类（例如“按下鼠标”既属于 Mouse 也属于 Input）。
- **Event (基类)**：所有事件的祖先，定义了获取类型、分类、是否被处理（Handled）的接口。
- **EventDispatcher (分发器)**：核心逻辑。它像一个过滤器，根据事件的类型，把它交给正确的函数去处理

```
#pragma once
#include "Core.h"  // 我们需要在这里定义位操作宏

namespace gl {

    // 事件类型枚举
    enum class EventType {
        None = 0,
        WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
        AppTick, AppUpdate, AppRender,
        KeyPressed, KeyReleased, KeyTyped,
        MouseButtonPressed, MouseButtonReleased, MouseMoved, MouseScrolled
    };

    // 事件分类（使用位移操作，方便一个事件属于多个分类）
    enum EventCategory {
        None = 0,
        EventCategoryApplication = BIT(0),
        EventCategoryInput = BIT(1),
        EventCategoryKeyboard = BIT(2),
        EventCategoryMouse = BIT(3),
        EventCategoryMouseDevice = BIT(4)
    };

    // 为了方便子类实现，定义的辅助宏
#define EVENT_CLASS_TYPE(type) static EventType GetStaticType() { return EventType::##type; }\
                               virtual EventType GetEventType() const override { return GetStaticType(); }\
                               virtual const char* GetName() const override { return #type; }

#define EVENT_CLASS_CATEGORY(category) virtual int GetCategoryFlags() const override { return category; }

    // 事件基类
    class Event {
    public:
        bool Handled = false; // 如果被处理了，就不再传给下一层

        virtual EventType GetEventType() const = 0;
        virtual const char* GetName() const = 0;
        virtual int GetCategoryFlags() const = 0;
        virtual std::string ToString() const { return GetName(); }

        // 检查该事件是否属于某个分类
        inline bool IsInCategory(EventCategory category) {
            return GetCategoryFlags() & category;
        }
    };

    // 事件分发器：根据类型执行对应的函数
    class EventDispatcher {
        template<typename T>
        using EventFn = std::function<bool(T&)>;
    public:
        EventDispatcher(Event& event) : m_Event(event) {}

        template<typename T>
        bool Dispatch(EventFn<T> func) {
            if (m_Event.GetEventType() == T::GetStaticType()) {
                m_Event.Handled = func(*(T*)&m_Event);
                return true;
            }
            return false;
        }
    private:
        Event& m_Event;
    };

    // 方便 spdlog 直接打印事件对象
    inline std::ostream& operator<<(std::ostream& os, const Event& e) {
        return os << e.ToString();
    }
}
```

 Core.h，用来放引擎最基础的宏定义

```
#pragma once

// BIT(x) 宏： 1 << 0 = 1, 1 << 1 = 2, 1 << 2 = 4...
// 用于 EventCategory 的位掩码判定
#define BIT(x) (1 << x)
```

同理编写各种事件，目前包含KeyEvent、MoudeEvent、ApplicationEvent，这里需要注意头文件的作用情况

```
#pragma once
#include "Glimmer/Events/Event.h"

namespace gl {

    class KeyEvent : public Event {
    public:
        inline int GetKeyCode() const { return m_KeyCode; }
        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
    protected:
        KeyEvent(int keycode) : m_KeyCode(keycode) {}
        int m_KeyCode;
    };

    class KeyPressedEvent : public KeyEvent {
    public:
        KeyPressedEvent(int keycode, int repeatCount)
            : KeyEvent(keycode), m_RepeatCount(repeatCount) {}

        inline int GetRepeatCount() const { return m_RepeatCount; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "KeyPressedEvent: " << m_KeyCode << " (" << m_RepeatCount << " repeats)";
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyPressed)
    private:
        int m_RepeatCount;
    };
}#pragma once
#include "Glimmer/Events/Event.h"

namespace gl {

    class KeyEvent : public Event {
    public:
        inline int GetKeyCode() const { return m_KeyCode; }
        EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
    protected:
        KeyEvent(int keycode) : m_KeyCode(keycode) {}
        int m_KeyCode;
    };

    class KeyPressedEvent : public KeyEvent {
    public:
        KeyPressedEvent(int keycode, int repeatCount)
            : KeyEvent(keycode), m_RepeatCount(repeatCount) {}

        inline int GetRepeatCount() const { return m_RepeatCount; }

        std::string ToString() const override {
            std::stringstream ss;
            ss << "KeyPressedEvent: " << m_KeyCode << " (" << m_RepeatCount << " repeats)";
            return ss.str();
        }

        EVENT_CLASS_TYPE(KeyPressed)
    private:
        int m_RepeatCount;
    };
}
```

最后尝试在Application调试

```
    void Application::Run() {

        WindowResizeEvent e(1920, 1080);
        GL_TRACE("{0}", e);

        while (true) {
            // 这里将是未来游戏的心脏：Game Loop
        }
    }
```

但是出现报错

```
1>E:\Zaproject\Engine\Glimmer\Glimmer\vendor\spdlog\include\spdlog\fmt\bundled\base.h(2310,45): error C2079: “_”使用未定义的 struct“fmt::v12::detail::type_is_unformattable_for<T,char>”
1>        with
1>        [
1>            T=gl::WindowResizeEvent
1>        ]
```

回头检查了很多次，很奇怪，宏就是无法识别e作为传输对象，只有加上ToString才可以通过

![image-20260326113548656](README.assets/image-20260326113548656.png)

查了一些资料，找到了原因：fmt 12+（新版）不认识你的 Event 类，不知道怎么格式化它

需要**在 Event.h 末尾**加一段 fmt 格式化支持代码

```
#include <spdlog/fmt/fmt.h>

template<typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of_v<gl::Event, T>, char>>
    : fmt::formatter<std::string>
{
    auto format(const T& e, format_context& ctx) const
    {
        return formatter<std::string>::format(e.ToString(), ctx);
    }
};
```

之后就不会进行报错

![image-20260326114511827](README.assets/image-20260326114511827.png)

## 预编译头文件 (PCH)

在上一节的事件系统可以看到，如果你使用了某个标准库的功能（比如 std::function）却没有包含对应的头文件（<functional>），编译器会变得非常混乱。它不认识 std::function，导致后面的**模板解析全部失败**，进而引发了一连串莫名其妙的错误（甚至连 spdlog 和系统的 <ratio> 库都会跟着报废）。这在之后模块越来越多的情况下是难以进行管理的，因此为了彻底解决“漏掉头文件”的问题并大幅提升编译速度，正式引入“预编译头文件 (PCH)”

在 Glimmer/src 下创建 glpch.h。把所有常用的标准库全放进去。

```
#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#ifdef GL_PLATFORM_WINDOWS
#include <Windows.h>
#endif
```

在 Glimmer/src 下创建 glpch.cpp。
*(注意：这是给 Visual Studio 用的，它需要一个空的源文件来触发编译 PCH)*。

```
#include "glpch.h"
```

修改 premake5.lua 启用 PCH，需要告诉 Premake 哪个是 PCH 文件。

```
project "Glimmer"
    -- ... 之前的配置 ...

    pchheader "glpch.h" -- 告诉编译器 PCH 的名字
    pchsource "Glimmer/src/glpch.cpp" -- 只有 Glimmer 项目需要这个源文件

    files {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
    }
    -- ...
```

**所有属于 Glimmer 项目内部的 .cpp 源文件**（即 Glimmer/src 下的所有文件），都**必须**在第一行写上 #include "glpch.h"。

- **原因**：你在 Premake 里为 Glimmer 项目开启了 PCH。如果编译器在 Glimmer 项目的 .cpp 文件里第一行没看到这个头文件，它就会报错（报错代码通常是 C1010）。

**绝对不要包含 glpch.h 的文件**

1. 引擎内部的 .h 头文件 (重要！)

   **永远不要在 .h 文件里 #include "glpch.h"。**

   **原因**：头文件是被别人引用的。如果你在 Event.h 里写了 glpch.h，而外部的 Sandbox 项目引用了 Event.h，但 Sandbox 没有配置这个 PCH，它就会报错找不到 glpch.h。

   **做法**：在 .h 里，如果你需要 std::string，就直接包含 #include <string>。

2. Sandbox 项目的所有文件

   **绝对不要在 Sandbox 里的任何文件包含 glpch.h。**

   **原因**：glpch.h 是属于 Glimmer 引擎项目的内部私有财产。Sandbox 是另一个独立的工程，它有自己的编译环境。它甚至不需要 PCH，或者它可能有自己的一套 sandpch.h。

启用过后编译速度真起飞了

## 窗口和GLFW

引入 **GLFW** 是 Glimmer 引擎从“控制台黑框框”向“真正的图形化软件”跨越的关键一步。

我们将遵循工业级引擎的开发模式：**不要直接在 Application 里写 GLFW 代码**，而是先建立一个**窗口抽象层（Window Abstraction）**。这样未来你想把底层换成 DirectX 或支持手机端时，只需要增加一个实现类，而不需要改动引擎核心逻辑。

添加子模块

```
git submodule add https://github.com/glfw/glfw.git .\Glimmer\vendor\GLFW
```

在 **Glimmer/vendor/GLFW/** 目录下新建文件 **premake5.lua**：



**修改 premake5.lua**：
我们需要告诉 Glimmer 项目去哪里找 GLFW。

```
-- Glimmer 项目配置中增加：
project "Glimmer"
    -- ...
    includedirs {
        "%{prj.name}/src",
        "%{prj.name}/vendor/spdlog/include",
        "%{prj.name}/vendor/GLFW/include" -- 新增
    }

    filter "system:windows"
        systemversion "latest"
        defines { "GL_PLATFORM_WINDOWS" }
        
        -- 新增链接项
        links { 
            "GLFW", 
            "opengl32.lib" -- Windows 自带的 OpenGL 驱动库
        }

        libdirs {
            "Glimmer/vendor/GLFW/lib" -- 告诉 Premake 去哪里找 glfw3.lib
        }
```

定义窗口接口 (Window.h)

在 Glimmer/src/Glimmer 下创建 Window.h。这是一个纯虚基类，定义了所有平台窗口都必须有的功能。

**Glimmer/src/Glimmer/Window.h**:



Windows 平台的具体实现 (WindowsWindow.h/cpp)

我们在 Glimmer/src/Platform/Windows 目录下专门存放 Windows 特有的代码。

**Glimmer/src/Platform/Windows/WindowsWindow.h**:



实现窗口初始化 (WindowsWindow.cpp)

在这里，我们将调用 GLFW 的 API 来真正弹出一个窗口，并设置 **VSync（垂直同步）**。

**Glimmer/src/Platform/Windows/WindowsWindow.cpp**:



在 Application 中接入窗口

**Glimmer/src/Glimmer/Application.h**:

```
    private:
        std::unique_ptr<Window> m_Window; // 引擎持有的窗口指针
        bool m_Running = true;
```

**Glimmer/src/Glimmer/Application.cpp**:

```
// Application.cpp
#include "glpch.h"
#include "Application.h"

namespace gl {
    Application::Application() {
        m_Window = std::unique_ptr<Window>(Window::Create());
    }
    Application::~Application() {}

    void Application::Run() {

        while (m_Running) {
            m_Window->OnUpdate();
        }
    }
}
```

现在运行可以生成一个全黑的窗口了！

![image-20260326132003846](README.assets/image-20260326132003846.png)

## 窗口事件

目前窗口虽然能弹出来，但它就像一个植物人，对外界的点击、缩放、关闭毫无反应。我们要利用 GLFW 的**回调（Callbacks）**机制，将系统的原生消息转化为我们之前写好的 Event 类，并传回给 Application。

在 WindowsWindow.cpp 中绑定 GLFW 回调

这是最关键的一步。GLFW 是 C 语言写的，它使用全局函数作为回调，而我们的引擎是 C++ 面向对象的。我们利用 glfwSetWindowUserPointer 将我们的 WindowData 结构体塞进 GLFW 窗口里。

修改 **WindowsWindow.cpp** 的 Init 函数：

引入三类事件文件

```
// 设置窗口缩放回调
glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
    // 从“口袋”里掏出我们的 Data
    WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
    data.Width = width;
    data.Height = height;

    WindowResizeEvent event(width, height);
    data.EventCallback(event); // 触发回调！
    });
```

（我们在之后通过 glfwSetWindowUserPointer 和 glfwGetWindowUserPointer 获取了 m_Data,并将其复制到一个名为data的引用上：WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window); ） 

调用出来的 OnEvent( ) 就相当于 data.EventCallback( )，然而 OnEvent 在定义上是需要参数的，所以 data.EventCallback(event) == OnEvent(event) ,这个 event ，就是我们用占位符延缓的参数（ 这个参数被标明会在后续使用） 

在使用 Event 对象作为 OnEvent 的参数填入之后，event这个参数参与到 OnEvent 函数体内的操作中去，完成我们定义的操作。

![image-20260326151138428](README.assets/image-20260326151138428.png)

其余同理

在 Application 中接收事件

现在 WindowsWindow 会疯狂地向外发送事件，我们需要在 Application 里接住它们并决定如何处理。

**Application.h**:

```
class Application {
    public:
        // ...
        void OnEvent(Event& e); // 处理事件的中心枢纽
    private:
        bool OnWindowClose(WindowCloseEvent& e); // 专门处理关闭的逻辑
        // ...
    };
```

**Application.cpp**:

```
// 在构造函数里绑定回调
Application::Application() {
    m_Window = std::unique_ptr<Window>(Window::Create());
    
    // 使用 Lambda 表达式把事件传给 OnEvent
    m_Window->SetEventCallback([this](Event& e) {
        this->OnEvent(e);
    });
}

void Application::OnEvent(Event& e) {
    // 1. 打印所有事件（调试用）
    GL_CORE_TRACE("{0}", e.ToString());

    // 2. 使用分发器处理特定事件
    EventDispatcher dispatcher(e);
    // 如果是关闭窗口事件，就执行 OnWindowClose 函数
    dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& event) {
        return this->OnWindowClose(event);
    });
}

bool Application::OnWindowClose(WindowCloseEvent& e) {
    m_Running = false; // 停止 while 循环，优雅退出
    return true;
}
```

```
m_Window->SetEventCallback([this](Event& e) {
    this->OnEvent(e);
});
```

- **原理**：编译器会为你自动生成一个匿名的“闭包”类。[this] 表示通过值捕获当前对象的指针，使得 Lambda 内部可以访问成员函数。

  **性能更好**：编译器更容易对 Lambda 进行内联（Inline）优化。它不涉及复杂的包装转换，几乎没有额外开销。

  **更直观**：代码一眼就能看出逻辑——“当事件 e 发生时，执行 this->OnEvent(e)”。

  **调试友好**：在断点调试时，堆栈信息比 std::bind 简单得多。

![image-20260326183438714](README.assets/image-20260326183438714.png)

## 图层(Layer)

在游戏引擎开发中，**图层（Layer）** 是组织游戏逻辑的核心架构。

想象一下：你的游戏有背景地图、玩家角色、敌人、以及顶层的 UI 菜单和调试信息。如果不分层，所有的代码都会堆在 Application::Run 的死循环里，变得无法维护。

**图层系统的目的：** 把引擎的一帧拆解成多个有序的步骤，并让事件（如鼠标点击）能从上往下传递（UI 层先接住，如果 UI 没拦截，再传给游戏层）。

定义图层基类 (Layer.h)

在 Glimmer/src/Glimmer 下创建 Layer.h。

**Layer.h**:

```
#pragma once

#include "Glimmer/Core.h"
#include "Glimmer/Events/Event.h"

namespace gl {

    class Layer {
    public:
        Layer(const std::string& name = "Layer");
        virtual ~Layer();

        virtual void OnAttach() {}    // 当图层被推入引擎时调用（类似 Start）
        virtual void OnDetach() {}    // 当图层被移除时调用
        virtual void OnUpdate() {}    // 每帧调用（类似 Update）
        virtual void OnEvent(Event& event) {} // 当事件发生时调用

        inline const std::string& GetName() const { return m_DebugName; }
    protected:
        std::string m_DebugName; // 用于调试的名字
    };

}
```

**Layer.cpp**:

```
#include "glpch.h"
#include "Layer.h"

namespace gl {
    Layer::Layer(const std::string& name) : m_DebugName(name) {}
    Layer::~Layer() {}
}
```

实现图层栈 (LayerStack.h/cpp)

图层栈负责管理图层的顺序。在 Glimmer 中，我们把图层分为两类：

1. **普通图层 (Layers)**：放在栈的前半部分（如关卡、背景）。
2. **覆盖层 (Overlays)**：永远放在栈的最后面（如 UI、控制台），保证它们始终在最上层。

**LayerStack.h**:

```
#pragma once

#include "glpch.h"
#include "Layer.h"

namespace gl {

    class LayerStack {
    public:
        LayerStack();
        ~LayerStack();

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);
        void PopLayer(Layer* layer);
        void PopOverlay(Layer* overlay);

        // 为了方便循环遍历
        std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
        std::vector<Layer*>::iterator end() { return m_Layers.end(); }
    private:
        std::vector<Layer*> m_Layers;
        unsigned int m_LayerInsertIndex = 0; // 用于追踪普通图层应该插在哪里
    };

}
```

**LayerStack.cpp**:

```
#include "glpch.h"
#include "LayerStack.h"

namespace gl {

    LayerStack::LayerStack() {}

    LayerStack::~LayerStack() {
        for (Layer* layer : m_Layers)
            delete layer;
    }

    void LayerStack::PushLayer(Layer* layer) {
        // 普通图层插入到 Index 位置，Index 后移
        m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
        m_LayerInsertIndex++;
    }

    void LayerStack::PushOverlay(Layer* overlay) {
        // 覆盖层直接插在末尾
        m_Layers.emplace_back(overlay);
    }

    void LayerStack::PopLayer(Layer* layer) {
        auto it = std::find(m_Layers.begin(), m_Layers.end(), layer);
        if (it != m_Layers.end()) {
            m_Layers.erase(it);
            m_LayerInsertIndex--;
        }
    }

    void LayerStack::PopOverlay(Layer* overlay) {
        auto it = std::find(m_Layers.begin(), m_Layers.end(), overlay);
        if (it != m_Layers.end())
            m_Layers.erase(it);
    }
}
```

在 Application 中集成

现在我们要让 Application 拥有这个栈，并在每帧去更新它，在每个事件去分发它。

**Application.h**:

```
// ... 增加引用 ...
#include "Glimmer/LayerStack.h"

    class Application {
    public:
        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);
        // ...
    private:
        LayerStack m_LayerStack;
        // ...
    };
```

随后在**Application.cpp**:实现函数

注意顺序

```
// 核心逻辑：事件倒序分发
// 为什么要倒序？因为最上层（UI）在 vector 的末尾，它们应该先处理事件
for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); ) {
    (*--it)->OnEvent(e);
    if (e.Handled) // 如果事件被某一层拦截了，直接停止传递
        break;
}
```

```
void Application::Run() {

    while (m_Running) {
        // 核心逻辑：正序更新
        // 先渲染背景，再渲染玩家，最后渲染 UI
        for (Layer* layer : m_LayerStack)
            layer->OnUpdate();

        m_Window->OnUpdate();
    }
}
```

在 Sandbox 中测试

在 SandboxApp.cpp 里创建一个测试图层：

```
class ExampleLayer : public gl::Layer {
public:
    ExampleLayer() : Layer("Example") {}

    void OnUpdate() override {
         GL_INFO("ExampleLayer::Update");
    }

    void OnEvent(gl::Event& event) override {
        GL_TRACE("{0}", event.ToString());
    }
};

class Sandbox : public gl::Application {
public:
    Sandbox() {
        std::cout << "Glimmer Engine Initialized! Hello World!" << std::endl;
        //在这里激活
        PushLayer(new ExampleLayer());
    }
    ~Sandbox() {}
};
```

运行可以看到，Example层一直在更新，但同时也会监听其他操作

<img src="README.assets/image-20260326200942243.png" alt="image-20260326200942243" style="zoom:67%;" />

## Glad

**GLAD** 是 Glimmer 引擎与显卡驱动之间的“翻译官”。OpenGL 的函数实现是在显卡驱动里的，C++ 默认找不到它们。GLAD 的作用就是**加载所有 OpenGL 函数的指针**，让你能写出 glClear、glDrawArrays 这些指令。

我们要遵循**高度解耦**的架构：创建一个 GraphicsContext（图形上下文）接口。这样以后如果你想把 OpenGL 换成 Vulkan，只需要换一个 Context 实现，而不必拆掉整个窗口系统。

**获取 GLAD 源代码**

1. 访问 [GLAD 在线生成器](https://glad.dav1d.de/)。
2. **Language**: C/C++
3. **API**: gl Version **4.6** (或者 4.5，确保选 **Core** 模式)。
4. 点击 **Generate**，下载生成的 ZIP 包。
5. **物理存放**：将 include/glad 和 include/KHR 文件夹放入 Glimmer/vendor/Glad/include。将 src/glad.c 放入 Glimmer/vendor/Glad/src。

**为 Glad 编写 Premake 脚本**

因为 Glad 包含一个 .c 文件，我们需要把它编译进引擎。

在 **Glimmer/vendor/Glad/** 目录下新建 **premake5.lua**：

```
project "Glad"
    kind "StaticLib"
    language "C"
    staticruntime "on"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "include/glad/glad.h",
        "include/KHR/khrplatform.h",
        "src/glad.c"
    }

    includedirs {
        "include"
    }

    filter "system:windows"
        systemversion "latest"
```

修改根目录的 **premake5.lua**：

1. 在 include "Glimmer/vendor/GLFW" 下面增加 include "Glimmer/vendor/Glad"。
2. 在 Glimmer 项目的 includedirs 中增加 "%{prj.name}/vendor/Glad/include"。
3. 在 Glimmer 项目的 links 中增加 "Glad"。

**运行 GenerateProject.bat。**

修改WindowsWindow.cpp进行测试

在WindowsWindow更新了

```
m_Window = glfwCreateWindow((int)props.Width, (int)props.Height, m_Data.Title.c_str(), nullptr, nullptr);
glfwMakeContextCurrent(m_Window);
int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

glfwSetWindowUserPointer(m_Window, &m_Data);
SetVSync(true);
```

报错1>E:\Zaproject\Engine\Glimmer\Glimmer\vendor\Glad\include\glad\glad.h(27,1): fatal  error C1189: #error:  OpenGL header already included, remove this include, glad already provides it

**报错原因**：
GLAD 和 GLFW 都在争夺对 OpenGL 头文件的控制权。

1. GLAD 必须在**最前面**被包含，因为它定义了所有的 OpenGL 函数。
2. GLFW 默认也会自动包含一个标准的 GL.h。
3. 如果你先包含了 GLFW/glfw3.h 再包含 glad/glad.h，或者在同一个文件里它们顺序反了，就会触发 GLAD 源码里的那个 #error 保护机制。

可以

```
// WindowsWindow.h 或 WindowsWindow.cpp 的顶部
#include <glad/glad.h>   // 1. GLAD 永远在第一位
#define GLFW_INCLUDE_NONE // 2. 告诉 GLFW 别带上默认的 OpenGL 头文件
#include <GLFW/glfw3.h>  // 3. 然后再包含 GLFW
```

不加宏定义也行，只是要保证顺序

断点调试结果保证正常运行

![image-20260327125047422](README.assets/image-20260327125047422.png)

## ImGui

集成 **Dear ImGui** 是 Glimmer 引擎开发中里程碑式的一步。它不仅能让你画出各种调试滑块、性能图表，更是未来**可视化编辑器**的基石。

按照 Glimmer 的架构，我们将 ImGui 作为一个特殊的 **Overlay（覆盖层）** 插入图层栈。这样它就能永远显示在游戏画面最顶端，并且优先拦截鼠标/键盘事件。

```
git submodule add -b docking https://github.com/ocornut/imgui.git Glimmer/vendor/imgui
```

> *-b docking* *分支。这个分支支持“窗口停靠”和“多视图”功能，是做引擎编辑器的标准选择。*

为 ImGui 编写 Premake 脚本

ImGui 本身只是源码，我们需要告诉 Premake 如何编译它的核心代码以及对应的 **GLFW** 和 **OpenGL3** 后端。

在 **Glimmer/vendor/imgui/** 目录下新建 **premake5.lua**：

并**修改根目录的 premake5.lua：**

1. 在 include "Glimmer/vendor/Glad" 下增加 include "Glimmer/vendor/imgui"。
2. 在 Glimmer 项目的 includedirs 中增加 "%{prj.name}/vendor/imgui" 和 "%{prj.name}/vendor/imgui/backends"。
3. 在 Glimmer 项目的 links 中增加 "ImGui"。

**运行 GenerateProject.bat。**



**在 Application 中渲染 ImGui**

现代引擎通常不在 Layer::OnUpdate 里直接画 UI，而是在主循环里专门留出一个位置。

**Application.cpp 改动**：



为了让 ImGuiLayer 能顺利初始化，我们需要让它能访问到全局的 Application 实例，以及底层的 GLFWwindow 指针。

1. 修改 Window.h (暴露原生窗口指针)

   ```
   // Glimmer/src/Glimmer/Window.h
   namespace gl {
       // ...
       class Window {
       public:
           // ... 其他虚函数
           virtual void* GetNativeWindow() const = 0; // 【新增】：返回底层窗口句柄
       };
   }
   ```

2. 修改 WindowsWindow.h (实现接口)

   ```
   // Glimmer/src/Platform/Windows/WindowsWindow.h
   namespace gl {
       class WindowsWindow : public Window {
       public:
           // ...
           inline virtual void* GetNativeWindow() const override { return m_Window; } // 【新增】
       };
   }
   ```

3. 修改 Application.h (实现单例模式)

4. 修改 Application.cpp (初始化单例)

   ```
   namespace gl {
       Application* Application::s_Instance = nullptr; // 【新增】：定义静态变量
   
       Application::Application() {
           GL_CORE_ASSERT(!s_Instance, "Application already exists!"); // 防止实例化多次
           s_Instance = this; // 【新增】：把自己存入单例
   ```

**编写 ImGuiLayer (完整的 UI 渲染层)**

在 Glimmer/src/Glimmer/ImGui 目录下创建这两个文件。

1. ImGuiLayer.h，为了给cpp调用，需要重载Layer里的函数

   ```
   #pragma once
   #include "Glimmer/Layer.h"
   
   namespace gl {
       class ImGuiLayer : public Layer {
       public:
           ImGuiLayer();
           ~ImGuiLayer();
   
           virtual void OnAttach() override;
           virtual void OnDetach() override;
           virtual void OnUpdate() override;
           virtual void OnEvent(Event& event) override;
   
           virtual void OnImGuiRender() override;
   
           void Begin(); // 每帧开始前呼叫
           void End();   // 每帧结束后呼叫
       private:
           float m_Time = 0.0f;
       };
   }
   ```

2. cpp代码过长，简要说明：代码实现了一个 ImGui 的引擎层封装（ImGuiLayer），把 Dear ImGui 完整接入到你的引擎生命周期中。核心逻辑是：在 `OnAttach` 中创建 ImGui 上下文并开启键盘控制、Docking 和多窗口 Viewport 等高级功能，同时通过 GLFW 获取原生窗口并初始化 ImGui 的 GLFW + OpenGL 后端；在每一帧通过 `Begin` 和 `End` 控制 ImGui 的更新与渲染流程，其中 `End` 负责将 UI 绘制数据提交给 OpenGL，并在开启多窗口时处理额外的平台窗口渲染和上下文切换；在 `OnDetach` 中则完整释放 ImGui 相关资源。整体上，这段代码把 UI 系统封装成引擎的一个独立 Layer，使 UI 渲染流程与游戏逻辑解耦，并为后续实现类似 Unity 的编辑器（可拖拽面板、多窗口界面）提供基础。

最后打开 SandboxApp.cpp，在 ExampleLayer 中重写 OnImGuiRender：





回头来看全过程：

我们将 ImGui 的集成分为 **资源导入、项目构建、层级集成、渲染循环** 四个阶段：

1. **资源获取（Submodule）**：
   使用 Git 子模块拉取 ImGui 的 docking 分支。这个分支不仅支持窗口停靠，还支持 **Viewports**（允许 UI 拖出主窗口），是做现代引擎编辑器的必备选择。
2. **项目构建（Premake）**：
   由于 ImGui 是以源代码形式分发的，我们编写了一个专门的 premake5.lua。关键点在于包含 backends 文件夹下的 imgui_impl_glfw.cpp 和 imgui_impl_opengl3.cpp，并将它们编译成一个静态库（StaticLib），链接到我们的 Glimmer 引擎中。
3. **图层抽象（ImGuiLayer）**：
   我们创建了一个 ImGuiLayer 类，继承自引擎的 Layer 基类。**OnAttach**：执行 ImGui 的初始化（创建上下文、开启 Docking/Viewports 标志位、初始化 GLFW 和 OpenGL 后端）。**OnDetach**：执行清理工作。
4. **渲染管线集成（The Loop）**：
   在 Application::Run 的主循环中，我们将 UI 渲染独立出来。每帧执行：m_ImGuiLayer->Begin()：开启 ImGui 新帧，处理输入轮询。调用所有 Layer 的 OnImGuiRender()：让每个游戏模块能在此处编写自己的调试 UI。m_ImGuiLayer->End()：执行渲染并将数据交给 GPU，同时处理 **Multi-Viewport** 的上下文切换（Context Switching），保证弹出窗口的正确渲染。

相关机制

- **架构层面的解耦设计**
  “在集成过程中，我采用了 **Backend-Agnostic（后端无关）** 的设计思路。ImGui 的核心代码与具体的图形 API 是分离的。虽然我目前使用的是 GLFW 和 OpenGL3 组合，但我将其封装在 ImGuiLayer 中，并通过 Application::Get().GetWindow().GetNativeWindow() 获取原生句柄。这种设计保证了如果未来将 Glimmer 升级到 Vulkan 或 DX12，我只需要更换 ImGuiLayer 内部的实现细节，而不需要触动任何上层 UI 代码。”
- **针对多视口（Viewports）的特殊处理**
  “为了实现类似 Unity 那种可以将窗口拖出主程序的体验，我开启了 ImGuiConfigFlags_ViewportsEnable。这引入了一个难点：**OpenGL 上下文管理**。在 End() 函数中，ImGui 需要在多个 OS 窗口间切换渲染上下文。我实现的逻辑是：在处理完额外视口后，必须通过 glfwMakeContextCurrent 强制将 Context 还原回引擎主窗口，否则主循环后续的 SwapBuffers 会作用在错误窗口导致崩溃。这体现了对图形 API 状态机机制的理解。”
- **事件系统与 UI 的冲突处理（劫持机制）**
  “目前我正在完善 UI 对事件的**拦截机制**。即当鼠标悬浮在 ImGui 窗口上时，通过检查 io.WantCaptureMouse 标志位，由 ImGuiLayer 设置 Event.Handled = true。这样可以确保玩家在点击 UI 按钮时，不会误触发背后游戏世界的攻击动作。这证明了我对引擎内‘从顶层向底层分发事件’这一流向的准确控制。”

## 接入ImGui事件

接入 ImGui 事件是让引擎从“只能看”变成“能交互”的关键。

目前，你的 WindowsWindow 捕获了系统事件并分发给了 Application。但 ImGui 作为一个独立的库，它也需要知道鼠标在哪、哪个键按下了。

最重要的是，我们要实现 **“事件拦截（Event Blocking）”**：当点击 ImGui 的按钮时，鼠标点击事件应该在 UI 层就被消耗掉，不应该传给底层的游戏角色

BIND_EVENT_FN

这个宏的作用是简化 std::bind 的写法，让你在绑定事件函数时不需要每次都写那长长的一串。

**请在 Glimmer/src/Glimmer/Core.h 中添加如下代码：**

```
#pragma once

#include <memory>

// ... 之前的 BIT(x) 和断言宏 ...

// ✨ 添加这个宏定义
#define BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)

// ...
```

我们需要利用 EventDispatcher 将 Glimmer 的事件转化为 ImGui 的输入状态。

修改 **Glimmer/src/Glimmer/ImGui/ImGuiLayer.cpp** 的 OnEvent 函数及其配套的处理函数：

```
void ImGuiLayer::OnEvent(Event& event)
{
    // 如果你希望 UI 拦截所有事件，可以在这里根据 ImGui 的状态设置 event.Handled
    ImGuiIO& io = ImGui::GetIO();
    
    // 核心逻辑：如果 ImGui 想要捕获鼠标/键盘，就标记事件已处理
    // 这样事件就不会传给下层的 GameLayer 了
    event.Handled |= event.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
    event.Handled |= event.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;

    // 以下是手动将 Glimmer 事件喂给 ImGui 的逻辑（如果你使用的是手动对接模式）
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<MouseButtonPressedEvent>(BIND_EVENT_FN(ImGuiLayer::OnMouseButtonPressedEvent));
    dispatcher.Dispatch<MouseButtonReleasedEvent>(BIND_EVENT_FN(ImGuiLayer::OnMouseButtonReleasedEvent));
    dispatcher.Dispatch<MouseMovedEvent>(BIND_EVENT_FN(ImGuiLayer::OnMouseMovedEvent));
    dispatcher.Dispatch<MouseScrolledEvent>(BIND_EVENT_FN(ImGuiLayer::OnMouseScrolledEvent));
    dispatcher.Dispatch<KeyPressedEvent>(BIND_EVENT_FN(ImGuiLayer::OnKeyPressedEvent));
    dispatcher.Dispatch<KeyReleasedEvent>(BIND_EVENT_FN(ImGuiLayer::OnKeyReleasedEvent));
    dispatcher.Dispatch<KeyTypedEvent>(BIND_EVENT_FN(ImGuiLayer::OnKeyTypedEvent));
    dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(ImGuiLayer::OnWindowResizeEvent));
}

// --- 以下是具体的转换函数示例 ---

bool ImGuiLayer::OnMouseButtonPressedEvent(MouseButtonPressedEvent& e)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDown[e.GetMouseButton()] = true;
    return false; // 返回 false 表示不强制拦截，交给上面的 WantCapture 逻辑判断
}

bool ImGuiLayer::OnMouseMovedEvent(MouseMovedEvent& e)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(e.GetX(), e.GetY());
    return false;
}

// ... 同理实现 KeyPressed (io.KeysDown[e.GetKeyCode()] = true) 
// ... 和 MouseScrolled (io.MouseWheel += e.GetYOffset())
```

**配置 KeyMap（在 OnAttach 中）**

为了让 ImGui 认识你的按键编号（GLFW 的编号），需要在初始化时建立映射关系。

**ImGuiLayer.cpp (OnAttach)**:

```
void ImGuiLayer::OnAttach() {
    // ... 之前的初始化代码 ...
    
    ImGuiIO& io = ImGui::GetIO();
    // 建立 KeyMap：把 ImGui 的索引映射到 GLFW 的键码
    io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
```

过程中发现这段代码io.KeysDown不能用了，找了很久方法

```
	bool ImGuiLayer::OnKeyPressedEvent(KeyPressedEvent& e)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.KeysDown[e.GetKeyCode()] = true;

		io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
		io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
		io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
		io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
		return false;
	}
```

但最后得知如果你在 OnAttach 里调用了 ImGui_ImplGlfw_InitForOpenGL(window, true)，**这里的 true 表示 ImGui 会自己去钩住 GLFW 的回调**。这样你根本不需要在 OnEvent 里手动写 OnKeyPressedEvent 这些函数。ImGui 自己会偷偷处理好一切。



WindowsWindow.cpp加入

- **KeyPressedEvent**：捕捉的是**物理按键**。比如你按下 Shift + A，它收到的是 Shift 键按下和 A 键按下。它不知道你想输入的是大写的 A。
- **KeyTypedEvent**：捕捉的是**文本输入**。它由操作系统处理，直接告诉你用户输入了字符 A。它会自动处理输入法、大小写、特殊符号。

如果你以后在引擎里想做一个**文本输入框**（比如给物体改名字、写控制台指令），没有 KeyTypedEvent 的话，你的退格键、大小写、甚至是中文输入都会出大问题。

```
glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
{
    WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

    KeyTypedEvent event(keycode);
    data.EventCallback(event);
});
```

## 输入轮询

在 Glimmer 引擎中实现**输入轮询（Input Polling）**，标志着你终于可以像在 Unity 里写 Input.GetKey(KeyCode.W) 那样，随时随地获取按键状态了。

目前你的事件系统是“被动”的（只有按键时才发通知），而输入轮询是“主动”的（随时询问状态）。这在处理角色移动等需要连续输入的场景中是必不可少的。

**创建输入抽象接口 (Input.h)**

在 Glimmer/src/Glimmer 下创建 Input.h。我们使用**单例模式（Singleton）**，但通过静态方法暴露接口，保证调用最整洁。

**Input.h**:

```
#pragma once

#include "Glimmer/Core.h"

namespace gl {

    class Input {
    public:
        // 静态接口：外部直接调用 Input::IsKeyPressed(key)
        inline static bool IsKeyPressed(int keycode) { return s_Instance->IsKeyPressedImpl(keycode); }

        inline static bool IsMouseButtonPressed(int button) { return s_Instance->IsMouseButtonPressedImpl(button); }
        inline static std::pair<float, float> GetMousePosition() { return s_Instance->GetMousePositionImpl(); }
        inline static float GetMouseX() { return s_Instance->GetMouseXImpl(); }
        inline static float GetMouseY() { return s_Instance->GetMouseYImpl(); }

    protected:
        // 由不同平台实现的受保护虚函数 (Impl = Implementation)
        virtual bool IsKeyPressedImpl(int keycode) = 0;
        virtual bool IsMouseButtonPressedImpl(int button) = 0;
        virtual std::pair<float, float> GetMousePositionImpl() = 0;
        virtual float GetMouseXImpl() = 0;
        virtual float GetMouseYImpl() = 0;

    private:
        // 静态单例指针
        static Input* s_Instance;
    };
}
```

**Windows 平台的具体实现 (WindowsInput.cpp)**

在 Glimmer/src/Platform/Windows 目录下创建 WindowsInput.h/cpp。它将直接调用 **GLFW** 的 API 来查询状态。

对外提供一个统一的 `Input` 静态接口（比如 `Input::IsKeyPressed`），但内部并不直接实现，而是通过一个静态单例指针 `s_Instance` 转发到具体平台（这里是 `WindowsInput`）去执行。也就是说，上层代码只依赖 `Input` 这个抽象接口，而真正调用的是底层用 GLFW 实现的 `WindowsInput`，例如在 `IsKeyPressedImpl` 里通过 `glfwGetKey` 轮询当前键盘状态。这样一来，你的引擎逻辑层完全不需要关心是 Windows、Mac 还是 Linux，只需要调用统一接口即可，实现了**平台解耦**。

同时，这套设计本质上是一个“**静态接口 + 单例 + 虚函数分发**”的组合：

- 静态函数保证调用方便（不用到处传对象）
- 单例保证全局唯一输入系统
- 虚函数保证不同平台可以替换实现

最终效果就是：

> 上层写 `Input::IsKeyPressed(W)`，底层自动走到 GLFW（Windows）实现，实现了干净的分层。

**在 Application 中测试**

现在，可以在任何 Layer 的 OnUpdate 中，极其方便地检测输入了。

```
    void Application::Run() {

        while (m_Running) {
            // 1. 游戏逻辑更新 (清除屏幕、移动角色等)
            for (Layer* layer : m_LayerStack)
                layer->OnUpdate();

            // 2. UI 渲染 (极其重要：必须在 Begin 和 End 之间)
            m_ImGuiLayer->Begin();
            for (Layer* layer : m_LayerStack)
                layer->OnImGuiRender(); // 调用每个图层的 UI 绘制函数
            m_ImGuiLayer->End();

            // 检测按键
            if (gl::Input::IsKeyPressed(GLFW_KEY_TAB))
                GL_TRACE("TAB 键正被按住！");

            // 检测鼠标
            if (gl::Input::IsMouseButtonPressed(0)) {
                auto [x, y] = gl::Input::GetMousePosition();
                GL_TRACE("鼠标左键点击坐标: {0}, {1}", x, y);
            }

            // 3. 交换缓冲区
            m_Window->OnUpdate();
        }
    }
```

<img src="README.assets/image-20260329132404500.png" alt="image-20260329132404500" style="zoom:67%;" />

## 按键与鼠标码解耦

现在的 Sandbox 如果想判断按键，必须 #include <GLFW/glfw3.h> 并使用 GLFW_KEY_A。这违反了**依赖倒置原则**——上层游戏逻辑不应该依赖底层的实现库。

在 Glimmer/src/Glimmer 下创建 KeyCodes.h 和 MouseCodes.h。

**KeyCodes.h (部分示例)：**

```
#pragma once

// 这里的数值完全参照 GLFW，但名字变成了我们引擎自己的前缀
#define GL_KEY_SPACE           32
#define GL_KEY_APOSTROPHE      39  /* ' */
#define GL_KEY_A               65
#define GL_KEY_W               87
#define GL_KEY_S               83
#define GL_KEY_D               68
// ... (你可以从 GLFW 源码拷贝剩下的并替换前缀)
```

**MouseButtonCodes**

```
#pragma once

// From glfw3.h
#define GL_MOUSE_BUTTON_1         0
#define GL_MOUSE_BUTTON_2         1
#define GL_MOUSE_BUTTON_3         2
#define GL_MOUSE_BUTTON_4         3
#define GL_MOUSE_BUTTON_5         4
#define GL_MOUSE_BUTTON_6         5
#define GL_MOUSE_BUTTON_7         6
#define GL_MOUSE_BUTTON_8         7
#define GL_MOUSE_BUTTON_LAST      GL_MOUSE_BUTTON_8
#define GL_MOUSE_BUTTON_LEFT      GL_MOUSE_BUTTON_1
#define GL_MOUSE_BUTTON_RIGHT     GL_MOUSE_BUTTON_2
#define GL_MOUSE_BUTTON_MIDDLE    GL_MOUSE_BUTTON_3
```

**更新引擎“全家桶”头文件 (Glimmer.h)**

**作用：** 在 Unity 里，你只需要 using UnityEngine;。我们也要给 Glimmer 做一个“出口”，让用户只需要 #include <Glimmer.h> 就能用到引擎的所有核心功能。

在 Glimmer/src 下创建 **Glimmer.h**：

```
#pragma once

// 供客户端（Sandbox）使用的总头文件
#include "Glimmer/Application.h"
#include "Glimmer/Layer.h"
#include "Glimmer/Log.h"

#include "Glimmer/Input.h"
#include "Glimmer/KeyCodes.h"
#include "Glimmer/MouseButtonCodes.h"

#include "imgui.h"

// --- 入口点 (EntryPoint) ---
#include "Glimmer/EntryPoint.h"
// ----------------------------
```

SandBox

```
    void OnUpdate() override {
        // 使用我们自己的键码判断移动
        if (gl::Input::IsKeyPressed(GL_KEY_W))
            GL_TRACE("向北前进!");

    }
```

<img src="README.assets/image-20260329141655268.png" alt="image-20260329141655268" style="zoom:67%;" />

## GLM

在底层 C++ 开发中，这些数学类型都需要我们自己找库。**GLM** 是工业界的绝对标准，它的设计几乎完全模仿 **GLSL**（OpenGL 着色语言），让你在 C++ 里写数学逻辑和在 Shader 里写起来一模一样。

```
git submodule add https://github.com/g-truc/glm.git Glimmer/vendor/glm
```

**修改 Premake 配置**

由于 GLM 只是头文件，我们不需要为它写 premake5.lua 脚本，只需要把它的路径加入到 **Engine** 和 **Sandbox** 的包含目录中。

修改根目录的 **premake5.lua**：includedirs 两个项目

在Application中进行测试

```
#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale, glm::perspective
namespace gl {
    Application* Application::s_Instance = nullptr;

    Application::Application() {
        GL_CORE_ASSERT(!s_Instance, "Application already exists!"); // 防止实例化多次
        s_Instance = this; // 【新增】：把自己存入单例
        m_Window = std::unique_ptr<Window>(Window::Create());
        // 使用 Lambda 表达式把事件传给 OnEvent
        m_Window->SetEventCallback([this](Event& e) {
            this->OnEvent(e);
            });

        m_ImGuiLayer = new ImGuiLayer();
        PushOverlay(m_ImGuiLayer); // 【新增】：把 ImGuiLayer 作为覆盖层推入栈顶

        // --- GLM 数学测试 ---
        glm::vec4 vector(1.0f, 1.0f, 1.0f, 1.0f);

        // 创建一个平移矩阵，让坐标向右移动 2 个单位
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));

        // 矩阵乘法
        auto result = trans * vector;

        GL_CORE_INFO("GLM Math Test: Result X = {0}", result.x); // 应该输出 3.0
    }
```

![image-20260329145655166](README.assets/image-20260329145655166.png)

## KB

### 为什么不用动态库？

如果用动态链接库，将会出现每次生成解决方案都要手动复制一遍dll文件的情况

#### 1. 什么是 StaticLib (静态库 .lib)？

**通俗比喻**：相当于你把 Glimmer 引擎所有的代码（碰撞、渲染、数学库）**打印成了一本厚厚的实体书**。

- **工作原理**：当编译 Sandbox 游戏时，链接器（Linker）会把这本 Glimmer.lib 里的**所有内容**，直接“抄”一份，**死死地缝合（打包）进最终的 Sandbox.exe 文件里**。
- **结果**：你最终只得到一个胖胖的 Sandbox.exe。你只需要把这个 .exe 发给玩家，玩家双击就能直接玩！
- **优点**：**极度省事**：不需要管环境变量，不需要把一堆 .dll 文件和 .exe 放在同一个目录下，玩家绝不会遇到恶心的“找不到 xxxx.dll”报错。**运行极快**：因为所有的代码都在一个 .exe 的内存空间里，函数调用的速度是最快的（编译器甚至能做极致的跨文件内联优化）。
- **为什么目前用它？**：在引擎开发的初期，代码量很少，编译速度极快。用静态库可以让你少踩无数个“DLL 导出宏（__declspec(dllexport)）”的坑。

#### 2. 什么是 SharedLib (动态链接库 .dll)？

**通俗比喻**：相当于你把 Glimmer 引擎做成了一个**在线的云文档**。

- **工作原理**：当编译 Sandbox 游戏时，它只会生成一个极小的 Sandbox.exe 和一个小小的引导文件（通常也叫 .lib，但只是个空壳目录）。真正的核心代码全都在独立生成的 **Glimmer.dll** 里。

- **结果**：你必须把 Sandbox.exe 和 Glimmer.dll 放在同一个文件夹里发给玩家。当玩家双击 .exe 时，程序会在运行时（Run-time）动态地去旁边寻找并加载这个 .dll。

- **优点**：**内存共享**：如果玩家电脑上同时运行了三个用 Glimmer 引擎做的游戏，它们可以共享内存中的同一个 Glimmer.dll，极大地节省了系统内存。**热更新（极度高级）**：你可以只替换掉玩家目录下的 Glimmer.dll 来修复引擎的 Bug，而不需要重新让玩家下载整个几 GB 的 Sandbox.exe 游戏本体！

- **为什么现在不用它？**：
  要写出一个能完美跨平台（Windows 用 .dll，Mac 用 .dylib，Linux 用 .so）的动态库引擎，你需要在所有的类和函数前面加上恶心至极的导出宏：

  ```
  // 如果用 DLL，你的代码得写成这样： 
  #ifdef GLIMMER_BUILD_DLL    
  	#define GLIMMER_API __declspec(dllexport) 
  #else    
  	#define GLIMMER_API __declspec(dllimport) 
  #endif class GLIMMER_API Application { ... };
  ```

  这对于刚起步的引擎来说，纯粹是自找麻烦。

### 什么是“静态链接（Static Link）”下的入口点冲突？

**如果你在引擎的头文件里定义了 int main()，而用户在两个不同的 .cpp 文件里都包含了这个头文件，会发生什么？**

- **标准答案**：会触发**重定义错误（Multiple Definition Error）**。
- **如何规避？**：**约定俗成**：明确告知开发者，EntryPoint.h 只能在一个项目中有且仅有一个 .cpp 文件包含（通常是主程序入口）。**强制唯一性**：通过条件编译或特定的架构设计，确保 main 函数所在的翻译单元是唯一的。

### **为什么我们要自己定义 GL_PLATFORM_WINDOWS，而不是直接用微软自带的 _WIN32？**

- **标准答案（显摆你的架构思维）**：

  **命名空间保护**：\_WIN32 是编译器厂商提供的宏。如果以后我们要支持 Android、iOS，每个平台都有自己乱七八糟的内置宏。使用 GL_ 前缀的宏（如 GL_PLATFORM_WINDOWS、GL_PLATFORM_LINUX），可以统一我们引擎自己的逻辑，**代码更干净，且不依赖于特定编译器。**

  **灵活控制**：有时候我们可能在 Windows 上模拟 Linux 的运行逻辑。如果使用自己的宏，我们可以通过 Premake 脚本随时开启或关闭，而内置宏是没法手动关掉的。

### **为什么在开发跨平台引擎时，我们倾向于强制开启** **/utf-8** **标志？**

- **标准答案**：**一致性**：不同国家的开发者、不同操作系统的默认编码（Windows 的 GBK, Linux 的 UTF-8）各不相同。如果不统一，你的代码里写了一句中文注释，发给国外的合作伙伴，他的电脑打开可能全是乱码，甚至导致编译失败。
- **现代标准**：C++11 之后，像 spdlog、fmt、yaml-cpp 等现代库都遵循 UTF-8 标准。
- **运行环境**：设置 /utf-8 会同时设置**源代码字符集**和**执行字符集**。这意味着你在代码里写的 "你好"，在运行时输出到控制台时，编译器会确保它以 UTF-8 的形式正确呈现，而不是变成 ?? 或 锟斤拷。

### **在 C++ 中，如果你要输出一段包含大量换行、反斜杠（\）或引号的字符串，你该怎么做？**

- **标准答案**：使用 **C++11 引入的原始字符串字面量 (Raw String Literals)**，语法为 R"(字符串内容)"。
- **为什么要用它？**：**无需转义**：在普通字符串里，你需要写 \\ 来代表一个 \，这在画 ASCII 艺术字时简直是噩梦。在 R"(...)" 中，你看到什么，输出就是什么。**支持多行**：它允许直接在代码里换行，非常适合写 Shader 源码、JSON 模板或艺术字 Logo。

### **为什么在事件系统中，EventCategory **要使用 **BIT(x)** **位移操作，而不是简单的 1, 2, 3 数字？**

- **标准答案**：**多重归属（Multiple Categories）**：使用位掩码（Bitmask）可以允许一个事件同时属于多个分类。例如，MouseButtonPressedEvent 的分类标志可以是 EventCategoryMouse | EventCategoryInput（结果是二进制的 1010）。
- **高效查询**：检查一个事件是否属于某个分类只需要一次位运算（&），速度极快，这在每秒产生成千上万个事件的引擎中至关重要。

### **Glimmer 现在的事件系统是“阻塞式（Blocking）”的，这有什么优缺点？**

- **标准答案**：

  **优点**：实现简单，逻辑直观。事件一发生立即处理，不需要额外的内存缓冲区。

  **缺点**：如果处理某个事件（如复杂计算）太耗时，会直接卡住主循环，导致掉帧。

  **未来优化**：后续可以引入“事件队列（Event Queue）”，将不紧急的事件存起来，在下一帧统筹处理。

### **为什么大型 C++ 项目一定要用 PCH？它的原理是什么？**

- **标准答案**：

  **原理**：C++ 的 #include 是简单的文本拷贝。如果你有 1000 个文件都包含了 <Windows.h>（约 10 万行代码），编译器就要处理 1 亿行代码。PCH 的做法是**将这些头文件预先编译成二进制格式**。

  **作用**：后续编译其他源文件时，编译器直接加载二进制缓存，不再重复解析。

  **结果**：可以把数分钟的编译时间缩短到几秒钟，显著提升开发效率。

### **为什么在 Window::Create 中使用静态工厂方法，而不是直接 new WindowsWindow？**

- **标准答案**：
  为了实现**跨平台隐藏**。在 Application.cpp 中，我们只需要包含通用的 Window.h，而不需要知道 WindowsWindow.h 的存在。这样在编译 Linux 版时，Window::Create 会返回 LinuxWindow，而 Application 的逻辑代码一行都不用改。这符合设计模式中的**工厂模式**和**依赖倒置原则**。

### **什么是 VSync（垂直同步）？它的底层原理是什么？**

- **标准答案**：
  VSync 用于将显示器的刷新率（如 60Hz）与 GPU 的渲染帧率同步。
  **原理**：GPU 有两个缓冲区（Front Buffer 和 Back Buffer）。当开启 VSync 时，GPU 会等待显示器的 **垂直空白间隙 (Vertical Blanking Interval)** 信号，才执行 **双缓冲交换 (Buffer Swapping)**。这能有效防止画面撕裂（Screen Tearing），但可能会增加微小的输入延迟。

### **在处理 GLFW 回调时，为什么不能直接在回调里写 Application::OnEvent(...)？**

- **标准答案**：**语法限制**：GLFW 的回调是 C 风格的函数指针，它无法直接调用 C++ 对象的成员函数（因为没有 this 指针）。

  **解耦原则**：底层的窗口模块不应该知道上层的 Application 是谁。

  **解决方案**：使用 glfwSetWindowUserPointer。这是一个极其经典的 C/C++ 混编技巧。它允许我们将一个自定义对象的地址存在 C 库持有的句柄里，在回调触发时再强转回来。这本质上是在为 C 语言的回调函数提供“上下文”。

### **如果我想在引擎里加一个“性能监控面板（FPS计数器）”，我该怎么做？**

- **标准答案**：我会创建一个专门的 **Overlay（覆盖层）**。把它放在 LayerStack 的最顶端（最后面）。在 OnUpdate 中计算 FPS 并使用渲染指令画在屏幕顶端。因为它处于最顶层，所以无论游戏层怎么渲染，性能面板永远不会被遮挡，且它通常不拦截鼠标事件（Handled = false），保证不影响玩家玩游戏。

### **为什么 OpenGL 需要 Glad 或 Glew 这样的加载库？直接调用不行吗？**

- **标准答案**：

  **动态寻址**：OpenGL 函数实现在 GPU 驱动里。驱动版本不同，函数的内存地址也不同。

  **跨平台限制**：Windows 只默认支持 OpenGL 1.1。所有更高版本的函数（如 Shader 相关函数）必须在运行时动态获取地址。

  **Glad 的工作**：它通过调用 OS 提供的接口（如 Windows 上的 wglGetProcAddress）把这些深藏在驱动里的函数地址一个一个抠出来，赋值给 C++ 指针，我们才能正常调用。

### **什么是“双缓冲（Double Buffering）”？为什么要 SwapBuffers？**

- **标准答案**：
  为了防止画面闪烁。

  **后缓冲区（Back Buffer）**：GPU 在后台静静地画图。

  **前缓冲区（Front Buffer）**：显示器当前显示的图。

  **SwapBuffers**：当后缓冲区画好了，瞬间把它和前缓冲区交换。玩家看到的就是完整的画面，而不是 GPU 正在涂色的过程。

### **ImGui 层应该由客户端（Sandbox）手动挂载，还是由引擎自动集成？**

“这取决于引擎的定位。

**手动挂载（Hazel 早期）\**遵循了\**组合优于继承**的原则，具有极高的灵活性。如果应用是一个不需要交互的后台程序或纯性能演示，可以完全剥离 UI 模块，减少内存和渲染开销。

**自动集成（我目前的做法）\**则是将 ImGui 视为\**引擎的基础设施（Infrastructure）**。
首先，它统一了**渲染序列**。由于 ImGui 需要每帧执行 Begin/End 上下文设置，由引擎持有 m_ImGuiLayer 指针可以确保 UI 渲染逻辑始终包裹在所有图层的 OnImGuiRender 之外。
其次，它提升了**开发效率**。开发者在创建新的 Sandbox 游戏或新的 Layer 时，无需关心 UI 环境的初始化，可以直接通过重写 OnImGuiRender 来实现调试工具的快速开发。
这实际上是向**‘编辑器驱动’**的架构演进，因为未来的引擎编辑器本身就是建立在这个自动集成的 ImGui 层之上的。”

### **你是如何处理 UI 事件与游戏场景事件冲突的？**

“在 Glimmer 引擎中，我通过 **ImGuiIO 标志位** 与 **图层事件拦截机制** 的结合来解决这个问题。

首先，事件在引擎中是**倒序分发**的，即位于栈顶的 ImGuiLayer 会最先收到事件。
在 ImGuiLayer::OnEvent 函数中，我会查询 ImGui 内部的两个状态位：io.WantCaptureMouse 和 io.WantCaptureKeyboard。

- 当鼠标悬浮在任何 ImGui 窗口上时，ImGui 会自动将 WantCaptureMouse 设为 true。
- 此时，ImGuiLayer 会在处理完该事件后，将 Event::Handled 属性设为 true。

由于我们在 Application::OnEvent 中实现了拦截逻辑：一旦某个 Layer 处理了事件并标记为 Handled，循环就会立即中断，事件不再传递给下层的 GameLayer。这保证了玩家在点击 UI 按钮时，场景里的角色不会同时发射子弹或移动。”

### 为什么要这么费劲手动映射？

你会发现 ImGui_ImplGlfw_InitForOpenGL(window, true) 的第二个参数如果传 true，ImGui 其实会自动帮你安装 GLFW 回调。

**但是，为什么在写引擎时我们要手动映射（传 false 或者像我们这样自己写 OnEvent）？**

1. **控制权**：作为引擎开发者，我们希望**所有的**系统事件（从 GLFW 来的）都必须先经过我们的 Application::OnEvent 统一调度。如果让 ImGui 直接去钩住 GLFW，我们的事件系统就会被架空。
2. **平台无关性**：如果我们未来支持手机端，手机端没有 GLFW。通过手动映射，我们可以把触摸事件转化为 ImGui 的鼠标点击，而底层的 ImGui 逻辑完全不需要修改。

### **为什么要把 GLFW 的键码重定义一遍？直接在游戏里用 GLFW 的宏不是更快吗？**

- **标准答案**：

  **屏蔽实现细节**：如果明年我想把底层库从 GLFW 换成 SDL，或者要在手机上跑（手机没键盘，只有触摸），如果我用了 GLFW_KEY_A，我得改掉成百上千个游戏逻辑文件。

  **二进制兼容性**：作为引擎开发者，我希望暴露给用户的 API 是**绝对稳定**的。通过重定义，我可以保证 GL_KEY_A 永远代表 A，而不受底层第三方库版本更新（比如宏改名）的影响。

### **GLM 使用的是“行优先 (Row-major)”还是“列优先 (Column-major)”存储？这有什么影响？**

- **标准答案**：

  **GLM 是列优先 (Column-major)**。这与 OpenGL 的标准保持一致。

  **影响**：

  - **内存排列**：一个 4x4 矩阵，在内存里是先存第一列，再存第二列。
  - **乘法顺序**：在代码里我们要写 Matrix * Vector。如果你用的是行优先的库（如 DirectX 数学库），乘法顺序通常是 Vector * Matrix。
  - **传参**：当你使用 glUniformMatrix4fv 把矩阵传给显卡时，不需要进行转置处理，因为内存结构和 OpenGL 驱动预期的完全一致。