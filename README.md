# Glimmer

## 项目概览

Glimmer 是我用 C++17 一点点搭出来的图形与游戏引擎。刚开始时，我只是想弄清楚一个引擎怎样接管程序入口、维持主循环，再把第一个三角形送上屏幕。写到现在，项目里已经有了 Scene/ECS、资产与材质系统、2D/3D 渲染、编辑器，还有程序化地形、水文和简化气候模拟。回头看，这些东西几乎都不是事先规划好的，多半是旧代码真的撑不住下一个功能时，才被迫补上的。

这份 README 记录的是开发过程，并没有按照使用手册的方式维护。后面的章节基本按照实现顺序保留下来：当时碰到了什么问题，为什么选择这种做法，以及后来又怎样推翻或补全它。部分早期代码已经不是今天的最终写法，但它们能说明项目是怎么走到这里的。

### 目前做到哪里

Glimmer 核心会编译成一个静态库。上层应用通过 `Application`、`Layer`、Scene/ECS 和渲染接口来组织逻辑。现在的开发与验证都放在 Windows 上，基线环境是 Visual Studio 2026 和 `Debug | x64`，真正能运行的图形后端也只有 OpenGL。Vulkan 目前只有枚举、接口和依赖预埋，还不能称为支持。

平时主要使用 `GlimmerEditor-CyouBranch` 开发和验证完整功能；`Sandbox` 留给较小的引擎示例与 Renderer2D 测试。工程由 Premake 生成，窗口和输入交给 GLFW，编辑器界面使用 Dear ImGui。GLM、EnTT 和 spdlog 分别处理数学、ECS 与日志，这几项依赖到现在仍是项目的基础。

### 开发时在想什么

功能少的时候，能跑起来就很让人满足。功能多起来以后，最磨人的问题反而变成了：一份状态到底归谁管，数据在哪一层被改掉，编辑模式和运行模式会不会互相污染。这类问题通常不会立刻报错，却很容易在几周后的重构里一起爆出来。

所以我后来养成了一个习惯：新系统先做一个规模很小、但可以完整验证的版本，确认数据流和所有权没有含糊的地方，再继续加功能。抽象也只做到当前问题真正需要的程度。OpenGL 之外的后端可以预留接口，但没有跑通就不会写成 `已经支持`。这样推进不算快，不过出问题时至少知道该从哪里查，也能用已有回归确认这次修改有没有把旧功能带坏。

## Hello World!

这一章记录的是 Glimmer 第一次真正跑起来的样子。那时还没有窗口、事件和渲染器，目标很小：把引擎编译成库，再让另一个项目创建 `Application` 并进入主循环。代码简单得有些寒酸，但它先验证了一件重要的事：引擎和使用引擎的程序可以分开编译。

### 最早的应用骨架

第一版 `Application` 只保留构造、析构和 `Run()`。循环里暂时什么也不做，甚至没有正常关闭的条件。这个版本当然不能长期使用，我当时只是想先看见进程稳定地跑起来。

```cpp
// Glimmer/Core/Application.h
namespace gl {
    class Application {
    public:
        Application();
        virtual ~Application();
        void Run();
    };

    Application* CreateApplication();
}
```

```cpp
// 早期 Application.cpp 的核心逻辑
void gl::Application::Run()
{
    while (true)
    {
        // 窗口更新、事件和渲染都还没有接进来
    }
}
```

`Sandbox` 是第一个使用这套接口的宿主。它继承 `Application`，再实现引擎约定的 `CreateApplication()`。返回类型写成 `Application*`，调用方因此只需要认识引擎接口，不需要知道创建出来的具体子类。

```cpp
#include <Glimmer.h>

class Sandbox : public gl::Application
{
};

gl::Application* gl::CreateApplication()
{
    return new Sandbox();
}

int main()
{
    gl::Application* app = gl::CreateApplication();
    app->Run();
    delete app;
}
```

![第一次运行 Glimmer 应用](README.assets/image-20260324181422163.png)

### 从源码到 Sandbox.exe

按下 F5 后，Visual Studio 会先编译 `Glimmer`。这个项目在 Premake 中被声明为静态库，所以产物是 `Glimmer.lib`，它本身不能直接运行。随后 `SandboxApp.cpp` 被编译，头文件负责告诉编译器有哪些公开接口，链接阶段再从 `Glimmer.lib` 中找到这些接口的实现，最后生成 `Sandbox.exe`。

我最开始对这段流程的理解很模糊，总觉得 `#include` 之后代码就已经连在一起了。真正拆成两个工程后才看清：头文件解决的是编译时的声明，`links { "Glimmer" }` 解决的是链接时的实现。少掉任何一边，报错发生的阶段都不一样。

### 跑通后留下的问题

这时的控制关系已经有了雏形：宿主负责创建自己的 `Application` 子类，进入 `Run()` 后，程序的持续更新交给引擎。多态在这里没有什么神秘的，它只是让引擎可以拿着 `Application*` 工作，同时允许 `Sandbox` 决定自己要装入哪些内容。

不过 `main()` 仍然写在 `SandboxApp.cpp` 里。每建一个新宿主，都要重复创建、运行和销毁应用的代码，而且客户端也知道了太多初始化细节。下一章的入口点改造，就是从这个别扭之处开始的。

## 入口点

上一章虽然跑通了 `Sandbox`，但 `main()` 还留在客户端。新建一个应用时，创建实例、进入循环、释放实例这套代码都要再抄一遍。更麻烦的是，初始化顺序也暴露给了客户端。日志或性能采样一旦加入，每个宿主都有可能写出不同的启动流程。

这一章要解决的就是这个问题：`main()` 由引擎提供，客户端只负责说明自己想创建哪一种 `Application`。

### 把启动流程收回引擎

`EntryPoint.h` 保存 Windows 入口。它先初始化日志，再调用客户端实现的 `CreateApplication()`，等主循环结束后销毁实例。当前源码还在创建、运行和关闭三个阶段外包了一层性能采样宏；`GL_PROFILE` 关闭时，这些宏不会生成实际代码。

```cpp
#pragma once

#ifdef GL_PLATFORM_WINDOWS

extern gl::Application* gl::CreateApplication();

int main(int argc, char** argv)
{
    gl::Log::Init();

    auto app = gl::CreateApplication();
    app->Run();
    delete app;
}

#endif
```

客户端现在只需要包含一次入口头文件，并实现工厂函数。`Sandbox` 可以装入 `Sandbox2D`，编辑器也可以创建自己的 `GlimmerEditor`，启动和关闭过程仍由同一份代码处理。

```cpp
#include <Glimmer.h>
#include "Glimmer/Core/EntryPoint.h"

class Sandbox : public gl::Application
{
public:
    Sandbox()
    {
        PushLayer(new gl::Sandbox2D());
    }
};

gl::Application* gl::CreateApplication()
{
    return new Sandbox();
}
```

这里有个容易忽略的限制：`EntryPoint.h` 含有 `main()` 的定义，只能被一个可执行目标中的一个源文件包含。它不是普通的公共头文件，随手放进多个 `.cpp` 会直接造成重复符号。

### 平台开关放进构建配置

入口目前只支持 Windows，因此引擎和所有宿主都要看到 `GL_PLATFORM_WINDOWS`。我没有在 Visual Studio 的项目属性里逐个添加，而是让 Premake 在生成工程时统一写入：

```lua
filter "system:windows"
    systemversion "latest"
    defines { "GL_PLATFORM_WINDOWS" }
```

使用项目自己的宏还有一个实际好处：源码判断的是 Glimmer 允许启用的实现，不是编译器碰巧运行在哪个系统上。现在 `Core.h` 对非 Windows 平台仍会报错，所以这只是把边界说清楚，并不表示其他平台已经能运行。

做到这里，职责终于比较顺手了。引擎决定程序怎样启动，宿主决定启动哪一个应用。后面再往入口里加入日志或采样，也不需要同时修改 `Sandbox` 和两个编辑器宿主。

## 日志系统

最早调试时，我直接把文字写到 `std::cout`。窗口和渲染代码一多，这种做法很快就失去作用：消息没有级别，也看不出是引擎内部还是客户端打出来的。Glimmer 因此接入了 spdlog，并把日志初始化放到 `EntryPoint.h`，保证 `Application` 创建前就能记录启动错误。

spdlog 作为仓库子模块放在 `Glimmer/vendor/spdlog`，Premake 将它的 `include` 目录同时提供给引擎和宿主。格式写法可参考 [spdlog 的 Custom formatting 文档](https://github.com/gabime/spdlog/wiki/Custom-formatting)。

### Core 和 Client 分开

`Log` 持有两个 logger。`GLIMMER` 用于引擎内部，`APP` 留给宿主代码。两边都输出到带颜色的控制台，默认开放到 `trace` 级别。

```cpp
namespace gl {
    class Log
    {
    public:
        static void Init();

        static std::shared_ptr<spdlog::logger>& GetCoreLogger();
        static std::shared_ptr<spdlog::logger>& GetClientLogger();

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
    };
}
```

调用处使用两组宏。名字看起来只差一个 `CORE`，但日志一多以后，这个区别很有用；看到前缀就能先判断问题属于引擎还是应用。

```cpp
#define GL_CORE_TRACE(...) ::gl::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define GL_CORE_ERROR(...) ::gl::Log::GetCoreLogger()->error(__VA_ARGS__)
#define GL_CORE_WARN(...)  ::gl::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define GL_CORE_INFO(...)  ::gl::Log::GetCoreLogger()->info(__VA_ARGS__)

#define GL_TRACE(...)      ::gl::Log::GetClientLogger()->trace(__VA_ARGS__)
#define GL_ERROR(...)      ::gl::Log::GetClientLogger()->error(__VA_ARGS__)
#define GL_WARN(...)       ::gl::Log::GetClientLogger()->warn(__VA_ARGS__)
#define GL_INFO(...)       ::gl::Log::GetClientLogger()->info(__VA_ARGS__)
```

初始化时统一设置时间、logger 名称和正文格式：

```cpp
void gl::Log::Init()
{
    spdlog::set_pattern("%^[%T] %n: %v%$");

    s_CoreLogger = spdlog::stdout_color_mt("GLIMMER");
    s_CoreLogger->set_level(spdlog::level::trace);

    s_ClientLogger = spdlog::stdout_color_mt("APP");
    s_ClientLogger->set_level(spdlog::level::trace);
}
```

### 一次编码问题

接入后第一次构建并不顺利。MSVC 报出了 `Unicode support requires compiling with /utf-8`，原因是 Windows 中文环境的默认代码页与 spdlog/fmt 期望的 UTF-8 源文件不一致。最后没有去改第三方库，而是在 Premake 的 Windows 配置里统一加入：

```lua
filter "system:windows"
    buildoptions { "/utf-8" }
```

这个改动后来也保护了源码里的中文注释和字符串。编码问题最烦人的地方是，它经常只在另一台机器上出现；把选项写进生成脚本，比依赖某台电脑的 Visual Studio 设置可靠得多。

### 启动时留下一个明确标记

日志跑通后，我用原始字符串写了一段 Glimmer ASCII 标题，并在 `Log::Init()` 末尾通过 `GL_CORE_INFO` 输出。它没有功能价值，但很适合作为启动检查：标题能完整显示，至少说明 logger 创建、颜色输出和多行原始字符串都在正常工作。

![Glimmer 日志与启动标题](README.assets/image-20260325164801944.png)

后来事件对象也需要直接写进日志，这又碰到了 fmt 的类型格式化限制。那个问题放在下一章记录，因为修复最终落在 `Event.h` 中。

## 事件系统

窗口库会报告关闭、缩放、按键和鼠标动作，但上层不应该到处直接依赖 GLFW 回调。事件系统放在两者中间：平台层创建 Glimmer 事件，`Application` 接收后再交给 Layer。当前实现是同步分发，没有事件队列；事件产生后会沿着同一条调用栈立即处理。

### 事件怎样表示

所有事件都继承 `Event`。`EventType` 表示具体类型，`EventCategory` 用位掩码描述所属分组，`Handled` 则记录事件是否已经被消费。比如 `MouseButtonPressedEvent` 同时属于 Mouse 和 Input，只需要把两个分类位做按位或。

```cpp
#define BIT(x) (1 << x)

enum class EventType
{
    // 本章只摘录会用到的类型
    None = 0,
    WindowClose, WindowResize,
    KeyPressed, KeyReleased, KeyTyped,
    MouseButtonPressed, MouseButtonReleased,
    MouseMoved, MouseScrolled
};

enum EventCategory
{
    None = 0,
    EventCategoryApplication = BIT(0),
    EventCategoryInput       = BIT(1),
    EventCategoryKeyboard    = BIT(2),
    EventCategoryMouse       = BIT(3),
    EventCategoryMouseDevice = BIT(4)
};

class Event
{
public:
    bool Handled = false;

    virtual EventType GetEventType() const = 0;
    virtual const char* GetName() const = 0;
    virtual int GetCategoryFlags() const = 0;
    virtual std::string ToString() const { return GetName(); }

    bool IsInCategory(EventCategory category)
    {
        return GetCategoryFlags() & category;
    }
};
```

每个具体事件都要提供类型和分类。手写这些重复函数既无聊又容易漏，所以当前代码用 `EVENT_CLASS_TYPE` 与 `EVENT_CLASS_CATEGORY` 两个宏生成它们。宏在这里承担的是样板代码，不负责隐藏事件数据。

```cpp
class KeyPressedEvent : public KeyEvent
{
public:
    KeyPressedEvent(int keycode, int repeatCount)
        : KeyEvent(keycode), m_RepeatCount(repeatCount) {}

    int GetRepeatCount() const { return m_RepeatCount; }

    EVENT_CLASS_TYPE(KeyPressed)

private:
    int m_RepeatCount;
};
```

目前的事件文件按 Application、Keyboard 和 Mouse 分开。分类不是文件目录的替代品，它主要用于上层一次判断一组输入，例如 ImGui 想拦截全部鼠标事件时，不必枚举每一种鼠标动作。

### 分发与停止传播

`EventDispatcher` 持有一个 `Event&`。`Dispatch<T>()` 先比较运行时类型，匹配后才把事件交给回调；回调返回的布尔值会写入 `Handled`。

```cpp
class EventDispatcher
{
public:
    explicit EventDispatcher(Event& event)
        : m_Event(event) {}

    template<typename T>
    bool Dispatch(std::function<bool(T&)> func)
    {
        if (m_Event.GetEventType() == T::GetStaticType())
        {
            m_Event.Handled = func(*(T*)&m_Event);
            return true;
        }
        return false;
    }

private:
    Event& m_Event;
};
```

`Application::OnEvent()` 会先处理窗口关闭。随后事件从 LayerStack 顶部向下传递，让 UI 和其他 Overlay 比场景层更早收到输入。某一层把 `Handled` 设为 `true` 后，循环立即停止。

```cpp
EventDispatcher dispatcher(e);
dispatcher.Dispatch<WindowCloseEvent>(
    [this](WindowCloseEvent& event)
    {
        return OnWindowClose(event);
    });

for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
{
    (*--it)->OnEvent(e);
    if (e.Handled)
        break;
}
```

这个顺序后来变得很重要。没有它时，在 ImGui 面板上点击按钮也可能把同一次鼠标输入传给场景；有了逆序传播和 `Handled`，上层可以明确截住事件。

### fmt 不认识 Event

事件刚写完时，我想直接输出一个 `WindowResizeEvent`：

```cpp
WindowResizeEvent event(1920, 1080);
GL_TRACE("{}", event);
```

结果 fmt 12 报出 `type_is_unformattable_for`。`Event` 虽然实现了 `operator<<` 和 `ToString()`，fmt 仍然不知道怎样格式化这个自定义类型。临时写成 `event.ToString()` 可以通过，但每个调用点都这样写很别扭。

![fmt 无法格式化 Event 的编译错误](README.assets/image-20260326113548656.png)

最后在 `Event.h` 中为 `Event` 派生类型补了 formatter，统一复用 `ToString()`：

```cpp
template<typename T>
struct fmt::formatter<
    T,
    std::enable_if_t<std::is_base_of_v<gl::Event, T>, char>>
    : fmt::formatter<std::string>
{
    auto format(const T& event, format_context& ctx) const
    {
        return formatter<std::string>::format(event.ToString(), ctx);
    }
};
```

![Event 可以直接写入日志](README.assets/image-20260326114511827.png)

这次报错让我意识到，能被 `std::ostream` 输出，不等于能被 fmt 自动格式化。把适配放在事件类型旁边以后，日志调用保持简洁，新的事件子类也会沿用同一套字符串输出规则。

## 预编译头文件 (PCH)

事件系统加入 `std::function`、字符串流和日志以后，很多 `.cpp` 都在反复解析同一批标准库头文件。构建还能完成，只是每加一个源文件，等待时间都会多一点。PCH 就是在这个阶段接进来的：把稳定且高频使用的头文件预先编译，后续编译单元直接复用结果。

我起初还把 PCH 当成了防止漏写 `#include` 的办法，后来发现这正好反了。某个头文件因为 PCH 碰巧提供了 `std::string`，并不代表它的依赖写对了；一旦被另一个目标单独包含，问题还是会出现。PCH 负责速度，头文件仍要能够说明自己的直接依赖。

### Glimmer 的 PCH 内容

`glpch.h` 放在 `Glimmer/src`，里面主要是常用标准库、日志与性能采样基础。Windows SDK 只在 Windows 配置下进入 PCH。

```cpp
#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Glimmer/Core/Core.h"
#include "Glimmer/Core/Log.h"
#include "Glimmer/Debug/Instrumentor.h"

#ifdef GL_PLATFORM_WINDOWS
#include <Windows.h>
#endif
```

Visual Studio 需要一个源文件创建预编译产物，因此 `glpch.cpp` 只有一行：

```cpp
#include "glpch.h"
```

Premake 把头文件和创建源文件绑定到 `Glimmer` 静态库目标。这里的路径相对于 `Glimmer/premake5.lua`：

```lua
pchheader "glpch.h"
pchsource "src/glpch.cpp"
```

### 使用边界

引擎实现文件通常把 `#include "glpch.h"` 放在最前面，否则 MSVC 可能报 C1010。`Sandbox` 和编辑器是独立目标，不应该直接使用引擎私有 PCH；它们可以按需要建立自己的预编译头。

头文件则应直接包含对外接口真正需要的标准库或类型声明。仓库里仍有少数早期头文件直接包含 `glpch.h`，新增代码不再沿用这个写法。否则编译可能暂时变快了，模块边界却会越来越难看清。

PCH 接入后没有改变任何运行时行为，收益都发生在编译阶段。它不是什么复杂功能，但项目变大以后，不再反复解析 `<Windows.h>` 和常用 STL 头文件，体感非常明显。

## 窗口与 GLFW

只有主循环还不够，图形程序至少需要一个窗口和对应的图形上下文。Glimmer 使用 GLFW 创建 Windows 窗口、轮询系统消息，并交换前后缓冲区。GLFW 作为静态库项目接入 Premake，`Glimmer` 同时链接 Windows 的 `opengl32.lib`。

我没有把 GLFW 调用直接塞进 `Application`。当时最直接的原因不是未来要支持多少平台，而是 `Application` 已经开始负责主循环和生命周期，再让它保存 GLFW 细节，很快就会变成谁都不敢改的文件。

### Window 接口

`Window` 描述上层实际需要的操作：更新窗口、查询尺寸、接收事件、控制 VSync，以及在少数平台集成场景下取得原生句柄。

```cpp
namespace gl {
    struct WindowProps
    {
        std::string Title;
        unsigned int Width;
        unsigned int Height;

        WindowProps(const std::string& title = "Glimmer Engine",
                    unsigned int width = 1280,
                    unsigned int height = 720)
            : Title(title), Width(width), Height(height) {}
    };

    class Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~Window() = default;
        virtual void OnUpdate() = 0;

        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;
        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void SetVSync(bool enabled) = 0;
        virtual bool IsVSync() const = 0;
        virtual void* GetNativeWindow() const = 0;

        static Window* Create(const WindowProps& props = WindowProps());
    };
}
```

这个接口为其他平台留出了位置，但当前工厂只有一个实现：

```cpp
gl::Window* gl::Window::Create(const WindowProps& props)
{
    return new WindowsWindow(props);
}
```

所以这里的抽象只是隔开了依赖，还不能当作跨平台支持。`Application` 拿到返回值后立即交给 `std::unique_ptr<Window>` 管理，上层不需要包含 `WindowsWindow.h` 或 GLFW 头文件。

### WindowsWindow 做了什么

`WindowsWindow` 保存 `GLFWwindow*`、窗口属性和一个 `GraphicsContext`。初始化时，它只调用一次 `glfwInit()`，创建窗口，然后建立 `OpenGLContext`。OpenGL 上下文负责把窗口设为当前上下文、通过 Glad 加载函数地址，并输出显卡信息。VSync 默认开启。

每帧末尾，窗口只做两件事：处理系统消息，再交换缓冲区。

```cpp
void WindowsWindow::OnUpdate()
{
    glfwPollEvents();
    m_Context->SwapBuffers();
}

void WindowsWindow::SetVSync(bool enabled)
{
    glfwSwapInterval(enabled ? 1 : 0);
    m_Data.VSync = enabled;
}
```

`Application::Run()` 把 `m_Window->OnUpdate()` 放在循环末尾。从这一步开始，程序终于不再只是任务管理器里的一个进程，而是能显示一个 1280×720 的黑色窗口。

![Glimmer 创建的第一个 GLFW 窗口](README.assets/image-20260326132003846.png)

这个黑窗口当时已经足够让我高兴一阵子。不过它还不会把关闭、缩放或输入动作交给引擎。窗口能显示和窗口能参与应用逻辑，是两件不同的事。

## 窗口事件

窗口创建完成后，下一步是把 GLFW 回调转换为前面定义的 Glimmer 事件。平台层负责翻译，不直接决定游戏如何响应；`Application` 只接收 `Event&`，也不需要知道事件最初来自 GLFW。

### 给 C 回调补上对象上下文

GLFW 的回调是普通函数指针，不能直接保存 `WindowsWindow` 的 `this`。`WindowsWindow::Init()` 因此把成员 `m_Data` 的地址存进 GLFW 窗口：

```cpp
glfwSetWindowUserPointer(m_Window, &m_Data);
```

`WindowData` 保存窗口尺寸、VSync 状态和上层注册的事件回调。它是 `WindowsWindow` 的成员，在 GLFW 窗口销毁前一直有效。

```cpp
struct WindowData
{
    std::string Title;
    unsigned int Width;
    unsigned int Height;
    bool VSync;
    EventCallbackFn EventCallback;
};
```

回调触发时，再通过 `glfwGetWindowUserPointer()` 取回这个地址。窗口缩放的转换过程很直接：更新缓存尺寸，构造 `WindowResizeEvent`，然后交给上层回调。

```cpp
glfwSetWindowSizeCallback(
    m_Window,
    [](GLFWwindow* window, int width, int height)
    {
        WindowData& data =
            *(WindowData*)glfwGetWindowUserPointer(window);

        data.Width = width;
        data.Height = height;

        WindowResizeEvent event(width, height);
        data.EventCallback(event);
    });
```

关闭、按键、字符输入、鼠标按钮、滚轮和光标移动都沿用同一套做法。GLFW 的 action 会在平台层转换为 `KeyPressedEvent`、`KeyReleasedEvent` 或对应的鼠标事件。这样 GLFW 常量和回调签名不会继续向 `Application` 扩散。

### Application 接住事件

窗口创建后，`Application` 用一个捕获 `this` 的 Lambda 注册自己的 `OnEvent()`：

```cpp
m_Window->SetEventCallback(
    [this](Event& event)
    {
        OnEvent(event);
    });
```

这里的 Lambda 没有复杂技巧。它只是把 C++ 成员函数和 `Window` 保存的通用回调类型接在一起，比单独维护静态转发函数更容易读。

`OnEvent()` 首先尝试分发窗口关闭事件。匹配成功后，`OnWindowClose()` 把 `m_Running` 设为 `false`，主循环会在当前帧结束后退出。

```cpp
void Application::OnEvent(Event& event)
{
    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<WindowCloseEvent>(
        [this](WindowCloseEvent& closeEvent)
        {
            return OnWindowClose(closeEvent);
        });

    // 随后按逆序交给 LayerStack，Handled 后停止传播
}

bool Application::OnWindowClose(WindowCloseEvent& event)
{
    m_Running = false;
    return true;
}
```

![Application 收到 GLFW 转换后的窗口事件](README.assets/image-20260326183438714.png)

这条链路跑通后，窗口、平台实现和应用循环之间的关系才算清楚：GLFW 负责报告系统动作，`WindowsWindow` 把动作翻译成引擎事件，`Application` 决定传播顺序。后面加入 Layer 时，只需要接在 `Application::OnEvent()` 的下游，不用重新碰 GLFW 回调。

## 图层 (Layer)

窗口和事件接进来以后，`Application` 很快塞满了测试逻辑。每加一个功能都去改主循环，短期省事，过几天就很难分清哪些代码属于引擎，哪些只是某个示例。Layer 是当时用来拆开这些逻辑的最小单位。

`Layer` 提供挂载、卸载、逐帧更新、事件处理和 ImGui 绘制入口。现在的更新函数接收 `Timestep`，各层可以用同一份帧间隔推进动画或场景逻辑。

```cpp
class Layer
{
public:
    explicit Layer(const std::string& name = "Layer");
    virtual ~Layer();

    virtual void OnAttach() {}
    virtual void OnDetach() {}
    virtual void OnUpdate(Timestep ts) {}
    virtual void OnEvent(Event& event) {}
    virtual void OnImGuiRender() {}
};
```

`LayerStack` 用 `m_LayerInsertIndex` 隔开普通 Layer 和 Overlay。普通 Layer 插在分界线之前，Overlay 追加到容器末尾。主循环按正序更新，事件则从末尾反向传递，因此后加入的 UI Overlay 可以先处理输入，并在设置 `Handled` 后结束传播。

```cpp
void LayerStack::PushLayer(Layer* layer)
{
    m_Layers.emplace(m_Layers.begin() + m_LayerInsertIndex, layer);
    ++m_LayerInsertIndex;
}

void LayerStack::PushOverlay(Layer* overlay)
{
    m_Layers.emplace_back(overlay);
}
```

`Application::PushLayer()` 和 `PushOverlay()` 会先把对象放入栈，再调用 `OnAttach()`。`LayerStack` 销毁时负责释放仍在容器中的对象，这也是当前裸指针接口隐含的所有权约定。需要注意的是，`PopLayer()` 和 `PopOverlay()` 只负责移出容器，不会调用 `OnDetach()` 或删除对象；如果以后支持运行时卸载，调用方还要补齐这段生命周期。

我最早用 `ExampleLayer` 验证更新和事件是否能到达同一个模块。控制台不停刷日志并不优雅，但这次测试确认了一件重要的事：主循环已经不需要知道示例层具体在做什么。

![ExampleLayer 接收更新与事件](README.assets/image-20260326200942243.png)

## Glad

GLFW 把窗口建起来以后，OpenGL 函数还不能直接使用。这些函数由显卡驱动提供，需要先拿到当前平台上的函数地址。GLAD 在这里负责加载指针，后面的 `glClear`、`glDrawElements` 等调用才真正有落点。

仓库把 GLAD 作为一个独立的 C 静态库编译。根 Premake 脚本负责引入子项目，Glimmer 再添加头文件目录并链接 `Glad`。这样生成的 `glad.c` 只编译一次，也没有混进引擎自己的 C++ 源文件。

最初我把 `glfwMakeContextCurrent()` 和 `gladLoadGLLoader()` 直接写进了 `WindowsWindow`。窗口确实能跑，但 GLFW 的窗口管理和 OpenGL 初始化被粘在了一起。后来增加 `GraphicsContext` 接口，把上下文初始化和交换缓冲区收进 `OpenGLContext`，`WindowsWindow` 只负责创建它并调用接口。

```cpp
void OpenGLContext::Init()
{
    glfwMakeContextCurrent(m_WindowHandle);

    int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    GL_CORE_ASSERT(status, "Failed to initialize Glad!");

    GL_CORE_INFO("OpenGL Info:");
    GL_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
    GL_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
    GL_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));
}

void OpenGLContext::SwapBuffers()
{
    glfwSwapBuffers(m_WindowHandle);
}
```

这里有一个很容易踩到的包含顺序问题。GLFW 默认可能带入系统 OpenGL 头文件，而 GLAD 要自己提供这些声明；如果 `GLFW/glfw3.h` 先被包含，编译会报 `OpenGL header already included`。当前代码统一先包含 GLAD，再包含 GLFW：

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
```

加载成功后，我用启动日志中的 Vendor、Renderer 和 Version 做了第一次验证。比起只看窗口有没有出现，这三项输出更直接：Context 已经成为当前上下文，函数指针也确实可以调用。

![GLAD 初始化后的 OpenGL 信息](README.assets/image-20260327125047422.png)

## ImGui

接入 Dear ImGui 的直接原因很简单：日志能告诉我程序发生了什么，却不适合反复调参数。渲染颜色、相机速度或者调试开关时，如果每次修改都要重新编译，开发节奏会被切得很碎。ImGui 提供的是一套即时 UI，正好适合这类只在工具侧存在的控件。

仓库把 ImGui 核心源码、GLFW 后端和 OpenGL3 后端编译成独立静态库，再链接到 Glimmer。引擎侧的入口是 `ImGuiLayer`。它作为 Overlay 放在 `LayerStack` 末尾，因此既能最后绘制，也能在事件反向传播时最先收到输入。

`OnAttach()` 创建 ImGui Context，开启键盘导航、Docking 和 Viewports，加载正文与图标字体，然后用原生 `GLFWwindow` 初始化两个后端。这个步骤需要 `Application::Get()` 和 `Window::GetNativeWindow()`，也是应用单例最早出现的实际用途之一。

```cpp
Application& app = Application::Get();
GLFWwindow* window = static_cast<GLFWwindow*>(
    app.GetWindow().GetNativeWindow());

ImGui_ImplGlfw_InitForOpenGL(window, true);
ImGui_ImplOpenGL3_Init("#version 410");
```

主循环把 UI 单独夹在 `Begin()` 和 `End()` 之间。每个 Layer 只实现自己的 `OnImGuiRender()`，不需要碰 ImGui 后端的帧管理。`End()` 提交主窗口的绘制数据；启用多视口后，它还会渲染额外的平台窗口，并在结束时恢复先前的 OpenGL Context。这个恢复动作不能省，否则下一次交换缓冲区可能落到错误的窗口上。

```cpp
m_ImGuiLayer->Begin();
for (Layer* layer : m_LayerStack)
    layer->OnImGuiRender();
m_ImGuiLayer->End();
```

第一个滑块跑通后，UI 在引擎里的位置也定了下来。它是一个参与生命周期的 Overlay；上层模块负责描述面板，后端初始化和平台窗口处理留在 `ImGuiLayer` 内部。

## 接入 ImGui 事件

UI 能显示以后，紧接着遇到的是输入冲突。点击一个 ImGui 按钮时，同一次鼠标事件仍可能传到场景层；编辑器里的拖动、滚轮和快捷键都会因此误触发后面的游戏逻辑。

我一开始尝试在 `ImGuiLayer::OnEvent()` 里手动维护 `io.KeysDown`、鼠标位置和按键映射。后来才发现这条路既重复又已经过时。`ImGui_ImplGlfw_InitForOpenGL(window, true)` 的第二个参数会让 GLFW 后端安装输入回调，并串联窗口上已有的回调。ImGui 自己接收鼠标、键盘和文本输入，Glimmer 不需要再把每一种 Event 转换一遍。

当前 `OnEvent()` 只做拦截判断：

```cpp
void ImGuiLayer::OnEvent(Event& event)
{
    if (!m_BlockEvents)
        return;

    ImGuiIO& io = ImGui::GetIO();
    event.Handled |= event.IsInCategory(EventCategoryMouse)
        & io.WantCaptureMouse;
    event.Handled |= event.IsInCategory(EventCategoryKeyboard)
        & io.WantCaptureKeyboard;
}
```

`ImGuiLayer` 位于栈顶，`Application` 又按反序把事件交给各层，所以它有机会先设置 `Handled`。如果 ImGui 正在使用鼠标或键盘，传播就停在这里；没有捕获时，事件继续交给编辑器相机或场景层。`BlockEvents(false)` 则允许调用方临时关闭这层保护。

文本输入仍值得单独说明。物理按键由 `glfwSetKeyCallback` 生成 `KeyPressedEvent` 和 `KeyReleasedEvent`，字符输入则由 `glfwSetCharCallback` 生成 `KeyTypedEvent`。后者已经经过系统的键盘布局和修饰键处理，输入框需要的是这条路径，而不是自行把键码猜成字符。

```cpp
glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode)
{
    WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
    KeyTypedEvent event(keycode);
    data.EventCallback(event);
});
```

这次调整后，职责终于清楚了：GLFW 后端把输入送给 ImGui，Glimmer 的窗口回调生成自己的事件，`ImGuiLayer` 只决定这些事件是否还能继续向下传播。

## 输入轮询

事件适合描述一次变化，例如按键刚刚按下或鼠标滚轮滚了一格。角色移动和相机平移却需要在每一帧判断某个键是否仍然按住。为此我补了一套输入轮询接口，它和事件系统并行存在，各自解决不同的问题。

上层通过 `Input` 的静态方法查询键盘、鼠标按钮和光标位置。真正的实现藏在 `s_Instance` 后面，当前实例是由 `Scope<Input>` 持有的 `WindowsInput`。这种写法让 Layer 不必保存输入对象，也不会直接依赖 GLFW。

```cpp
class Input
{
public:
    static bool IsKeyPressed(int keycode);
    static bool IsMouseButtonPressed(int button);
    static std::pair<float, float> GetMousePosition();
    static float GetMouseX();
    static float GetMouseY();

private:
    static Scope<Input> s_Instance;
};
```

`WindowsInput` 每次查询都会从 `Application` 取得原生窗口，再调用 GLFW。键盘查询同时接受 `GLFW_PRESS` 和 `GLFW_REPEAT`，因此按住按键时每一帧都能得到 `true`。

```cpp
bool WindowsInput::IsKeyPressedImpl(int keycode)
{
    auto* window = static_cast<GLFWwindow*>(
        Application::Get().GetWindow().GetNativeWindow());
    auto state = glfwGetKey(window, keycode);
    return state == GLFW_PRESS || state == GLFW_REPEAT;
}
```

最早的验证是在 Layer 更新时持续查询按键和鼠标，并把光标坐标写进日志。结果不复杂，但它把两种输入方式的边界测清楚了：Event 记录变化，Polling 读取当前状态。后面的相机控制也正是建立在这个区别上。

![输入轮询测试](README.assets/image-20260329132404500.png)

## 按键与鼠标码解耦

输入轮询刚接通时，Sandbox 为了判断 W 键还得包含 `GLFW/glfw3.h`。这让平台库的名字直接跑进了客户端代码，之后替换窗口后端时，连游戏逻辑里的按键判断都要跟着改。

我先增加了 `KeyCodes.h` 和 `MouseButtonCodes.h`，把常用键位统一成 `GL_KEY_*` 与 `GL_MOUSE_BUTTON_*`。`Input` 的调用方式没有变化，客户端只需要使用 Glimmer 自己的名字：

```cpp
if (gl::Input::IsKeyPressed(GL_KEY_W))
    GL_TRACE("向前移动");

if (gl::Input::IsMouseButtonPressed(GL_MOUSE_BUTTON_RIGHT))
    GL_TRACE("旋转相机");
```

这些常量目前仍直接采用 GLFW 的数值，`WindowsInput` 也会把收到的整数原样交给 `glfwGetKey()` 和 `glfwGetMouseButton()`。所以这一步解决的是客户端头文件依赖，还算不上完整的输入后端映射。如果以后接入数值体系不同的平台，需要在平台实现里增加转换，而不能继续假设两边编码一致。

`Glimmer.h` 随后成为客户端常用的聚合头，集中导出 Application、Layer、Input、键码以及渲染接口。程序入口仍由各可执行项目显式包含 `Glimmer/Core/EntryPoint.h`，没有塞进聚合头。这个分界可以避免普通业务文件因为包含 `Glimmer.h` 而意外定义 `main()`。

最初的测试只是把 `GLFW_KEY_W` 换成 `GL_KEY_W`，运行结果没有变化。看起来很小，但从这一步开始，Sandbox 和相机控制代码不再需要知道 GLFW 的键名。

![引擎键码输入测试](README.assets/image-20260329141655268.png)

## GLM

开始写相机和物体变换后，向量与矩阵很快变成绕不开的基础设施。我没有自己实现一套数学库，而是把 GLM 作为头文件依赖放进 `vendor/glm`。它的类型和函数命名接近 GLSL，在 CPU 侧准备 Shader 数据时比较顺手。

GLM 不需要单独编译。Premake 把它的路径加入 Glimmer、Sandbox 和编辑器项目的包含目录，引擎项目还把 GLM 的 `.hpp` 与 `.inl` 文件纳入工程列表。代码按需包含 `glm/glm.hpp`、矩阵变换或四元数扩展即可。

第一次验证用了一个很朴素的平移：把 `(1, 1, 1, 1)` 乘上 X 方向平移 2 个单位的矩阵，结果的 X 应该是 3。

```cpp
glm::vec4 point(1.0f, 1.0f, 1.0f, 1.0f);
glm::mat4 translation = glm::translate(
    glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));

glm::vec4 result = translation * point;
GL_CORE_INFO("GLM Math Test: Result X = {0}", result.x);
```

这个测试之后，GLM 逐渐进入了真正的引擎数据结构。`TransformComponent` 用平移、旋转和缩放组合模型矩阵，`SceneCamera` 用 `glm::perspective` 或 `glm::ortho` 生成投影，Renderer 再把这些矩阵传给 Shader。早期的一行 `result.x == 3`，后来成了整条变换链的起点。

![GLM 平移矩阵测试](README.assets/image-20260329145655166.png)

## 渲染上下文

GLAD 第一次接入时，创建当前 Context、加载函数指针和交换缓冲区都写在 `WindowsWindow` 里。功能可以运行，但窗口类已经同时处理系统窗口、输入回调和 OpenGL 启动，继续往里加渲染细节只会让它更难维护。

`GraphicsContext` 是这次拆分留下的最小接口。它只约定初始化与交换缓冲区，不暴露 GLFW 或 OpenGL 类型：

```cpp
class GraphicsContext
{
public:
    virtual ~GraphicsContext() = default;
    virtual void Init() = 0;
    virtual void SwapBuffers() = 0;
};
```

当前实现是 `OpenGLContext`。它保存窗口句柄，在 `Init()` 中把窗口设为当前 Context，调用 GLAD 加载函数指针，并输出 Vendor、Renderer 和 Version。每帧结束时，`SwapBuffers()` 再把 GLFW 的前后缓冲区交换封装起来。

`WindowsWindow` 使用 `Scope<GraphicsContext>` 持有上下文。窗口创建成功后构造并初始化它，`OnUpdate()` 只负责轮询事件，再通过接口交换缓冲区：

```cpp
m_Context = CreateScope<OpenGLContext>(m_Window);
m_Context->Init();

void WindowsWindow::OnUpdate()
{
    glfwPollEvents();
    m_Context->SwapBuffers();
}
```

`Scope` 补上了早期裸指针版本的所有权问题，窗口销毁时 Context 会随成员自动释放。这里的抽象仍有一段没走完：`WindowsWindow` 目前直接构造 `OpenGLContext`，还没有根据 Renderer API 选择实现的工厂。现阶段它隔离了职责，却没有让图形后端真正做到可切换。

![OpenGLContext 初始化验证](README.assets/image-20260329164819786.png)

## 首个三角形

窗口、OpenGL Context 和 GLAD 都能正常启动后，我需要一个足够小的渲染目标来验证整条链路。三角形正合适：三个顶点、一组三角形索引，再配一对最简单的 Shader，任何一步出错都会直接表现为黑屏。

第一版代码直接写在 Application 附近，完全使用 OpenGL 原生接口。VBO 保存三个顶点的位置，EBO 保存 `{0, 1, 2}` 的绘制顺序，VAO 记录位置属性的解析方式。创建 EBO 时必须保持 VAO 处于绑定状态，否则索引缓冲不会成为这个 VAO 的状态。

```cpp
float vertices[] = {
    -0.5f, -0.5f, 0.0f,
     0.5f, -0.5f, 0.0f,
     0.0f,  0.5f, 0.0f
};
uint32_t indices[] = { 0, 1, 2 };

glBindVertexArray(vertexArray);
glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
```

绘制时绑定 Shader 与 VAO，再调用 `glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr)`。屏幕上出现三角形后，至少能确认 Context、函数加载、显存上传、顶点布局和 Draw Call 已经连通。那时的代码很粗糙，但故障范围足够小，适合做第一次排查。

这些原生调用后来分别进入 `VertexBuffer`、`IndexBuffer`、`VertexArray`、`Shader` 和 `RendererAPI`。当前正式路径通过 `RenderCommand::DrawIndexed()` 绘制，Application 已经不再保存三角形的 OpenGL ID。

![首个索引三角形](README.assets/image-20260329175722236.png)

## Shader

首个三角形最开始使用内嵌 GLSL。顶点 Shader 负责写入 `gl_Position`，片元 Shader 输出颜色。把编译和链接代码继续留在 Application 里很快就会失控，尤其是语法错误只留下黑屏时，排查体验相当差。

我先封装了 Shader Program 的创建、绑定和销毁，随后又把接口与 OpenGL 实现拆开。当前 `Shader` 是渲染层接口，`Shader::Create()` 返回 `Ref<Shader>`；OpenGL 后端由 `OpenGLShader` 负责。它可以从顶点、片元源码创建，也可以读取带 `#type vertex` 与 `#type fragment` 分段的 `.glsl` 文件。

```cpp
class Shader
{
public:
    virtual ~Shader() = default;
    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;

    static Ref<Shader> Create(const std::string& filepath);
    static Ref<Shader> Create(
        const std::string& name,
        const std::string& vertexSrc,
        const std::string& fragmentSrc);
};
```

`OpenGLShader` 会逐阶段编译源码，再链接成 Program。失败时会收集驱动返回的编译或链接日志，并清理已经创建的 Shader 对象。文件重载采用先构建新 Program、成功后再替换旧对象的顺序，所以一次编辑错误不会立刻销毁当前仍可用的 Shader。成功重载会增加版本号，并让依赖它的渲染缓存知道资源已经变化。

最初我做了两种片元输出。固定 RGBA 用来确认基础路径；位置渐变则把顶点位置传给片元阶段，观察光栅化插值是否符合预期。

```glsl
#version 330 core
layout(location = 0) out vec4 color;
in vec3 v_Position;

void main()
{
    color = vec4(v_Position * 0.5 + 0.5, 1.0);
}
```

![根据顶点位置生成的渐变](README.assets/image-20260330104705597.png)

![固定颜色输出](README.assets/image-20260330104640582.png)

现在 Shader 还由 `ShaderLibrary` 按名称管理，并支持文件监视与批量重载。图形 Shader 的当前工厂仍创建 OpenGL 实现，Vulkan 后端只是接口预留，不能当作已经可用。

## Uniform 上传

Shader 能编译以后，下一步是让 CPU 在运行时传入数据。Uniform 最早只用来上传 `u_Time`，后来扩展到相机矩阵、材质参数、纹理槽和环境模拟数据，已经成了渲染路径里最常用的接口之一。

当前 `Shader` 提供整数、整数数组、标量、`vec2` 到 `vec4`、`mat4` 以及纹理绑定接口。OpenGL 实现先按名称取得 Uniform Location，再调用对应的 `glUniform*`。矩阵通过 `glm::value_ptr()` 取得连续数据，并按不转置的方式上传。

```cpp
void OpenGLShader::UploadUniformFloat(
    const std::string& name, float value)
{
    glUniform1f(GetUniformLocation(name), value);
}

void OpenGLShader::UploadUniformMat4(
    const std::string& name, const glm::mat4& matrix)
{
    glUniformMatrix4fv(
        GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(matrix));
}
```

Location 查询结果保存在 `m_UniformCache`，同一个名字不会每帧重复调用 `glGetUniformLocation()`。Shader 成功重载后 Program ID 会变化，旧 Location 随即失效，因此重载路径会清空缓存。调用上传函数前仍要先绑定目标 Shader，这是 OpenGL 状态机留下的使用约定。

第一个动态实验把 `glfwGetTime()` 上传给 `u_Time`，片元 Shader 用正弦函数改变颜色。随后我又在顶点阶段修改 Y 坐标，做了一个轻微摆动的三角形。

![时间驱动的颜色变化](README.assets/image-20260330120556157.png)

![时间驱动的顶点摆动](README.assets/image-20260330120936484.png)

继续试验时，我让三个颜色通道使用不同相位，得到了一版流动的彩色效果。这段 Shader 没有进入正式渲染管线，但它很适合确认 `u_Time` 和插值数据都在逐帧更新。

![基于位置和时间的颜色实验](README.assets/image-20260330120529730.png)

这里还踩过一个很具体的坑：顶点 Shader 修改了局部变量 `pos`，却仍把原始 `a_Position` 写入 `v_Position`。几何已经发生摆动，片元颜色仍按旧坐标计算，两种效果看起来像错开了一层。把输出改为 `v_Position = pos` 后，颜色才会跟随变形后的顶点数据。

```glsl
vec3 pos = a_Position;
pos.y += sin(pos.x * 5.0 + u_Time) * 0.1;
v_Position = pos;
gl_Position = u_ViewProjection * vec4(pos, 1.0);
```

![插值位置未同步时的错误效果](README.assets/image-20260330140556451.png)

今天的 Uniform 已经不只服务这些小实验。Renderer 会上传 ViewProjection 和 Transform，材质系统会提交 PBR 参数，Compute Shader 也使用同样的思路传递模拟步长与环境数据。早期的 `u_Time` 测试留下了一个实用习惯：先用能直接看见的变化验证数据通路，再把接口接进更复杂的渲染逻辑。



## Buffer 抽象

首个三角形跑通后，Application 里还散落着 `glGenBuffers`、`glBufferData` 和资源销毁代码。继续照这个方式增加网格，很快就会出现重复的创建流程，也很难看出谁负责释放显存。我先把顶点缓冲和索引缓冲从这些调用里拆了出来。

`VertexBuffer` 负责顶点数据，可以用现有数据创建静态缓冲，也可以只分配容量，之后通过 `SetData()` 更新。`IndexBuffer` 保存索引并记录数量，绘制时不必在外部重复维护 count。两者都提供绑定接口和静态工厂，当前返回 `Ref`，GPU 对象会随最后一个引用释放。

```cpp
auto vertexBuffer = VertexBuffer::Create(vertices, sizeof(vertices));
auto dynamicBuffer = VertexBuffer::Create(maxVertexBytes);
dynamicBuffer->SetData(batchVertices, usedVertexBytes);

auto indexBuffer = IndexBuffer::Create(indices, indexCount);
```

OpenGL 后端在构造函数中创建 Buffer，析构时调用 `glDeleteBuffers()`。带初始数据的 VertexBuffer 使用 `GL_STATIC_DRAW`；只分配容量的版本使用 `GL_DYNAMIC_DRAW`，更新时调用 `glBufferSubData()`。这个差别后来直接支撑了 Renderer2D 的批量顶点上传和 Renderer3D 的实例数据更新。

工厂目前仍直接创建 `OpenGLVertexBuffer` 与 `OpenGLIndexBuffer`。接口已经挡住上层的 OpenGL 类型，后端选择却还没有进入工厂逻辑。Vulkan 要真正接入时，这里仍需要按 Renderer API 分派。

我当时最在意的是把资源生命周期收回来。原生 ID 一旦散在 Application 和 Layer 里，很容易漏删；改成 `Ref<Buffer>` 后，创建位置和实际共享关系更容易追踪。

## 缓冲区布局与顶点数组封装

Buffer 只能保存字节，GPU 还需要知道每段数据的含义。最早的 `glVertexAttribPointer()` 把分量数、步长和偏移全写成数字，顶点结构一改，这些数字就可能悄悄错位。`BufferLayout` 就是为了解决这类同步问题。

`BufferElement` 记录属性名称、`ShaderDataType`、大小、偏移、归一化标记和输入频率。`BufferLayout` 按声明顺序累加大小，算出每个元素的 Offset 与整条顶点的 Stride：

```cpp
vertexBuffer->SetLayout({
    { ShaderDataType::Float3, "a_Position" },
    { ShaderDataType::Float3, "a_Normal" },
    { ShaderDataType::Float2, "a_TexCoord" }
});
```

这套计算按字段紧密排列，不会替 C++ 结构体补齐额外对齐。传入的真实顶点内存必须和 Layout 保持一致；如果以后给顶点结构增加显式对齐，布局计算也要一起调整。

`VertexArray` 把一个或多个 VertexBuffer、它们的 Layout 以及 IndexBuffer 组合成可绘制状态。`OpenGLVertexArray::AddVertexBuffer()` 会拒绝空 Layout，然后依次配置属性位置。浮点属性走 `glVertexAttribPointer()`，整数属性走 `glVertexAttribIPointer()`；矩阵拆成多列，占用连续的 attribute location。

当前 Layout 还支持 `PerVertex` 和 `PerInstance` 两种输入频率。实例属性会调用 `glVertexAttribDivisor(location, 1)`，这正是 Renderer3D 批量提交 Transform 与 Entity 数据时使用的路径。

```cpp
instanceBuffer->SetLayout({
    { ShaderDataType::Mat4, "a_InstanceTransform", false,
      BufferInputRate::PerInstance },
    { ShaderDataType::Int4, "a_InstanceEntityData", false,
      BufferInputRate::PerInstance }
});
```

VAO 内部保留这些 Buffer 的 `Ref`，避免顶点状态仍在使用时底层对象已经销毁。设置 IndexBuffer 时会先绑定 VAO，再绑定 EBO，让索引缓冲成为对应 VAO 的状态。到这里，上层组装网格时已经不需要直接计算 attribute offset，也不再碰 OpenGL ID。

## Renderer 分层

Buffer 和 VertexArray 封装完成后，清屏、深度状态和 Draw Call 仍然由业务代码直接调用。为这些操作再加一层包装很有必要，但我不想把所有事情塞进一个巨大的 Renderer，于是先分成 `RendererAPI`、`RenderCommand` 和 `Renderer` 三层。

`RendererAPI` 描述后端动作。当前接口覆盖初始化、颜色与深度清理、混合和深度状态，以及普通或实例化索引绘制。`OpenGLRendererAPI` 把这些操作翻译成 `glClear`、`glDepthMask`、`glDrawElements` 和 `glDrawElementsInstanced`。绘制函数可以接收显式 index count；传入 0 时使用 VertexArray 中 IndexBuffer 的完整数量。

`RenderCommand` 是一组薄的静态转发函数。Renderer2D、Renderer3D、ShadowRenderer 和 RenderPass 都通过它修改状态或提交 Draw Call，调用方不会直接包含 OpenGLRendererAPI。

```cpp
RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
RenderCommand::Clear();
RenderCommand::SetBlendEnabled(true);
RenderCommand::DrawIndexed(vertexArray);
```

高层 `Renderer` 保存当前场景的 ViewProjection，`Submit()` 负责绑定 Shader、上传 `u_ViewProjection` 与 `u_Transform`，再把 VertexArray 交给 RenderCommand。初始化入口现在还会启动 Renderer2D、Renderer3D、TerrainRenderer、环境光照和 SkyboxRenderer，并创建共享的灯光 UniformBuffer。

```cpp
Renderer::BeginScene(camera);
Renderer::Submit(shader, vertexArray, transform);
Renderer::EndScene();
```

这套分层后来容纳了批处理、实例化、阴影和地形，但底层选择仍是固定的。`RendererAPI` 虽然声明了 OpenGL 与 Vulkan 枚举，`RenderCommand` 当前持有的对象依旧直接由 `new OpenGLRendererAPI()` 创建，Buffer、VertexArray 和 Shader 工厂也采用同样方式。调用 `SetAPI()` 只会改变枚举值，不会自动替换后端对象。文档里的 Vulkan 因此只是接口预留。

这一轮重构最实际的变化，是 Application 和 Layer 开始使用 `清屏`、`提交网格` 这样的渲染语义。OpenGL 调用集中到了 Platform 后端，后面调整深度、混合或实例绘制时，不必再沿着所有业务层逐个修改。

## 正交摄像机

最初的三角形直接写在标准化设备坐标里，顶点一旦确定，画面就只能跟着窗口比例变化。我想让渲染使用世界坐标，也希望镜头可以移动，于是先实现了一台二维正交摄像机。

`OrthographicCamera` 保存 Projection、View 和两者的乘积。Projection 由 `glm::ortho()` 生成，当前深度范围是 `-100` 到 `100`。摄像机的位置或旋转变化后，会先组合自身 Transform，再取逆得到 View Matrix：

```cpp
glm::mat4 transform =
    glm::translate(glm::mat4(1.0f), m_Position)
    * glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation),
        glm::vec3(0.0f, 0.0f, 1.0f));

m_ViewMatrix = glm::inverse(transform);
m_ViewProjectionMatrix = m_ProjectionMatrix * m_ViewMatrix;
```

Renderer 在 `BeginScene()` 中保存 ViewProjection，提交物体时再上传给 Shader。这样摄像机向右移动，场景会在屏幕上向左移动；Shader 只消费最终矩阵，不需要知道镜头的位置和旋转是怎样计算的。

第一版控制逻辑直接在 Layer 里轮询 WASD。画面可以移动后，我才注意到宽高比也在影响结果。投影范围如果没有按窗口比例设置，同一个三角形会被横向或纵向拉伸。把横向范围设为 `aspectRatio * zoom` 后，物体比例才稳定下来。

![正交摄像机移动测试](README.assets/image-20260330194300345.png)

![修正宽高比后的画面](README.assets/image-20260330194657176.png)

这部分后来收进 `OrthographicCameraController`。Controller 负责 WASD 平移、可选的 Q/E 旋转，并响应滚轮和窗口缩放事件。滚轮改变 Zoom Level，窗口变化则重新计算 Aspect Ratio，最后都通过 `SetProjection()` 更新投影。Camera 本身只保存数学状态，输入策略留在 Controller 中。

## Timestep

摄像机刚能移动时，速度写成了每帧增加 `0.01f`。这在我的机器上看起来正常，换到不同帧率后移动距离立刻变了。问题出在单位：代码表达的是每帧位移，真正想要的是每秒速度。

Application 现在每轮读取 `glfwGetTime()`，用当前时间减去上一帧时间得到 Timestep，再把它传给所有 Layer：

```cpp
float time = static_cast<float>(glfwGetTime());
Timestep timestep = time - m_LastFrameTime;
m_LastFrameTime = time;

for (Layer* layer : m_LayerStack)
    layer->OnUpdate(timestep);
```

`Timestep` 只是一个很薄的秒数包装，支持隐式转成 `float`，也可以显式读取秒或毫秒。移动代码因此可以直接写成速度乘时间：

```cpp
if (Input::IsKeyPressed(GL_KEY_A))
    position.x -= translationSpeed * ts;
if (Input::IsKeyPressed(GL_KEY_D))
    position.x += translationSpeed * ts;
```

总运行时间和帧间隔解决的是两类问题。`Application::GetTime()` 适合给 `u_Time` 提供连续相位，动画可以按绝对时间计算。Timestep 适合积分速度、旋转速度或模拟变化率。Shader 通常没有跨帧累加状态，因此只传 Delta Time 也无法凭空得到稳定的总时间。

当前主循环使用可变 Timestep，没有钳制最大 Delta。窗口被拖住或调试器暂停后，下一帧可能收到很大的值。普通相机移动会直接反映这次停顿，需要确定性或稳定性的模拟则应使用自己的固定步长调度，而不是原样消费 Application Delta。

这次改动还把早期渲染测试从 Application 移进了 ExampleLayer。Application 只负责计算时间并更新 Layer，Sandbox 自己持有 Camera、Shader 和网格。最终的三角形可以按秒速移动，同时继续使用 `u_Time` 驱动颜色与顶点变化。

![使用 Timestep 后的 Sandbox 三角形](README.assets/image-20260330223810703.png)

## 变换矩阵

摄像机解决了观察位置，物体本身仍然共享同一组原始顶点。为了让同一个网格出现在不同位置，我给 `Renderer::Submit()` 增加了 Model Transform，并约定顶点 Shader 使用 `ViewProjection * Transform * Position`。

```glsl
uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;

void main()
{
    gl_Position = u_ViewProjection
        * u_Transform
        * vec4(a_Position, 1.0);
}
```

CPU 侧最早用 GLM 手动组合矩阵。按当前列向量约定，`translate * rotate * scale` 作用到顶点时会先缩放，再旋转，最后平移。顺序写反通常不会报错，只会得到一个很难解释的运动轨迹。

```cpp
glm::mat4 transform =
    glm::translate(glm::mat4(1.0f), position)
    * glm::rotate(glm::mat4(1.0f), glm::radians(rotation),
        glm::vec3(0.0f, 0.0f, 1.0f))
    * glm::scale(glm::mat4(1.0f), scale);

Renderer::Submit(shader, vertexArray, transform);
```

第一次验证复用了同一个四边形 VAO，在双层循环中生成不同的平移和缩放矩阵。随后加上按时间和网格位置变化的旋转角度，确认每次 Submit 都能把自己的 `u_Transform` 送到 Shader。

![使用 Transform 绘制方块阵列](README.assets/image-20260331104756545.png)

![加入旋转后的方块阵列](README.assets/image-20260331110107923.png)

现在场景实体通过 `TransformComponent` 保存 Translation、欧拉角 Rotation 和 Scale。`GetTransform()` 用 Z、Y、X 顺序组合四元数，再返回 `T * R * S`。Renderer2D 会在 DrawQuad 系列接口中构造相同含义的矩阵，Renderer3D 则把它放进渲染项或实例数据。早期一格一格提交方块的实验没有性能优势，但它确认了网格数据和物体位姿可以独立复用。

## 纹理

方块有了 Transform 以后，纯色很快就不够用了。接入纹理要解决两件事：从磁盘得到可靠的像素数据，以及把采样规则和 GPU 资源生命周期收进渲染接口。第一版使用 stb_image 读取 PNG、JPG，再由 `Texture2D` 工厂创建 OpenGL 对象。

当前 `Texture2D` 已经支持从文件、尺寸或完整 `TextureSpecification` 创建。Specification 记录格式、过滤方式、Wrap、Usage 和颜色空间。文件纹理默认使用 sRGB，适合 Base Color 这类颜色数据；法线、AO、高度等数值纹理应传入 Linear，避免采样时发生错误的伽马转换。

```cpp
auto colorTexture = Texture2D::Create(
    "assets/textures/Henry.jpg",
    TextureColorSpace::SRGB);

auto dataTexture = Texture2D::Create(
    "assets/textures/heightmap.png",
    TextureColorSpace::Linear);
```

`OpenGLTexture2D` 会让 stb_image 纵向翻转文件，按 1、3 或 4 通道选择 R8、RGB8 或 RGBA8，再创建不可变存储并上传像素。`SetData()` 会检查上传大小，接口还提供 Clear、Readback、Bind 和 Renderer ID 查询。析构函数负责删除 OpenGL Texture。

顶点侧增加 UV 后，Shader 用 `sampler2D` 采样，再与 Tint Color 相乘。Renderer2D 现在用一张 1x1 白纹理统一纯色与贴图路径，一个 Batch 最多维护 32 个纹理槽；相同纹理会复用已有 slot，槽位用完时才 Flush。

```glsl
vec4 texColor = v_Color;
switch (int(v_TexIndex))
{
case 0:
    texColor *= texture(u_Textures[0], v_TexCoord * v_TilingFactor);
    break;
case 1:
    texColor *= texture(u_Textures[1], v_TexCoord * v_TilingFactor);
    break;
// 其余纹理槽使用相同方式展开
}
color = texColor;
```

这里还有两处当前限制。文件路径工厂仍直接创建 `OpenGLTexture2D`，只有 Specification 工厂按 Renderer API 分支；另外 2D Texture 目前只分配一个 Mip Level，`LinearMipmapLinear` 枚举尚未对应完整的 Mip Chain。接口已经预留，实际行为仍以这两条为准。

![纹理与颜色相乘后的测试画面](README.assets/image-20260331154634223.png)

## Alpha 混合

第一次换成带透明通道的 PNG 时，透明区域显示成了黑色。像素里有 Alpha 只代表数据存在，光栅化阶段还要决定它怎样和 Framebuffer 里的颜色合成。早期修复是在 OpenGL 初始化时启用 Blend，并使用常见的 Source Alpha 公式。

![尚未启用混合时的透明纹理](README.assets/image-20260331162819221.png)

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

这个公式相当于 `source * alpha + destination * (1 - alpha)`。它解决了黑底问题，却也暴露了全局状态的麻烦：如果 Blend 一直开着，Opaque、阴影和需要深度写入的 Pass 都会受到影响。

![最初接入 Blend 状态](README.assets/image-20260331162947946.png)

当前 `OpenGLRendererAPI::Init()` 会设置混合函数，但默认关闭 Blend。Renderer2D 在 `StartBatch()` 时开启 Source Alpha 混合，`EndScene()` 后关闭。Renderer3D 把 `MaterialAlphaMode::Blend` 项目放进透明队列，按相机距离从远到近排序，绘制时关闭深度写入；队列结束后会恢复 Blend、Depth Write 和 Depth Function。

`Mask` 使用的是另一条路径。它保留深度写入，通过 Alpha Cutoff 丢弃片元，不需要颜色混合。Opaque、Mask 和 Blend 因此有各自的状态约定，不能只凭纹理文件是否带 Alpha 来决定。

![按 Alpha 正确合成后的 PNG](README.assets/image-20260331163140298.png)

这次问题给我留下的教训很直接：渲染状态要由当前 Pass 设置并负责恢复。依赖某次初始化留下的全局状态，短期能看到正确画面，后面加入更多 Pass 时很容易互相污染。

## 单文件多着色器模式

Shader 还写在 C++ 字符串里时，每改一行 GLSL 都要重新编译应用，错误位置也很难读。我把 Shader 搬到 `.glsl` 文件，并约定用 `#type` 把多个阶段放在同一个文件里。对常见的顶点与片元组合来说，一个资源文件比两条独立路径更容易管理。

```glsl
#type vertex
#version 330 core

void main()
{
    gl_Position = vec4(a_Position, 1.0);
}

#type fragment
#version 330 core

void main()
{
    color = vec4(1.0);
}
```

`OpenGLShader::ReadFile()` 以二进制方式读取内容，并移除可能存在的 UTF-8 BOM。`PreProcess()` 查找每个 `#type` 行，把后续源码切分到对应阶段。缺少标签、未知类型或空阶段都会返回明确错误。当前图形 Shader 只接受 `vertex`、`fragment` 和 `pixel` 别名；Compute Shader 使用独立的 `ComputeShader` 接口。

文件模式后来接上了 FileWatcher。检测到修改后，Shader 会先编译一个新 Program，成功才替换旧 Program，并清空 Uniform Location 缓存、增加 Version。编译失败时旧对象继续工作，编辑器可以显示错误而不必把当前画面一起弄丢。

我用这套文件格式做的第一个复杂实验是一张程序化漩涡背景。最早版本用极坐标和 FBM 扰动 UV，能动，但离参考效果很远。

![第一版程序化漩涡](README.assets/image-20260331193141079.png)

第二次增加了 `u_VortexAmt`，让扭曲强度随时间变化。轮廓接近了一些，颜色和纹理组织仍显得生硬。

![加入 Vortex 强度后的版本](README.assets/image-20260331195911871.png)

最后一版改用屏幕空间 `gl_FragCoord` 与 `u_Resolution`，再叠加像素化、角度扭曲和多轮正弦扰动。当前 Sandbox 的 `BalatroVortex.glsl` 保留的是这条实现，`Renderer2D::DrawFullscreenQuad()` 会上传时间、分辨率并绘制全屏四边形。

![适配屏幕空间后的漩涡效果](README.assets/image-20260331200053084.png)

这个实验没有发展成通用材质系统，但它验证了外部 Shader、文件重载和全屏 Pass 可以一起工作。调试循环也变成了保存文件、查看结果、继续修改，不再需要重编整个客户端。

## 着色器库

着色器数量变多以后，继续在每个 Layer 里保存一组 `Ref<Shader>` 很快就会变得难以维护。加载路径、对象名称和实际用途混在一起，调用方还得自己判断某个着色器是否已经创建。于是这一阶段加入了 `ShaderLibrary`，把常用着色器集中登记，再通过名称取用。

它内部是一张 `std::unordered_map<std::string, Ref<Shader>>`。使用文件路径加载时，库会采用着色器自身的名称，也就是文件名去掉扩展名后的部分；需要更清楚的业务名称时，也可以在加载时指定别名。

```cpp
m_ShaderLibrary.Load("assets/shaders/BalatroVortex.glsl");
m_ShaderLibrary.Load("Blinn-Phong", "assets/shaders/BlinnPhong.glsl");

Ref<Shader> shader = m_ShaderLibrary.Get("Blinn-Phong");
```

`Add`、`Get` 和 `Remove` 都会检查名称是否合法。重复登记或读取不存在的条目会触发断言，这比让空引用一路传到渲染阶段更容易定位问题。库中保存的是 `Ref<Shader>`，调用方拿到对象后可以直接共享，不需要额外处理生命周期。

前一章实现的安全重载也接到了这里。编辑器中的 Shader 面板持有一个库实例，可以调用 `ReloadChanged()` 只处理发生变化的文件，也可以用 `ReloadAll()` 主动重载全部条目。重载是否成功仍由每个 `Shader` 自己负责，库只做查找和批量调度。

这里有一个容易误判的边界：`ShaderLibrary` 目前是实例内的注册表，并不是全局资源缓存。Sandbox、示例 Layer 和编辑器各自创建库时，同一路径仍可能被重复加载和编译。现阶段这种设计足够直接，也避免了全局状态；如果以后需要统一资产管理，路径规范化和跨库去重应该放到更上层解决。

## 正交摄像机控制器

早期示例把按键判断、滚轮缩放和窗口尺寸变化直接写在 Layer 中。代码量不算大，但每增加一个二维场景都要复制一遍，而且 Layer 开始同时承担输入规则和渲染逻辑。为此项目增加了 `OrthographicCameraController`，把这一组常用行为收进一个可复用对象。

控制器持有正交摄像机、宽高比、缩放级别和移动状态。每帧更新时，它轮询 W、A、S、D，并用 `Timestep` 修正位移，因此移动速度不会跟着帧率变化。构造时可以开启旋转，开启后 Q、E 会更新摄像机角度。移动速度会随当前缩放级别调整，画面拉近时每秒跨过的世界坐标也会减少，操作起来更细一些。

Layer 只需要转发更新和事件：

```cpp
void Sandbox2D::OnUpdate(Timestep ts)
{
    m_CameraController.OnUpdate(ts);

    Renderer2D::BeginScene(m_CameraController.GetCamera());
    // 提交场景内容
    Renderer2D::EndScene();
}

void Sandbox2D::OnEvent(Event& event)
{
    m_CameraController.OnEvent(event);
}
```

滚轮事件会修改缩放级别，并把最小值限制为 `0.25f`，避免投影范围缩到零或翻转。窗口尺寸改变时，控制器重新计算宽高比和投影矩阵。这两个事件处理函数都会返回 `false`，所以控制器读取事件后不会阻止它继续向后传播。

当前接口还有一个值得记住的小边界：滚轮路径会执行最小值限制，`SetZoomLevel()` 则直接相信调用方。编辑器或脚本如果主动设置缩放值，需要自己保证它大于零。这个约束暂时没有藏进控制器内部，后续统一摄像机参数接口时可以再收紧。

## Renderer2D

完成缓冲区、顶点数组、纹理和着色器封装后，Layer 里仍然留着不少固定流程：创建四边形网格、准备白色纹理、绑定 Shader，再按顺序提交绘制。它们都属于二维渲染的内部细节，于是这一阶段把这些工作集中到 `Renderer2D`，让场景代码只描述要画什么。

最初拆分 Sandbox 示例时还遇到过一次入口冲突。`EntryPoint.h` 会生成 `main()`，如果它随着 Layer 实现被多个编译单元包含，链接阶段就会出现重复定义。现在入口只保留在 `SandboxApp.cpp`，`Sandbox2D.cpp` 负责具体场景，两部分的职责也因此清楚了许多。

现在的 `Renderer2D` 已经采用批处理。初始化阶段会一次性准备可容纳 20,000 个四边形的动态顶点缓冲区，并生成对应的索引缓冲区。每次调用 `DrawQuad()` 时，顶点先写入 CPU 端缓存；到 `EndScene()`，或批次容量达到上限时，再把有效数据上传并提交一次绘制。统计信息中的 `DrawCalls` 记录实际批次数，`QuadCount` 则记录本帧写入的四边形数量。

```cpp
Renderer2D::ResetStats();
Renderer2D::BeginScene(m_CameraController.GetCamera());

Renderer2D::DrawQuad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, { 0.2f, 0.7f, 0.9f, 1.0f });
Renderer2D::DrawRotatedQuad(
    { 1.5f, 0.0f }, { 1.0f, 1.0f },
    glm::radians(30.0f), m_CheckerboardTexture, 4.0f
);

Renderer2D::EndScene();
```

纯色和纹理四边形共用同一套顶点格式。槽位 0 固定放置一张 1×1 白色纹理，纯色绘制也走纹理采样，再乘上传入颜色；这样批次不必因为材质类型不同而拆开。一个批次最多使用 32 个纹理槽，相同纹理会复用已有槽位。顶点中还保留了 `EntityID`，编辑器的鼠标拾取可以沿用同一条绘制路径。

`BeginScene()` 会更新绑定点 0 上的摄像机 UBO，其中包含 ViewProjection 矩阵和当前时间，然后重置批次状态。绘制期间顶点变换在 CPU 端完成；`EndScene()` 上传数据、绑定本批次用到的纹理并调用 `DrawIndexed`。混合状态也在批次开始和结束时成对处理，带透明通道的精灵可以直接参与绘制。

![Renderer2D 示例](README.assets/image-20260413104502041.png)

这一版已经把常用二维绘制从 Layer 中拿走，但它仍是静态的全局渲染器，并且在 `BeginScene()` 中直接读取 `Application` 的时间。对于当前编辑器和 Sandbox 来说，这种接线方式简单有效；以后如果要支持多渲染上下文或离线渲染，这两处依赖会是需要继续拆分的地方。自定义全屏 Shader 的接口留到下一章再展开。

## Uniform解耦/全屏shader接口

普通精灵可以共享 Renderer2D 的 Texture Shader，但程序化背景和后处理 Shader 往往还需要时间、分辨率或输入纹理。最初我尝试给 `DrawQuad()` 增加自定义 Shader 重载，结果很快碰到坐标系问题：场景四边形经过摄像机矩阵，全屏效果需要的却是稳定的屏幕坐标。继续往普通 Quad 接口里塞参数，只会让两种用途越缠越紧。

后来单独增加了全屏绘制路径。`Renderer2D::Init()` 会创建一套覆盖 NDC 的四边形 VAO，顶点只包含位置和纹理坐标。`DrawFullscreenQuad()` 绑定调用方提供的 Shader，并按固定名称上传引擎能够提供的数据：

```cpp
shader->UploadUniformMat4("u_ViewProjection", glm::mat4(1.0f));
shader->UploadUniformMat4("u_Transform", fullscreenTransform);
shader->UploadUniformFloat("u_Time", s_Data.CameraBuffer.Time);
shader->UploadUniformFloat2("u_Resolution", {
    static_cast<float>(window.GetWidth()),
    static_cast<float>(window.GetHeight())
});
```

调用端因此可以保持很短：

```cpp
auto shader = m_ShaderLibrary.Get("BalatroVortex");
Renderer2D::DrawFullscreenQuad(shader, 0.9f);
```

这里的 `depth` 已经处于 NDC 语义下，和经过摄像机投影的世界坐标 Z 值不能直接比较。全屏背景、场景内容和后处理最好按独立 Pass 排列，由调用方明确设置渲染目标、清理规则和深度写入状态。把全屏绘制插进尚未提交的 2D 批次中，也容易留下 Shader 状态冲突。

`DrawPostProcess()` 在这条路径上多做了一步：把输入颜色附件绑定到 0 号槽，并约定采样器名称为 `u_SceneTexture`，随后复用全屏四边形。现在的 Bloom、雾和 Tone Mapping 已经由 `PostProcessRenderer` 组织 Pass，底层仍调用这个接口。

![全屏 Shader 运行效果](README.assets/image-20260413142738033.png)

这套接口解决了当时的接线问题，不过它还算不上完整的 Uniform 系统。`u_Time`、`u_Resolution` 和 `u_SceneTexture` 都依赖名称约定，额外参数仍由调用方上传；分辨率取自应用窗口，并不一定等于当前 Framebuffer 尺寸。时间则来自最近一次 `BeginScene()` 写入的摄像机缓冲。以后如果继续整理全屏 Pass，这两项应改成显式输入，减少对 Application 和调用顺序的依赖。

## 白贴图模式

开始做 Renderer2D 时，纯色方块和纹理方块各走一套 Shader。表面上很好理解，实际批处理时却很麻烦：颜色块会迫使渲染器切换 Shader，也无法和相邻精灵留在同一个批次。白贴图就是为了解掉这个分支。

初始化时，Renderer2D 创建一张 1×1 的 RGBA8 纹理，把唯一像素写成 `0xffffffff`，并固定放在纹理槽 0：

```cpp
s_Data.WhiteTexture = Texture2D::Create(1, 1);

uint32_t whitePixel = 0xffffffff;
s_Data.WhiteTexture->SetData(&whitePixel, sizeof(whitePixel));
s_Data.TextureSlots[0] = s_Data.WhiteTexture;
```

纯色 Quad 的顶点使用槽 0，颜色字段保存调用方传入的颜色。片元阶段仍执行纹理采样和颜色相乘，因为白色采样值是 1，结果正好保留顶点颜色。真实纹理走相同公式，只是 `TexIndex` 指向其所在槽位，`tintColor` 负责染色。

这一步真正有价值的地方，是让纯色、贴图、平铺和 Tint 共用同一种顶点格式与 Texture Shader。后来的 32 纹理槽批处理正是沿着这个约定建立的，白贴图一直占用 0 号槽，其余纹理从 1 开始登记。

`Texture2D` 如今也不再只有早期的宽高构造函数。它支持 `TextureSpecification`，可以指定格式、过滤、寻址、用途和颜色空间；`SetData()` 会按规格校验完整上传大小。白贴图仍使用最简单的 `Create(1, 1)`，因为这里确实不需要额外配置。

![白贴图统一后的绘制结果](README.assets/image-20260415211525172.png)

白贴图当前是 Renderer2D 私有资源，没有公共获取接口，也不是全引擎共享的默认纹理。Renderer3D 会为自己的材质回退单独创建白贴图。两边采用相同思路，但生命周期和槽位规则各自管理，调用方不该假设它们指向同一个 GPU 对象。

## 仪器测量

渲染结果正常以后，我开始需要回答更具体的问题：一帧时间花在哪里，偶发卡顿落在哪个函数，调整批处理后 CPU 提交有没有真的减少。单看 FPS 很难追到调用链，所以项目加入了一套轻量的 CPU Instrumentation。

核心实现位于 `Glimmer/Debug/Instrumentor.h`。`InstrumentationTimer` 在构造时记录起点，离开作用域时自动停止，再把名称、微秒时间戳和线程 ID 写入单例 `Instrumentor`。输出格式兼容 Chrome Trace Event，可以在时间轴查看函数的开始位置和持续时间。

日常埋点通过宏完成：

```cpp
void Sandbox2D::OnUpdate(Timestep ts)
{
    GL_PROFILE_FUNCTION();

    {
        GL_PROFILE_SCOPE("CameraController::OnUpdate");
        m_CameraController.OnUpdate(ts);
    }

    // 本帧其他工作
}
```

`GL_PROFILE_FUNCTION()` 使用 MSVC 的 `__FUNCSIG__` 记录完整函数签名，`GL_PROFILE_SCOPE()` 适合包住一段更有意义的工作。Application、窗口、OpenGL 后端、摄像机和 Renderer2D 等位置已经保留了这些埋点。关闭性能测量时，宏会展开为空，不需要逐处删除。

入口把一次运行拆成 Startup、Runtime 和 Shutdown 三个 Session，分别生成 JSON 文件。这样初始化资源、主循环和释放阶段不会挤在同一条时间轴上：

```cpp
GL_PROFILE_BEGIN_SESSION("Runtime", "GlimmerProfile-Startup.json");
auto app = gl::CreateApplication();
GL_PROFILE_END_SESSION();

GL_PROFILE_BEGIN_SESSION("Runtime", "GlimmerProfile-Runtime.json");
app->Run();
GL_PROFILE_END_SESSION();

GL_PROFILE_BEGIN_SESSION("Runtime", "GlimmerProfile-Shutdown.json");
delete app;
GL_PROFILE_END_SESSION();
```

![CPU Trace 时间轴](README.assets/image-20260416124421235.png)

当前 `Core.h` 中的 `GL_PROFILE` 是 `0`，因此默认构建不会生成这些文件。需要抓取时把开关设为 `1`，运行一段可复现操作并正常关闭程序，三个 Session 才能写完合法的 JSON 尾部。

这套工具适合本地找 CPU 热点，边界也很清楚。每条记录都会立即刷新文件，采样密度过高时会反过来干扰结果；`Instrumentor` 没有互斥保护，也没有处理会话重入和输出文件打开失败。它记录的是 CPU 作用域，不能替代 `GPUTimer` 对 Shadow、Terrain 等 GPU Pass 的非阻塞查询。我的使用习惯是先用它定位可疑阶段，再用渲染统计或 GPU Timer 验证具体瓶颈，避免只凭一张时间轴下结论。

## Renderer2D升级

有了基础 Quad 绘制后，需求很快多了起来：纹理平铺、Sprite 染色、旋转和编辑器拾取都要接进同一条路径。继续为每种组合复制绘制代码撑不了多久，所以 Renderer2D 开始围绕 transform 和统一顶点格式整理接口。

普通方块可以传位置与尺寸，也可以直接传完整 `transform`。旋转接口在 CPU 端构造 `Translate * Rotate * Scale`，再把单位 Quad 的四个顶点转换到世界空间。`rotation` 使用角度，内部调用 `glm::radians()`；纹理版本另带 `tilingFactor` 和 `tintColor`。

`DrawSprite()` 是场景侧入口。它读取 `SpriteRendererComponent`、实体 ID，以及可选的 Material 和 Overrides。材质存在时，BaseColor、BaseColorTexture 和 TilingFactor 会覆盖组件值，最终仍写入同一种 `QuadVertex`。EntityID 随顶点进入整数附件，编辑器拾取不用额外绘制。

```cpp
Renderer2D::DrawRotatedQuad(
    { 0.0f, 0.0f, 0.0f }, { 1.5f, 1.0f }, 30.0f,
    texture, 2.0f, { 0.8f, 0.4f, 0.4f, 1.0f }
);
```

这次升级里最难查的故障和旋转计算无关。全屏 Shader 改变了当前 VAO，批次 Flush 如果沿用 OpenGL 全局状态，就会拿错顶点布局，屏幕上出现与调用顺序有关的彩色方块。现在 `OpenGLRendererAPI::DrawIndexed()` 会先绑定参数中的 VertexArray，Renderer2D 不再依赖前一次绘制留下的 VAO；旧版绘制后主动解绑纹理的操作也已经移除。

透明像素没有被固化成 Renderer2D 的统一策略。批次会启用 SourceAlpha 混合，但保持现有深度写入状态。Sandbox 的旧 Texture Shader 仍在 Alpha 小于 `0.1` 时执行 `discard`，当前完整编辑器的 Shader 没有固定裁剪。透明 Sprite 的结果仍取决于提交顺序和深度状态。

![Renderer2D 接口升级后的示例](README.assets/image-20260421113429547.png)

`ResetStats()` 和 `GetStats()` 用于查看批次 Draw Call 与 Quad 数量。当前统计主要覆盖 Sprite Batch：全屏接口会增加 QuadCount，却没有增加 DrawCalls，因此不能把它当作完整帧的 GPU 提交总数。

源码里还有两处未收口。带位置和尺寸的纹理 `DrawQuad()` 转发时漏掉了 `tintColor`，需要 Tint 时应暂用 transform 重载；带纹理的 `DrawRotatedQuad()` 也缺少 32 槽容量检查。这些边界在继续扩展 2D API 前需要修正。

## 2D 批处理渲染

Renderer2D 最初每画一个方块就上传 Uniform 并提交 Draw Call。数量一多，CPU 时间便耗在重复绑定和驱动调用上。批处理的做法很直接：先把兼容 Quad 写进连续内存，最后一起交给 GPU。

当前单个批次最多容纳 20,000 个 Quad，也就是 80,000 个顶点和 120,000 个索引。索引拓扑在初始化时一次生成；CPU 端分配同样容量的 `QuadVertex` 数组，动态 VBO 只上传本批次实际使用的部分。

```cpp
struct QuadVertex
{
    glm::vec3 Position;
    glm::vec4 Color;
    glm::vec2 TexCoord;
    float TexIndex;
    float TilingFactor;
    int EntityID;
};
```

一帧的基本流程是：

1. `BeginScene()` 更新摄像机 UBO，`StartBatch()` 重置索引数、写指针和纹理槽。
2. `DrawQuad()` 在 CPU 缓冲区追加四个顶点，并把索引数增加 6。
3. `EndScene()` 计算有效字节数，一次上传动态 VBO。
4. `Flush()` 绑定本批次纹理，以当前索引数调用一次 `DrawIndexed()`。

顶点位置在写入时完成变换。GPU 只使用共享 ViewProjection，不必为每个 Quad 切换 `u_Transform`。这多做了少量 CPU 矩阵运算，却省下大量小型提交，更适合 Sprite 场景。

纹理槽让不同图片也能留在同一批次。槽 0 固定为白贴图，其余从 1 开始。加入纹理时先线性查找，命中就复用 `TexIndex`，未命中才占新槽。普通纹理 Quad 在 32 槽用满时会提交并重开批次，Shader 通过 `u_Textures[32]` 和顶点索引选择采样器。

索引容量耗尽也会调用 `FlushAndReset()`。`Flush()` 在索引数为 0 时必须直接返回，因为底层 `DrawIndexed()` 把 0 解释成使用整个 IndexBuffer；缺少这个保护，空 Sprite 帧会重画动态 VBO 中上一帧的数据。

![多纹理 Quad 合并到同一批次](README.assets/image-20260417190927104.png)

完整编辑器会延迟 Sprite Pass。Scene 先记录待绘制状态，EditorLayer 完成 Opaque、Terrain 和 Skybox 后调用 `Scene::FlushSpritePass()`，这时才执行 Renderer2D 的 Begin、Submit 和 End。Alpha Sprite 因此会与已经存在的 Skybox 颜色混合，自动拆批也不会跑到 Skybox 前面。Sandbox 没有这层编排，仍自行控制 Renderer2D 生命周期。

批处理减少了提交次数，不保证整帧永远只有一个 Draw Call。顶点容量和纹理槽都可能拆批；以后加入多种 2D Shader 或混合状态时，还要定义新的兼容条件。分析统计时应同时看 DrawCalls 和 QuadCount。

## 加载obj文件

最早的 OBJ 加载器是课程作业式的手写解析，能读规整的三角模型，却很难应付四边形、负索引、Shape、MTL 和缺失属性。我也试过把 Assimp 源码直接塞进 Premake，甚至准备手写 `config.h`。报错越来越多后才确认，绕过第三方库自己的 CMake 配置并不省事。

OBJ 最终交给 tinyobjloader。Assimp 后来重新接入，只负责静态 FBX，并通过官方 CMake 独立生成静态库。当前 `ModelImporter` 根据小写扩展名分发 `.obj` 和 `.fbx`；glTF、GLB 与动画模型尚未开放。

真正影响后续架构的是 CPU 中间层。两个 Importer 都只输出 `MeshSource`：

```text
MeshSource
├── SourcePath
├── Submeshes[]
│   ├── Vertices: Position / Normal / Tangent / TexCoord
│   ├── Indices
│   └── MaterialIndex
└── Materials[]
    ├── PBR 因子
    └── BaseColor / Normal / Metallic / Roughness / AO / Emissive 路径
```

`ObjModelImporter` 把 MTL 搜索目录设为 OBJ 所在文件夹，并要求 tinyobjloader 三角化。结果按材质拆成 Submesh，Position、Normal 和 TexCoord 的完整组合用于顶点去重。切线由三角形 UV 梯度计算；UV 退化时会从法线构造稳定正交方向，避免法线贴图路径出现 NaN。

OBJ 材质读取目前比较保守，只保存 MTL 名称和 Diffuse Texture 路径，其余 PBR 通道使用 `MeshMaterialSource` 默认值。FBX 路径会填写更多字段，但两种格式最终共享同一数据结构，Renderer 和 Scene 看不到 tinyobjloader 或 Assimp 类型。

`Model` 接收导入结果后，按材质索引复用纹理，再为有效 Submesh 创建 GPU `Mesh`。Mesh 持有 VAO、VBO、IBO、材质纹理和局部 AABB。Model 已经没有旧稿中的 `Draw()`；Scene 通过 ModelRendererComponent 的 AssetHandle 调用 `Renderer3D::SubmitModel()`，AssetManager 按 Handle 延迟创建并缓存 Model。

![OBJ 模型加载结果](README.assets/image-20260428155345962.png)

无窗口回归会生成一个三角形 OBJ，检查导入器只公开 OBJ/FBX、输出三顶点 Submesh，并验证切线有限且归一化。它覆盖 CPU 导入契约，不创建 OpenGL Context；纹理上传和 Renderer3D 绘制仍需图形环境验证。

Model 首次缓存未命中时仍会解析源文件并创建 GPU 资源，项目还没有版本化的内部 Mesh 烘焙格式。大型模型导入、源文件变化检测和发布打包仍需继续完善。现在至少已经分开格式解析、CPU 中间数据与渲染资源，增加下一种静态格式时不必改写 Model 和 Mesh。

## 3D全局光照

模型第一次能画出来时，颜色基本等于贴图本身，看不出体积。我先后试过 Phong、Toon 和 Blinn-Phong，用 Sandbox 手动上传灯光位置、颜色与相机位置。这些试验帮我把法线矩阵、漫反射和高光接通了，但每个 Layer 都维护一套 Uniform 很快就乱了。

现在灯光由 Scene 统一收集。每帧会取第一个启用的 DirectionalLight、最多 16 个 PointLight，以及第一个拥有有效 Cubemap 的 SkyLight。方向光的方向来自实体 Transform 的局部 `-Z`，点光位置直接使用 Transform Translation。

```text
Scene Components
    -> LightEnvironment
    -> Renderer::UploadLightEnvironment()
    -> binding 1 Light UBO
    -> PBRModel / Terrain Shader
```

Light UBO 使用 `std140`，大小固定为 576 字节。方向、颜色、环境强度和点光数组一次上传，Renderer3D 不再逐模型重复设置这些值。SkyLight 的 Handle 与强度交给 `EnvironmentLighting`，由它绑定 Diffuse Irradiance、Specular Prefilter 和 BRDF LUT。

当前 PBRModel Shader 使用 Cook-Torrance BRDF。方向光和点光进入直接光照，SkyLight 提供环境漫反射与镜面反射；没有有效 IBL 时，环境项回退到方向光颜色和 AmbientIntensity。方向光还可以驱动 1 至 4 级 CSM，阴影采样与灯光 UBO 分开管理。

![早期 3D 光照实验](README.assets/image-20260428163358738.png)

这里仍有明确上限。公共光照只采用第一个启用的方向光，点光最多 16 个且没有阴影，Spot Light 和 Area Light 也尚未实现。阴影系统会寻找第一个启用且允许 CastShadows 的方向光，所以场景放置多个方向光时，应避免让照明来源和投影来源分离。早期 Toon 与 Blinn-Phong Shader 仍可作为效果试验，但它们不代表当前完整编辑器的默认材质路径。

## 为3D对象绑定贴图

最初的实现让每个 Mesh 只保存一张漫反射贴图。OBJ 的 MTL 能显示了，但这种结构很快碰到天花板：法线、AO 和自发光没有位置，实体也无法用共享材质覆盖导入结果。后来纹理职责被拆成导入材质和 `.glmat` 两层。

Model 导入时会按 MaterialIndex 复用 `MeshMaterialTextures`。它可以保存 BaseColor、Normal、Metallic、Roughness、AO 和 Emissive 六个运行时 Texture2D 引用。BaseColor 与 Emissive 按 sRGB 读取，Normal、AO、Metallic 和 Roughness 保持 Linear，避免数据纹理被错误做 Gamma 转换。

渲染提交时，实体 MaterialInstance 的优先级更高。`.glmat` 目前可以显式指定 BaseColor、Normal、AO 和 Emissive Texture；缺少的通道回退到模型导入纹理。Metallic 与 Roughness 仍使用材质标量，并可乘上导入模型自带的独立纹理。全部纹理缺失时绑定白贴图，同时通过 `u_Has*Texture` 告诉 Shader 是否应该采样。

```text
最终 BaseColor Texture = .glmat / Overrides -> 导入纹理 -> 白贴图占位
最终 Normal、AO、Emissive = .glmat / Overrides -> 导入纹理 -> 白贴图占位
最终 Metallic、Roughness = 材质标量 + 导入数据纹理
```

纹理单元已经固定分区：材质主通道占 0 至 3，CSM 使用 4 至 7，IBL 使用 8 至 10，导入 Metallic 与 Roughness 使用 11、12。这个约定看起来有些死板，但能避免不同 Shader 把同一个槽误当成另一种采样器。Renderer3D 还会缓存已绑定纹理，并把纹理 ID 纳入排序和 Instancing 兼容条件。

![3D 模型贴图加载结果](README.assets/image-20260428171507539.png)

导入纹理目前由 Model 直接持有，尚未注册为 AssetHandle，也不会自动生成 `.glmat`。这意味着它们能参与运行时渲染，却不能像正式纹理资产那样独立编辑、重载和序列化。等内部 Mesh 与材质烘焙落地时，这条边界还需要继续收拢。

## 帧缓冲 (Framebuffers)

直接画到默认窗口后，我很快遇到两个实际问题：编辑器需要把场景嵌进 ImGui Viewport，后处理也需要先拿到完整场景颜色。Framebuffer 把场景输出变成可继续采样的离屏资源，渲染流程从这里开始有了 Pass 的概念。

当前接口由 `FramebufferSpecification` 描述尺寸、附件和采样数。可用格式包括 LDR `RGBA8`、HDR `RGBA16F`、用于拾取的 `RED_INTEGER`，以及 `Depth24Stencil8` 和阴影使用的 `Depth32F`。

```cpp
FramebufferSpecification specification;
specification.Width = viewportWidth;
specification.Height = viewportHeight;
specification.Attachments = {
    { FramebufferTextureFormat::RGBA16F },
    { FramebufferTextureFormat::RED_INTEGER },
    { FramebufferTextureFormat::Depth24Stencil8 }
};
auto framebuffer = Framebuffer::Create(specification);
```

完整编辑器的 Scene FBO 使用上面这组三附件。每帧开始前，EntityID 附件清为 `-1`；模型、地形和 Sprite 写入自己的实体 ID，鼠标坐标转换到 Framebuffer 空间后再通过 `ReadPixel(1, x, y)` 完成拾取。HDR Color 与可采样 Depth 随后交给 PostProcessRenderer，经过 Bloom、雾和 Tone Mapping 后，Display FBO 的颜色纹理才显示到 Viewport。

OpenGL 实现使用 DSA 创建附件。`Bind()` 同时切换 FBO 和 Viewport，`Resize()` 拒绝零尺寸及超过 8192 的尺寸，并在大小未变化时直接返回。普通颜色附件会原地重新分配，纹理 ID 保持稳定；深度附件会重新创建，因此缓存其 RendererID 的代码必须在 Resize 后重新获取。

Depth32F 使用 ClampToBorder 和白色边界，专门供 ShadowRenderer 的深度图采样。只有深度附件的 FBO 会关闭颜色读写目标。`ClearAttachment()` 和 `ReadPixel()` 当前按整数附件实现，调用方应只把它们用于 `RED_INTEGER`。

![Framebuffer 输出到编辑器 Viewport](README.assets/image-20260506180243041.png)

Framebuffer 抽象目前只有 OpenGL 后端。`SwapChainTarget` 仍未实现；Samples 大于 1 的分支使用 Renderbuffer，也没有完整的多颜色附件和 Resolve 方案，不能当作已经可用的编辑器 MSAA。现阶段生产路径保持 Samples 为 1，抗锯齿需要在后续单独设计解析 Pass。

## 建立新项目

Sandbox 能跑起来以后，我开始关心另一件事：引擎是否真的能被第二个程序使用。新建 Editor 项目的意义就在这里。它迫使 Application、入口点、Layer 和客户端资源从 Sandbox 的试验代码里分离出来，也暴露了不少依赖路径上的偷懒。

现在每个客户端都维护自己的 `premake5.lua`，根工作区只负责统一配置并通过 `include` 把项目收进解决方案。以仓库中的 `GlimmerEditor-CyouBranch` 为例，客户端使用 C++17、静态运行库，链接 `Glimmer`，源码和 Windows 资源文件则由自己管理。

```lua
project "MyApp"
    location "."
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files { "src/**.h", "src/**.cpp" }
    includedirs {
        "../Glimmer/src",
        "../" .. IncludeDir["spdlog"],
        "../" .. IncludeDir["ImGui"],
        "../" .. IncludeDir["glm"],
        "../" .. IncludeDir["entt"]
    }
    links { "Glimmer" }
```

项目脚本完成后，还要在根 `premake5.lua` 中加入 `include "MyApp"`。根工作区当前的 `startproject` 仍是 Sandbox；如果希望生成解决方案后直接启动新程序，需要同步修改它，或者在 Visual Studio 中手动设置启动项目。

客户端入口很薄。它继承 `Application`，压入自己的 Layer，并实现引擎约定的 `CreateApplication()`。`EntryPoint.h` 会提供真正的 `main`，所以它只能出现在这个入口翻译单元中。

```cpp
#include <Glimmer.h>
#include "Glimmer/Core/EntryPoint.h"
#include "EditorLayer.h"

class MyApp final : public gl::Application
{
public:
    MyApp() : Application("My App")
    {
        PushLayer(new EditorLayer());
    }
};

gl::Application* gl::CreateApplication()
{
    return new MyApp();
}
```

![独立 Editor 应用的早期窗口](README.assets/image-20260507090436321.png)

这一步最容易踩坑的是自行升级语言标准或随手改依赖版本。仓库目前固定 EnTT `v3.16.0`，根配置的包含目录是 `Glimmer/vendor/entt/src`，客户端继续使用 C++17。早期试过跟随 EnTT 开发分支并切到 C++20，结果撞上 MSVC concepts 和 tinyobjloader 内部 fast_float 的兼容问题，最后还是回到稳定标签与 C++17。新项目最好先复制现有客户端的配置，再删掉用不到的依赖。

## ECS

早期 Sandbox 把企鹅、方块和相机都保存成 Layer 成员。对象一多，更新、绘制和面板代码便开始互相缠绕。接入 EnTT 后，Scene 成了数据的所有者，渲染器只消费带有目标组件的实体。这个改动后来也给场景保存、Edit/Play 隔离和 Undo/Redo 留出了位置。

`Scene` 持有 `entt::registry`，`Entity` 只是 EnTT Handle 与所属 Scene 的轻量包装。创建实体时会自动添加 `IDComponent`、`TagComponent` 和 `TransformComponent`。其中 EnTT Handle 只适合当前 Registry 内的临时访问；UUID 才用于序列化、复制和跨重载查找，Scene 为此维护了一张 `UUID -> entt::entity` 索引。

```cpp
gl::Entity entity = scene->CreateEntity("Crate");
entity.GetComponent<gl::TransformComponent>().Translation = { 0.0f, 1.0f, 0.0f };
entity.AddComponent<gl::ModelRendererComponent>(modelHandle);
entity.AddComponent<gl::MaterialComponent>(materialHandle);

gl::UUID stableID = entity.GetUUID();
gl::Entity sameEntity = scene->FindEntityByUUID(stableID);
```

当前组件已经超过最初的 Tag、矩阵 Transform 和纯色 Sprite。Transform 分开保存 Translation、欧拉 Rotation 与 Scale；渲染侧有 Sprite、Model、Material 和 Terrain；环境侧有方向光、点光与 SkyLight；Camera 和 NativeScript 则负责运行期行为。大多数组件仍是可复制的数据结构，不过 Terrain 持有非持久化 Runtime，NativeScript 也含有运行时实例与工厂函数，复制时必须按各自规则处理。

```text
Scene
  -> EnTT Registry
      -> ID + Tag + Transform
      -> Renderer / Light / Camera / Script Components
  -> UUID Index
  -> Editor Update 或 Runtime Update
```

编辑模式调用 `OnUpdateEditor()`，相机由 EditorLayer 传入；运行模式调用 `OnUpdateRuntime()`，Scene 自己寻找 Primary Camera。两条路径都会上传灯光，并按组件 View 提交 Model、Terrain 和 Sprite。这样，面板只修改组件，Scene 决定本帧有哪些对象参与渲染，Renderer 不需要知道实体是从层级面板、脚本还是反序列化创建的。

`Scene::Copy()` 会保留 UUID，并复制当前支持的组件，用于进入 Play 时建立独立 Runtime Scene。复制 Terrain 时只保留 Specification，GPU Runtime 会重新生成；NativeScript 只复制构造与销毁函数，脚本实例不会跨 Scene 共用。`DuplicateEntity()` 则创建新 UUID，并复制可编辑组件。两者语义不同，后续实现复制命令时不能混用。

![ECS 接入后的早期组件调试](README.assets/image-20260507160731040.png)

EnTT 目前固定在提交 `b4e58bdd3`，对应 `v3.16.0`。这不是随意留下的旧版本。开发分支曾要求 C++20，并在当时的 MSVC 环境里触发 meta/concepts 解析问题；改语言标准又会碰到 fast_float 的 constexpr 兼容错误。稳定标签配合 C++17 是当前已经验证过的组合，升级时需要把整个依赖链一起回归。

## 相机组件

相机进入 ECS 后，投影参数和空间位置终于分开了。`Camera` 基类只保存 Projection；`SceneCamera` 负责 Perspective 与 Orthographic 两套参数；实体上的 `TransformComponent` 提供世界变换。运行时取相机 Transform 的逆矩阵作为 View，再与 Projection 相乘。

```text
CameraComponent.Camera.GetProjection()
    x inverse(TransformComponent.GetTransform())
    = ViewProjection
```

`CameraComponent` 还保存 `Primary` 和 `FixedAspectRatio`。Scene 会采用第一个 Primary Camera，因此一个场景最好只保留一个主相机。视口尺寸变化时，`OnViewportResize()` 会更新所有未锁定宽高比的 SceneCamera；如果组件在 Viewport 已建立后才添加，`OnComponentAdded<CameraComponent>()` 会立即补一次尺寸同步。

```cpp
auto cameraEntity = scene->CreateEntity("Main Camera");
auto& camera = cameraEntity.AddComponent<gl::CameraComponent>();
camera.Primary = true;
camera.FixedAspectRatio = false;
camera.Camera.SetPerspective(glm::radians(45.0f), 0.01f, 1000.0f);
```

SceneCamera 的 Setter 会立即重算投影矩阵。Perspective 保存垂直 FOV、Near 和 Far，Orthographic 保存 Size、Near 和 Far。这里的透视 FOV API 接收弧度；Inspector 展示角度时需要做一次转换，不能把界面上的 45 直接传进底层。

早期实现曾让 `OrthographicCameraController` 和 ECS Camera 各画一部分对象，共用同一个 Framebuffer。画面能出来，但一个 Viewport 同时存在两套观察坐标，调试起来相当别扭。

![两套相机并存时的早期验证画面](README.assets/image-20260507193832578.png)

现在编辑器和游戏相机按模式分工。Edit 模式使用 `EditorCamera`，支持轨道观察、聚焦和视口输入；进入 Play 后先通过 `Scene::Copy()` 创建 Runtime Scene，再由其中的 Primary Camera 驱动阴影、3D 模型、Terrain 和 Sprite。CameraComponent 会随场景 YAML 保存，NativeScript 运行时实例则留在运行阶段。这条分界让编辑器观察位置不会误写进游戏相机，也让停止播放后能够干净地回到编辑场景。

## 原生脚本系统

ECS 接通后，我需要一种最小成本的办法验证实体能否自己更新。原生脚本系统就是这层薄桥：脚本写成 C++ 类，通过 `NativeScriptComponent` 挂到实体上，Scene 在运行模式里负责创建、更新和销毁实例。它够用来测试生命周期和组件访问，但离可编辑、可热重载的正式脚本方案还有一段距离。

`ScriptableEntity` 保存所属 Entity，并提供 `GetComponent<T>()`。派生类只需要覆盖 `OnCreate()`、`OnUpdate()` 或 `OnDestroy()`。脚本实例并不会在添加组件时立刻创建；第一次进入 `Scene::OnUpdateRuntime()` 时，Scene 才调用工厂函数，注入 Entity，执行一次 `OnCreate()`，随后每帧执行 `OnUpdate()`。

```cpp
class CameraController final : public gl::ScriptableEntity
{
protected:
    void OnUpdate(gl::Timestep ts) override
    {
        auto& transform = GetComponent<gl::TransformComponent>();
        if (gl::Input::IsKeyPressed(GL_KEY_W))
            transform.Translation.z -= 2.0f * static_cast<float>(ts);
    }
};

cameraEntity
    .AddComponent<gl::NativeScriptComponent>()
    .Bind<CameraController>();
```

`Bind<T>()` 用两个无捕获 Lambda 填入构造与销毁函数指针，组件本身只保存基类指针，不需要知道具体脚本类型。退出 Play 时，`OnRuntimeStop()` 会依次调用 `OnDestroy()` 并释放实例；直接销毁实体也走同样的清理路径。`Scene::Copy()` 只复制这两个函数指针，不复制正在运行的脚本对象，因此 Runtime Scene 拥有自己的实例。

![原生脚本控制相机的早期验证](README.assets/image-20260508173136130.png)

当前边界需要说清楚。NativeScriptComponent 不写入场景 YAML，因为函数指针无法跨进程持久化；完整编辑器也没有按类名选择和重新绑定脚本的资产系统。旧 `GlimmerEditor` 里保留了 CameraController 示例。当前完整编辑器保留 NativeScript 的 Scene 复制和销毁逻辑，但默认场景没有绑定具体脚本。脚本异常隔离、动态模块重载和反射都还没有实现。

组件移除也有一个容易忽略的限制：运行中的脚本清理由 `OnRuntimeStop()` 和 `DestroyEntity()` 承担，通用 `RemoveComponent<NativeScriptComponent>()` 没有专门的销毁钩子。运行时不要直接移除一个已经实例化的脚本组件，否则 `OnDestroy()` 不会被调用，实例也无法正常回收。

## 代码审查+RenderDoc

第一次认真用 RenderDoc 抓帧，是为了追一个很荒唐的现象：代码里明明没有提交方块，画面上却留着一个巨大的白色 Quad。单看 CPU 调用很难解释，抓帧后却能直接看到一次包含 120000 个索引的 Draw。这个数字正好等于当时 Renderer2D 预生成的完整索引缓冲，问题一下缩小到空批次提交。

旧实现把 `indexCount = 0` 传给底层 `DrawIndexed()`，而这个接口把 0 解释为使用完整索引缓冲。上一帧 VBO 中残留的数据因此又被画了一遍。现在 `Renderer2D::Flush()` 会在索引数为零时直接返回，不绑定纹理，也不增加 DrawCall 统计。保留这段记录很有用，它提醒我：封装接口里的特殊值语义，迟早会在另一层变成真实 Bug。

![RenderDoc 中定位到异常索引绘制](README.assets/image-20260510184039610.png)

RenderDoc 没有嵌入 Glimmer，它仍是外部抓帧工具。分析完整编辑器时，应选择当前配置下的 `GlimmerEditor-CyouBranch.exe`，并把 Working Directory 设为 `D:\Glimmer\GlimmerEditor-CyouBranch`。编辑器以相对路径调用 `AssetManager::Initialize("assets")`，工作目录不对时，Shader 和资产会先于渲染问题报错。

我现在通常按下面的顺序看一帧：

```text
Event Browser
  -> 找到目标 Pass 与 Draw Call
Pipeline State
  -> 核对 Program、VAO、Depth、Blend、Cull 与 Framebuffer
Mesh Viewer
  -> 对比 VS Input 和 VS Output
Texture Viewer
  -> 检查采样器槽位、纹理类型和实际内容
```

这个顺序后来又抓到过两类问题。一次是 Sprite 在 Skybox 前绘制，透明像素先与 Clear Color 混合；当前完整编辑器已经把 Sprite Pass 延后到 Skybox 之后。另一次是 Tone Mapping Program 的 `sampler2D` 与 `samplerCube` 默认落在同一槽位，严格驱动直接报 `GL_INVALID_OPERATION`；现在 PostProcessRenderer 每帧都会声明完整的 0 至 3 号采样器绑定。

![RenderDoc 中检查纹理与管线状态](README.assets/image-20260510184048818.png)

代码审查时还顺手补了 `ShaderLibrary::Remove()`。Library 内部保存的是 `Ref<Shader>`，`erase` 只释放 Library 自己持有的那份引用；如果 Renderer 或 Layer 仍持有同一个 Ref，Shader 对象和 OpenGL Program 会继续存在，直到最后一份引用销毁。当前 Library 还提供 `ReloadChanged()` 与 `ReloadAll()`，热重载成功后才替换旧 Program，编译失败会保留上一份可用对象。

目前抓帧里看到的仍是原始 OpenGL 调用，Glimmer 没有接入 RenderDoc API，也没有为 Pass 添加 GPU Debug Group 或对象标签。复杂帧需要靠 Framebuffer、Shader 和调用顺序人工辨认；等 Pass 数继续增长，这会是值得补上的调试基础设施。

## 透视相机

正交相机很适合早期 2D 测试，但模型开始有前后距离后，所有物体看起来都像贴在同一张纸上。透视投影加入以后，近处变大、远处缩小，3D 场景终于有了正常的空间感。需要同步管理的参数包括垂直 FOV、Aspect Ratio、Near 和 Far。只替换投影函数，深度精度和视口比例很快就会出问题。

```cpp
projection = glm::perspective(
    glm::radians(verticalFOVDegrees),
    viewportWidth / viewportHeight,
    nearClip,
    farClip);
```

当前有两类透视相机。`SceneCamera` 属于 CameraComponent，可在 Perspective 与 Orthographic 之间切换，并随场景保存；它的 `SetPerspective()` 接收弧度。`EditorCamera` 是编辑器自己的观察相机，构造参数中的 FOV 使用角度，内部计算投影时再调用 `glm::radians()`。两套 API 的单位不同，调用时混淆会得到一个几乎无法使用的视锥。

EditorCamera 围绕 Focal Point 和 Distance 计算位置，再用 `glm::lookAt()` 生成 View。右键拖动旋转，右键配合 WASD/QE 移动，中键平移，滚轮改变观察距离；Pitch 被限制在 `-89` 到 `89` 度，Distance 限制在 `0.5` 到 `500`。只有 Viewport Hover 时 EditorLayer 才启用输入，避免操作面板时相机跟着跑。

Near/Far 也会影响后续系统。Scene 把它们传给 CSM 计算级联范围，Depth 又参与世界位置重建与距离雾。Near 设得过小、Far 设得过大，会把有限的深度精度浪费在很长的区间里。当前仍使用普通深度投影，没有 Reverse-Z；编辑大型地形时应先按实际可见范围调整裁剪面，而不是一味增大 Far。

## 场景层级面板 (Scene Hierarchy Panel)

场景里只有几个测试实体时，靠代码记住它们还勉强说得过去。模型、灯光和地形陆续加入后，我需要一个能直接看见 Scene 内容的入口。SceneHierarchyPanel 最初就是这样做出来的：遍历 Registry，把每个带 Tag 的实体画成一行，并把选中结果交给编辑器其它面板。

现在它的职责很窄。面板负责创建、枚举、选中、复制和删除实体；属性编辑已经交给 InspectorPanel。两者共享 `SelectionContext`，因此从 Hierarchy 选择实体会清掉资产选择，从 Content Browser 选择资产也会清掉实体选择。

```text
Scene
  -> SceneHierarchyPanel -> SelectEntity(Entity)
                              |
ContentBrowserPanel -> SelectAsset(AssetHandle)
                              |
                       SelectionContext
                              |
                       InspectorPanel
```

列表项会在名称后附加组件缩写，例如 `[Cam]`、`[Model]`、`[Terrain]`、`[Sun]` 和 `[Scr]`。它们只是快速提示，不参与组件查询或渲染。当前列表仍通过 Scene 的 friend 权限直接遍历 `m_Registry`，所以早期文档所说的 "只依赖公共接口" 并不完全准确；如果以后要让其它工具复用实体枚举，Scene 还需要补一个正式的遍历接口。

右键菜单提供 Duplicate 和 Delete。删除会先弹确认框，操作完成后清理选中项。创建、复制和删除在 Edit 模式下都会记录到 `EditorCommandHistory`，`EntitySnapshot` 依靠 UUID 恢复实体及可复制组件。这样 Undo 删除时恢复的是原实体身份，而不是随手创建一个外观相同的新对象。

```cpp
m_HierarchyPanel.SetContext(m_ActiveScene);
m_HierarchyPanel.SetSelectionContext(&m_SelectionContext);
m_HierarchyPanel.SetCommandHistory(&m_CommandHistory);
```

进入 Play 后，Hierarchy 会切到 Runtime Scene，并通过同一 UUID 尽量保留当前选择；命令历史在运行模式中断开，修改只作用于副本。临时性能场景甚至会直接关闭实体枚举，避免几千个测试实体把面板和压力测试本身一起拖慢。

![早期场景实体列表](README.assets/Pasted%20image%2020260716151430.png)

名称里虽然有 Hierarchy，目前的数据仍是平面列表。Transform 没有 Parent/Children 关系，面板也没有折叠树、拖拽重设父级、多选和搜索。这个命名保留了编辑器的发展方向，却不应让人误以为场景图已经实现。

## ImGUI自定义风格

默认 ImGui 很适合调试，却和编辑器窗口放在一起时显得过于紧凑。早期我先从间距、圆角和字体入手，没有单独做主题系统。样式现在仍集中在 `ImGuiLayer::OnAttach()`，所有使用引擎 ImGuiLayer 的客户端都会继承同一套配置。

初始化时启用键盘导航、Docking 和 Multi-Viewport。Multi-Viewport 开启后使用 Light 配色，整体尺寸放大 1.2 倍，窗口、Popup 和控件分别设置圆角与边框。EditorLayer 的全屏 DockSpace 和 Viewport 会局部覆盖 Padding、Rounding 等值，避免圆角与空白侵占实际渲染区域。

```cpp
ImGuiIO& io = ImGui::GetIO();
io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

ImGui::StyleColorsLight();
ImGuiStyle& style = ImGui::GetStyle();
style.ScaleAllSizes(1.2f);
style.WindowRounding = 16.0f;
style.FrameRounding = 16.0f;
```

![浅色圆角风格的早期效果](README.assets/Pasted%20image%2020260717133307.png)

文字字体使用 `OpenSans_SemiCondensed-Italic.ttf` 的 20 像素字号，随后以 MergeMode 合并 Font Awesome 6 的 `0xf000` 至 `0xf2ff` 图标区间。字体和普通资产一样依赖工作目录下的 `assets/fonts`。如果启动目录错误，界面会先出现字体或资产问题，排查渲染前应先确认 Working Directory。

![字体与面板间距调整后的界面](README.assets/Pasted%20image%2020260717141005.png)

Multi-Viewport 渲染结束后，ImGui 会切换多个 GLFW Context。`ImGuiLayer::End()` 会保存当前 Context，执行 Platform Windows 更新和绘制，再恢复原 Context，避免下一帧 OpenGL 命令落到错误窗口。事件阻断则只看 ImGui 的 `WantCaptureMouse` 与 `WantCaptureKeyboard`，不重复向后端注入输入。

这套样式目前是写死的：没有深浅主题切换，没有 DPI 感知缩放，也没有配置文件。`ScaleAllSizes(1.2f)` 只解决了当前开发设备上的密度问题。后续若认真处理多显示器 DPI，尺寸应从平台缩放计算，而不是继续叠加常量。

## 场景层级面板完善：内联组件检查器

第一版把组件属性直接画在 Hierarchy 列表下方。实现很快，实体一多却很难用：左边既要浏览场景，又要容纳 Terrain、Material 和 Camera 的长表单。后来我把这部分拆成独立的 InspectorPanel，Hierarchy 只保留实体列表，两者通过 SelectionContext 相连。

Inspector 会根据选择类型切换内容。Entity Selection 调用 `DrawComponents()`，Asset Selection 进入资产检查器；没有选择时只显示提示。这个统一入口解决了早期 Properties 面板与 Content Browser 各自维护选择状态的问题。

```cpp
if (selection.IsEntitySelected())
    DrawComponents(selection.GetEntity());
else if (selection.IsAssetSelected())
    DrawAssetInspector(selection.GetAsset());
```

实体检查器目前覆盖 Transform、Terrain、三类灯光、Camera、Model Renderer、Sprite Renderer 和 Material。组件标题的右键菜单支持 Reset 与 Remove，底部的 Add Component 菜单只列出实体尚未拥有的类型。Transform 不允许移除；Tag 固定显示在顶部，也不走通用组件折叠框。

Camera Inspector 是这次迭代最先打通的部分。为了让 UI 能独立修改投影参数，SceneCamera 补齐了 Perspective/Orthographic 的 FOV、Size 与 Near/Far Getter/Setter，每个 Setter 都立即重算 Projection。Inspector 中的 FOV 用角度显示，写回时转换为弧度，避免 UI 单位泄漏到 SceneCamera API。

![早期内联组件检查器](README.assets/Pasted%20image%2020260717153228.png)

现在连续拖动不会为每一帧都创建一条命令。`EditorValueTransaction` 在控件激活时保存旧值，释放控件后把整段操作压成一次 Undo。Transform、Terrain、灯光、Camera 和 Material 已接入这条路径；组件添加、移除和重置也通过 CommandHistory 执行。Play 模式和临时调试场景会断开 CommandHistory，防止运行时修改污染编辑记录。

边界还没有完全收口。Tag、SpriteRenderer 和 ModelRenderer 的部分字段仍直接改组件，尚未统一进入 Undo；TerrainMaterial 资产可以编辑、保存和重载，但资产字段也还没接入 CommandHistory。类职责虽然已经拆成 SceneHierarchyPanel 与 InspectorPanel，`DrawComponents()` 的大段实现目前仍放在 `SceneHierarchyPanel.cpp`。功能上没有冲突，源码位置却已经不符合类边界，后续应单独搬回 `InspectorPanel.cpp`。

## 修复：无贴图 3D 模型全黑 Bug

这个问题最初很迷惑：只要把 2D 批处理注释掉，Bunny、Dragon 和 Suzanne 这类没有漫反射贴图的 OBJ 就会变黑。模型、法线和灯光都还在，唯一变化只是少画了一层 2D。RenderDoc 最后把原因指向了纹理状态，而不是光照公式。

当时的 `OpenGLRendererAPI::DrawIndexed()` 会在绘制后执行 `glBindTexture(GL_TEXTURE_2D, 0)`。这条命令只影响当前 Active Texture Slot。2D 批处理存在时，最后活跃的往往是其它槽位，slot 0 的白贴图碰巧没被解绑；移除 2D Draw 后，slot 0 成了最后活跃槽，无贴图模型下一帧采样到空纹理，颜色自然全部乘成零。

最早的补丁是在 EditorLayer 里每帧重新绑定一张白贴图。画面恢复了，但 Renderer 的正确性依赖宿主按特定顺序补状态，这个修法留不住。当前实现把责任收回渲染器：低层 `DrawIndexed()` 不再擅自解绑纹理，Renderer3D 在初始化时创建自己的 `1x1 RGBA` 白贴图，并为每个缺失材质通道提供有效占位。

```text
材质 Texture Handle
  -> 模型导入纹理
  -> Renderer3D 白贴图占位

u_Has*Texture
  -> 0：Shader 使用材质常量或几何默认值
  -> 1：Shader 才真正采样对应纹理
```

占位纹理和 `u_Has*Texture` 必须同时存在。白贴图保证所有采样器槽都有合法 Texture2D，存在标记则区分 "真实白色贴图" 和 "没有贴图"。BaseColor 缺失时 Shader 使用 `u_BaseColor`，Normal 回退几何法线，AO 回退 1，Emissive 纹理回退白色后仍受 EmissiveStrength 控制。Metallic 与 Roughness 也采用相同的存在标记。

现在无贴图模型可以正常渲染，但无材质模型仍是另一回事。`Renderer3D::SubmitModel()` 要求 Model 和 Material 都能从 AssetManager 解析；MaterialHandle 为 0 或失效时会增加 SkippedModels 并跳过提交。白贴图解决的是材质通道缺失，不能代替整个 Material 资产。

## 场景序列化 (Scene Serialization)

编辑器能创建实体以后，下一个问题很直接：关掉程序，场景就全没了。我选择 YAML 是因为早期格式还在频繁变化，人能直接打开文件看出哪个组件或 Handle 写错，比一开始就做二进制资产省事得多。

`SceneSerializer` 使用 yaml-cpp，把 Scene 写成 `.glimmer` 文件。yaml-cpp 作为独立静态库参与 Premake 构建，Windows 下引擎与依赖项目都定义 `YAML_CPP_STATIC_DEFINE`，避免头文件把符号声明成 DLL Import。

当前场景格式版本是 6。根节点保存场景标识、版本和实体序列；每个实体写入稳定 UUID，再按实际拥有的组件输出字段。

```yaml
Scene: Untitled
Version: 6
Entities:
  - Entity: 13784169322866849271
    Components:
      TagComponent: Main Camera
      TransformComponent:
        Translation: [0, 2, 5]
        Rotation: [0, 0, 0]
        Scale: [1, 1, 1]
      CameraComponent:
        Primary: true
        ProjectionType: 0
```

目前参与往返的组件包括 Tag、Transform、SpriteRenderer、ModelRenderer、Material、Terrain、DirectionalLight、PointLight、SkyLight 和 Camera。资源引用统一保存 AssetHandle，不保存文件绝对路径、运行时指针或 OpenGL ID。Material 会保存共享材质 Handle 和实体 Overrides；Camera 同时保存两种投影的参数，切换模式后原参数不会丢失。

Terrain 只写 `TerrainSpecification`，包括生成参数、Authoring Erosion、Shader Handle、高度图和 TerrainMaterialHandle。Height、派生纹理、水文与气候状态都属于 Runtime，加载后按需重建。NativeScriptComponent 仍不序列化，因为组件里只有函数指针，没有可持久化的脚本类型名。

```text
.glimmer
  -> UUID + Components + AssetHandles
  -> SceneSerializer::Deserialize()
  -> CreateEntityWithUUID()
  -> Runtime 资源按需重建
```

反序列化在新 Scene 中进行，成功后 EditorLayer 才替换当前编辑场景，解析失败不会先清空原场景。Version 1 文件没有稳定实体 ID，加载时会生成新 UUID；Version 2 及以后恢复文件中的 UUID。后续字段主要靠 "存在则读取、缺失则保留结构默认值" 兼容，当前还没有独立的逐版本迁移器。

New、Save As 和 Open 同时出现在 File 菜单与 Ctrl+N/Ctrl+S/Ctrl+O 快捷键中。保存临时 Debug Scene 会被阻止；Play 期间保存的仍是 `m_EditorScene`，不会把 Runtime Scene 改动写回磁盘。打开成功后，Hierarchy、Inspector 和选择上下文都会切到新 Scene。

无窗口回归会把场景写入临时目录，再检查固定 UUID、资产 Handle、Material Overrides 和 Terrain Specification 是否完整恢复，同时确认 Terrain Runtime 没有被持久化。这里还有两个明确缺口：`Serialize()` 返回 `void`，没有把文件打开或写入失败反馈给编辑器；保存也直接覆盖目标文件，尚未采用临时文件替换。场景根节点仍固定写 `Untitled`，编辑器也没有记录当前文件路径和 Dirty 状态，所以 Ctrl+S 实际上每次都是 Save As。

## 原生文件对话框 (Windows File Dialog)

固定写 `assets/scenes/demo.glimmer` 只适合最早的往返测试。接入系统文件对话框以后，编辑器才真正能选择不同场景。公共接口放在 `Glimmer/Utils/FileDialog.h`，调用方只处理字符串路径；Windows 细节留在 `Platform/Windows/WindowsFileDialog.cpp`。

```cpp
std::string path = gl::FileDialog::OpenFile(
    "Glimmer Scene (*.glimmer)\0*.glimmer\0"
    "All Files (*.*)\0*.*\0");

if (!path.empty())
{
    auto scene = gl::CreateRef<gl::Scene>();
    if (gl::SceneSerializer(scene).Deserialize(path))
        SetEditorScene(scene);
}
```

实现使用 Win32 `OPENFILENAMEA`、`GetOpenFileNameA()` 和 `GetSaveFileNameA()`。GLFW 原生窗口通过 `glfwGetWin32Window()` 转成 HWND，作为对话框 Owner；这样系统窗口会正确模态化，不会躲到编辑器后面。`OFN_NOCHANGEDIR` 很重要，打开对话框后工作目录不会被系统悄悄改变，后续相对资产路径仍指向原来的 `assets`。

过滤器是双 Null 结尾的 Win32 字符串，Save 默认补 `.glimmer` 扩展名。用户取消时接口返回空字符串，编辑器直接结束本次操作。全局快捷键在 DockSpace 绘制前通过 `ImGui::IsKeyChordPressed()` 检查，不依赖 File 菜单当前是否获得焦点。

![Windows 原生场景文件对话框](README.assets/Pasted%20image%2020260717163457.png)

这层封装目前只有 Windows 实现，而且仍调用 ANSI 版本 API，结果缓冲区固定为 `MAX_PATH`。包含 Unicode 字符或超过传统路径长度的场景路径不可靠。Open 没有设置 `OFN_FILEMUSTEXIST`，Save 也没有设置 `OFN_OVERWRITEPROMPT`；取消和系统错误都会折叠成同一个空字符串。以后若要把它当成正式项目文件入口，应改用宽字符接口或现代 `IFileDialog`，同时返回可区分的错误状态。

## 视口 Gizmos (ImGuizmo 集成)

层级面板能选中实体，Inspector 也能改 Transform，但只靠输入框调整位置和角度，搭场景时还是有些绕。于是这一阶段把 ImGuizmo 接进视口，让移动、旋转和缩放都能直接在画面里完成。

### 接入位置

ImGuizmo 跟随 ImGui 的帧生命周期更新。`ImGuiLayer::Begin()` 在 `ImGui::NewFrame()` 之后调用 `ImGuizmo::BeginFrame()`，编辑器则在视口窗口内设置绘制区域：

```cpp
ImGuizmo::SetDrawlist();
ImGuizmo::SetRect(
    m_ViewportBounds[0].x,
    m_ViewportBounds[0].y,
    m_ViewportBounds[1].x - m_ViewportBounds[0].x,
    m_ViewportBounds[1].y - m_ViewportBounds[0].y);
```

这里必须使用视口在屏幕中的真实边界。若直接把整个 ImGui 窗口交给 ImGuizmo，标题栏和面板边距会把手柄推离实体，鼠标命中也会跟着错位。

当前 Gizmo 只在 Edit 模式显示，并且要求已有选中实体和 `TransformComponent`。计算时使用 `EditorCamera` 的 View、Projection 矩阵，因此它始终对应编辑器里正在观察的画面，不依赖场景中的主相机。

```text
EditorCamera View / Projection
             +
视口屏幕坐标 + 选中实体 Transform
             |
             v
      ImGuizmo::Manipulate()
             |
             v
   写回 Translation / Rotation / Scale
```

### 操作方式

视口悬停时可以用数字键切换工具：

- `1`：移动
- `2`：旋转
- `3`：缩放

三种工具目前都工作在 `LOCAL` 空间。按住左侧 `Ctrl` 会启用吸附，移动和缩放的步长是 `0.5`，旋转步长是 `45°`。鼠标位于手柄上时，拾取逻辑会检查 `ImGuizmo::IsOver()`，避免拖动 Gizmo 的第一下又把下方实体重新选中。

![视口中的移动 Gizmo](README.assets/Pasted%20image%2020260720104837.png)

选中带有 `CameraComponent` 的实体时，视口还会绘制一组黄色视锥线。它先用该相机的 View-Projection 逆矩阵还原八个裁剪空间角点，再通过 `EditorCamera` 投影到当前视口。这样调整场景相机时，可以同时看到它实际覆盖的范围。

### Transform 写回与留下的问题

ImGuizmo 返回的是完整变换矩阵，编辑器通过 `DecomposeMatrixToComponents()` 拆出位移、欧拉角和缩放。位移、缩放直接覆盖原值；旋转目前写成 `旧值 += 新值 - 旧值`，结果与直接赋值相同。

Transform 在生成矩阵时会用四元数组合 X、Y、Z 旋转，这让矩阵构造顺序更清楚，但组件里保存的仍是欧拉角，Gizmo 分解也会回到欧拉角。因此跨越角度边界时仍可能出现数值跳变，不能把这段处理理解成已经消除了万向节锁。

还有一处更实际的缺口：Inspector 的连续拖动已经接入 `CommandHistory`，Gizmo 拖拽目前仍然直接修改 Transform。也就是说，用手柄完成的变换还不能通过 `Ctrl+Z` 还原。后续应在 `ImGuizmo::IsUsing()` 的开始和结束阶段保存前后快照，把一次连续拖拽合并成一条命令。

## EditorCamera 编辑器自由相机

把 Gizmo 放进视口后，很快就遇到另一个问题：如果编辑场景也依赖场景里的主相机，移动观察位置就会同时改动游戏镜头。编辑器需要一台只服务于创作过程的相机，`EditorCamera` 因此单独放在渲染模块中，不作为 ECS 组件保存。

### 相机状态怎么组织

`EditorCamera` 保存焦点 `FocalPoint`、观察距离 `Distance`、俯仰角 `Pitch` 和偏航角 `Yaw`。位置由这些状态推导，朝向通过四元数计算，最终用 `glm::lookAt()` 生成 View 矩阵。

```cpp
glm::vec3 EditorCamera::CalculatePosition() const
{
    return m_FocalPoint - GetForwardDirection() * m_Distance;
}
```

这种组织方式很适合编辑器视角。旋转时镜头绕焦点运动，平移时移动焦点，滚轮则改变镜头与焦点之间的距离。默认投影参数为 `45°` 视野角、`0.1` 近裁剪面和 `1000.0` 远裁剪面；有效视口尺寸变化后会重新计算宽高比和投影矩阵。

### 当前操作

视口悬停并处于 Edit 模式时，相机才接收输入：

- 按住鼠标右键拖动：旋转视角，俯仰角限制在 `-89°` 到 `89°`
- 按住鼠标右键并使用 `W/A/S/D/Q/E`：前后、左右、上下移动
- 按住任一 `Shift`：移动速度提高到三倍
- 按住鼠标中键拖动：沿相机的右方向和上方向平移焦点
- 滚动滚轮：拉近或拉远，距离限制在 `0.5` 到 `500.0`

中键平移速度会随观察距离调整，远看大场景时不会挪得太慢，靠近物体后也不至于一步跨过去。右键旋转和中键平移分别记录上一帧鼠标位置，切换操作时不会共用一份残留增量。视口失去输入权时，这两组坐标会重置到当前鼠标位置，重新进入视口也就不会突然跳一下。

![EditorCamera 自由观察场景](README.assets/Pasted%20image%2020260720114008.png)

### Edit 与 Play 使用不同相机

编辑器每帧先根据视口悬停状态决定是否启用输入，再更新 `EditorCamera`。渲染 Edit 场景时，View、Projection、相机位置和裁剪面都会传给 `Scene::OnUpdateEditor()`，天空盒和后处理也沿用同一组参数。

进入 Play 模式后，渲染路径改用场景中的 Primary Camera。编辑器相机仍保留原来的观察状态，停止运行便能回到刚才的工作位置。这条分界避免了编辑视角对运行时镜头产生副作用。

```text
Edit  -> EditorCamera   -> Scene::OnUpdateEditor()
Play  -> Primary Camera -> Scene::OnUpdateRuntime()
```

### 快速聚焦选中实体

视口悬停时按 `F` 可以聚焦当前选中的实体。编辑器会尽量计算实体的世界包围盒：模型使用变换后的网格 Bounds，地形根据网格分辨率和高度缩放估算范围，其余实体回退到 Transform 的缩放值。得到中心和半径后，相机把焦点移到中心，并把距离设为半径的 `2.5` 倍。

这部分比写死一个观察距离实用得多。同一个快捷键既能查看小型网格，也能退到足够远的位置观察整块地形。调试面板也复用了 `SetView()` 和 `Focus()`，无需再维护一套临时相机逻辑。

当前相机状态只存在于编辑器运行期间，还没有保存到项目或场景；移动速度、FOV 和裁剪面也没有对应的编辑器设置项。若后面需要记住每个场景的工作视角，这些参数应进入编辑器配置，不能混进场景相机组件。

## Framebuffer 重构：多附件与优化

最初的 Framebuffer 只负责把颜色画到一张纹理上，显示视口已经够用。等鼠标拾取、HDR 后处理和阴影陆续加入，一张颜色附件就装不下这些数据了。这次重构的重点，是让调用方用规格描述附件组合，再由 OpenGL 后端创建对应资源。

### 用规格描述渲染目标

当前编辑器的场景 Framebuffer 由三类附件组成：

```cpp
FramebufferSpecification sceneFramebufferSpec;
sceneFramebufferSpec.Width = 1280;
sceneFramebufferSpec.Height = 720;
sceneFramebufferSpec.Attachments = {
    { FramebufferTextureFormat::RGBA16F },
    { FramebufferTextureFormat::RED_INTEGER },
    { FramebufferTextureFormat::Depth24Stencil8 }
};

m_Framebuffer = Framebuffer::Create(sceneFramebufferSpec);
```

每个格式承担的工作很明确：

- `RGBA8`：普通八位颜色，最终显示缓冲使用这一格式
- `RGBA16F`：保留 HDR 颜色，场景和 Bloom 中间结果会用到
- `RED_INTEGER`：保存实体 ID，供视口拾取读取
- `Depth24Stencil8`：常规深度和模板附件
- `Depth32F`：可采样的浮点深度纹理，阴影渲染使用这一格式

若规格中没有任何附件，OpenGL 实现会补一张 `RGBA8` 颜色纹理；若没有显式深度格式，还会补一张 `Depth24Stencil8`。这两个默认值让简单离屏渲染仍可只填写尺寸，但复杂渲染目标最好把用途写完整，阅读调用点时会更直观。

### 多附件怎样参与一帧渲染

非多重采样路径会为每个颜色格式创建独立的二维纹理，再把实际存在的颜色槽提交给 `glNamedFramebufferDrawBuffers()`。纯深度 Framebuffer 没有颜色目标，Draw Buffer 和 Read Buffer 都设为 `GL_NONE`，阴影贴图便可以直接使用同一套抽象。

编辑器里的主要数据流如下：

```text
Scene Framebuffer
  attachment 0: RGBA16F      -> 后处理输入
  attachment 1: RED_INTEGER  -> 鼠标拾取
  depth: Depth24Stencil8     -> 深度测试 / 后处理深度

PostProcessRenderer
  HDR Color + Depth -> Bloom / Tone Mapping -> RGBA8 Display

ShadowRenderer
  Depth32F -> 阴影采样
```

每帧绘制场景前，实体 ID 附件会清成 `-1`。鼠标落在视口中时，编辑器从附件 1 读取一个整数，并据此恢复实体选择。这里的 `ReadPixel()` 和 `ClearAttachment()` 按整数附件实现，只适合 `RED_INTEGER`；把它们用于普通颜色附件会得到与接口表面含义不符的结果。

![Framebuffer 多附件阶段的编辑器视口](README.assets/Pasted%20image%2020260720133559.png)

### Resize 为什么改成原地更新

视口尺寸会随着面板拖动频繁变化。旧做法每次都销毁整个 Framebuffer，ImGui 持有的颜色纹理 ID 也会更换，资源创建和界面绑定都比较躁动。

现在 `Resize()` 会先过滤零尺寸、超过 `8192 × 8192` 的尺寸以及与当前规格相同的尺寸。有效变化进入 `ResizeAttachments()`：非 MSAA 颜色纹理继续使用原来的 Renderer ID，只重新分配存储；深度纹理受固定存储接口限制，仍会删除后重建。调用方因此可以稳定持有颜色附件 ID，深度附件 ID 则不能假设永远不变。

`Bind()` 还会把 OpenGL Viewport 设置成 Framebuffer 的宽高。渲染代码只要在正确的目标上调用 Bind，就不必再单独同步一遍视口尺寸。

### 目前的实现边界

这套抽象当前只有 OpenGL 后端，选择 Vulkan 会触发断言，`SwapChainTarget` 字段也还没有落地。MSAA 的规格字段和 Renderbuffer 创建代码虽然已经存在，但多颜色附件会复用同一个 `m_MSAAColorRBO`，附件 ID 也无法作为纹理采样，同时缺少 Resolve 到普通纹理的步骤。因此 `Samples > 1` 只能视作未完成的试验路径，编辑器正式渲染仍应保持 `Samples = 1`。

这次重构解决的是多附件描述、拾取数据承载和常规 Resize 成本。若要继续完成 MSAA，需要为每个颜色槽分别保存多采样资源，并增加一组可采样的目标纹理和明确的 Resolve 阶段；单纯把采样数调大还不能形成完整管线。

## 鼠标拾取 (Mouse Picking)

有了场景视口和实体层级后，靠列表寻找物体很快就显得笨重。鼠标拾取解决的是一个很直接的问题：点击画面中的物体，Hierarchy 和 Inspector 就跟着切到这个实体；点到空白处则清除选择。

这里采用 GPU 整数附件拾取。它复用正常场景渲染的几何和遮挡结果，不需要在编辑器里再维护一套射线与各类包围体求交。

### Entity ID 跟着颜色一起写入

Scene Framebuffer 的附件 0 保存 `RGBA16F` 场景颜色，附件 1 是 `RED_INTEGER`。每帧开始时，编辑器先把整数附件清成 `-1`：

```cpp
RenderPass::Begin(scenePass);
m_Framebuffer->ClearAttachment(1, -1);
```

`-1` 表示当前像素没有实体。不能用 0 作为空值，因为 0 可能是有效的 EnTT entity ID。之后各条渲染路径把当前实体的临时整数 ID 写到 fragment output 的 location 1。

```glsl
layout(location = 0) out vec4 color;
layout(location = 1) out int entityID;

entityID = v_EntityID;
```

Sprite 的 ID 是 `QuadVertex` 中的整数属性，并以 `flat` 方式传到 Fragment Shader。OpenGL VAO 为它调用 `glVertexAttribIPointer()`；若误用浮点版 `glVertexAttribPointer()`，GLSL 虽然能编译，读出的整数却会错。

模型和地形也写入同一附件。普通模型通过 `u_EntityID` 提交，实例化模型从 Instance Buffer 读取 ID，地形使用自己的实体 Uniform。这样一次点击能覆盖当前完整编辑器里的 Sprite、Model 和 Terrain。

```text
Scene Pass
  Color output    -> attachment 0, RGBA16F
  EntityID output -> attachment 1, RED_INTEGER
  Depth test      -> 决定当前可见片元

左键点击 -> 坐标换算 -> ReadPixel(1, x, y) -> Scene 反查实体
```

材质裁剪也会影响拾取。Mask 被 `discard` 的像素不会留下 ID；Blend 像素的有效 Alpha 小于等于 `1/255` 时同样丢弃，其余透明片元按现有透明队列顺序写入。这让选择结果尽量贴近视口中真正画出来的表面。

### 从 ImGui 坐标走到 Framebuffer

ImGui 鼠标坐标以屏幕左上角为原点，Framebuffer 读取坐标以左下角为原点。编辑器先减去视口左上角，再按实际 Framebuffer 尺寸缩放，Y 轴在这一步翻转：

```cpp
int fbX = int(
    (mx - m_ViewportBounds[0].x) / viewportWidth * spec.Width);
int fbY = int(
    (1.0f - (my - m_ViewportBounds[0].y) / viewportHeight)
    * spec.Height);
```

点击只在 Edit 模式、视口悬停且鼠标不位于 Gizmo 上方时处理。读取到非负 ID 后，`Scene::GetEntityByID()` 先通过 EnTT Registry 检查句柄是否仍然有效，再交给 Hierarchy。Hierarchy 同时更新共享的 `SelectionContext`，所以 Inspector 和其他选择消费者会收到同一个结果。

![使用鼠标拾取连续选择并摆放 Cube](README.assets/Pasted%20image%2020260720151128.png)

### 当前取舍

`ReadPixel()` 最终调用一次同步 `glReadPixels()`，存在让 CPU 等待 GPU 的可能。当前只在左键点击时读取一个像素，没有放进每帧悬停逻辑，实际编辑负担很小。如果以后要做持续 Hover 高亮或框选，更合适的做法是增加异步 PBO 读回或单独的选择流程。

附件中保存的是当前 Scene 的 EnTT ID，不是实体 UUID。它只用于眼前这一帧和当前 Registry，不能写入场景文件，也不能跨 Scene 缓存。

## 着色器系统优化：Uniform 缓存与 UBO

Shader 接口早期每上传一个 Uniform 都会调用一次 `glGetUniformLocation()`。功能没问题，但同一 Program 链接完成后，Location 在它的生命周期内不会变化，逐帧重复用字符串查询没有意义。相机矩阵也在多个 `BeginScene()` 重载中走普通 Uniform 上传，公共数据和材质私有数据混在了一起。

这轮整理分成两部分：缓存单独 Uniform 的 Location，再用 Uniform Buffer 承载 Renderer2D 的相机块。

### Location 缓存要跟 Program 生命周期走

`OpenGLShader` 现在维护一张名称到 `GLint` 的缓存。所有整数、浮点、向量和矩阵上传都经过同一个查询入口：

```cpp
GLint OpenGLShader::GetUniformLocation(const std::string& name) const
{
    const auto cached = m_UniformCache.find(name);
    if (cached != m_UniformCache.end())
        return cached->second;

    const GLint location =
        glGetUniformLocation(m_RendererID, name.c_str());
    m_UniformCache[name] = location;
    return location;
}
```

首次使用仍会询问驱动，后续上传直接查表，找不到的 `-1` 也会被缓存。缓存的有效期不能超过 OpenGL Program：Shader 热重载成功后会换成新的 Program ID，旧 Location 全部失效。因此事务式重载在替换 Program 时会清空 `m_UniformCache`；若新源码编译或链接失败，旧 Program 和旧缓存都保留。

Renderer2D 的纹理采样器数组属于 Program 自身状态。Texture Shader 热重载成功后，渲染器会重新上传 `u_Textures[32]`，不能指望新 Program 继承旧值。

### Camera UBO 的实际布局

`UniformBuffer` 提供平台无关的创建、绑定和局部更新接口。OpenGL 后端用 DSA 创建 Buffer，通过 `glBindBufferBase()` 绑定到指定槽位，再用 `glNamedBufferSubData()` 更新内容。

Renderer2D 在 binding 0 创建一块 80 字节的 Camera Buffer：

```cpp
struct CameraData
{
    glm::mat4 ViewProjection; // 64 bytes
    float Time;               // 4 bytes
    float _pad[3];            // 补到 80 bytes
};

static_assert(sizeof(CameraData) == 80);
```

三个 `BeginScene()` 重载负责算出各自的 ViewProjection、记录时间并更新这块 Buffer，然后统一进入 `StartBatch()`。当前 Texture Shader 的 Camera Block 只声明了 `u_ViewProjection`，也就是读取前 64 字节；`Time` 虽然已经上传，却没有在这个 Shader 的 UBO 声明中消费。全屏绘制仍通过普通 `u_Time` Uniform 传时间，旧文档所说的所有 Shader 自动共享 VP 和时间并不准确。

```glsl
layout(std140, binding = 0) uniform Camera
{
    mat4 u_ViewProjection;
};
```

`std140` 约束 CPU 和 GLSL 对矩阵、向量的对齐理解。80 字节结构给后续扩充 Camera Block 留了位置，但两端声明仍需同步；仅在 C++ 里加字段不会让 Shader 自动获得数据。

### 这次优化覆盖到哪里

`StartBatch()` 统一重置索引数、CPU 顶点写指针和纹理槽，避免三个 `BeginScene()` 与 Flush 路径各自维护一份批次初始化。之后引擎也用同一套 `UniformBuffer` 抽象建立了 binding 1 的 Light UBO。

UBO 适合 ViewProjection、光源环境这类跨 Draw 共享的数据。材质参数、实体 ID、采样器槽位仍会随 Shader 或 Draw 改变，继续使用普通 Uniform 更合适。Location 缓存减少查询次数，UBO 减少公共数据的重复提交，两者解决的不是同一个问题。

## Vulkan / SPIR-V 接口预埋

这一阶段并没有实现 Vulkan 渲染器，做的是先把名称、依赖和少量工厂分支放进工程，暴露出 OpenGL 代码目前卡得有多深。回头看，这些预埋更像一份迁移清单，距离可以切换后端还有很长一段实现工作。

### 已经放进仓库的部分

`RendererAPI::API` 有 `None`、`OpenGL` 和 `Vulkan` 三个值，默认值仍是 OpenGL。Framebuffer、UniformBuffer、ComputeShader、PixelBuffer、Texture 和 Cubemap 等部分工厂识别 Vulkan 分支，但当前只会断言或返回空值。

`Shader::CreateFromBinary()` 接受 Vertex 和 Fragment 两组 SPIR-V 字节码，接口已经存在，不过 OpenGL 与 Vulkan 两个分支目前都会断言。仓库中没有 `Platform/Vulkan/`，也没有 `VkInstance`、Device、Surface、Swapchain、Command Buffer、Descriptor Set 或 Pipeline 的实现。

依赖侧加入了两个子模块：

- `Vulkan-Headers` 提供 Vulkan 类型和函数声明
- `SPIRV-Cross` 作为独立静态库项目进入 Premake Workspace

根 Premake 会读取 `VULKAN_SDK`，没有 SDK 时保留本地 Headers 路径；同时排除 SPIRV-Cross 自带的 samples 和 tests，避免它们混入引擎解决方案。当前 `Glimmer` 静态库没有包含 Vulkan Header 路径，没有链接 `vulkan-1.lib`，也没有链接或调用 SPIRV-Cross。它们现在只是可用依赖，还没进入运行链路。

### 为什么现在不能调用 SetAPI(Vulkan)

表面上有 `RendererAPI::SetAPI()`，实际后端选择还不统一。下面这些路径仍直接构造 OpenGL 类型：

- `RenderCommand::s_RendererAPI` 固定创建 `OpenGLRendererAPI`
- VertexBuffer、IndexBuffer 与 VertexArray 工厂直接返回 OpenGL 实现
- 文件型 Shader 和源码型 Shader 直接创建 `OpenGLShader`
- 从文件加载 Texture2D 的重载也绕过后端分支

因此在程序运行中把枚举改成 Vulkan，只会让一部分资源走断言，另一部分继续创建 OpenGL 对象。这不是安全的运行时开关。

```text
当前状态
  Renderer API 名称与部分 case 分支
  Vulkan-Headers / SPIRV-Cross 子模块
  SPIR-V 二进制工厂签名
                |
                v
  尚缺统一后端工厂、Vulkan Context 和完整资源实现
```

### 真正接入时要补什么

下一步应先统一 RendererAPI、Buffer、VertexArray、Texture 和 Shader 的创建入口，再建立 Vulkan Context、Surface、Swapchain 与帧同步。随后才能定义 Render Pass、Pipeline、Descriptor 和 Command Buffer 如何对应现有 Renderer2D/Renderer3D 调用。

SPIR-V 也不能只停留在 `CreateFromBinary()`。需要确定 GLSL 到 SPIR-V 的编译阶段、反射结果如何生成 Descriptor/Pipeline Layout、缓存怎样版本化，以及热重载失败时怎样保留上一条有效 Pipeline。在这些工作完成前，项目的可运行后端仍然只有 OpenGL。

## 内容浏览器 (Content Browser Panel)

编辑器刚能保存场景时，我一直在资源管理器和 Glimmer 之间来回切换：找文件、确认路径，再回到编辑器加载。文件一多，这种操作很容易打断手头的场景编辑。Content Browser 就是在这个阶段加入的，它留在编辑器应用层，负责浏览 `assets/`、选择资产和发起拖放，不进入引擎核心。

### 从文件列表长成资产入口

面板第一次绘制时才解析 `assets/` 的绝对路径，构造 `EditorLayer` 时不会提前遍历磁盘。左边是递归目录树，右边按当前宽度计算网格列数；中间的 4 像素分隔条可以拖动，树和网格各自滚动。

```text
Content Browser
  assets 目录树     |     当前目录文件网格
  单击切换目录      |     单击选择 / 双击打开 / 拖放
                    |
             可拖动分隔线
```

目录树只展开用户点开的节点，叶子目录不画多余箭头。回退按钮到达 `assets/` 后停止，面板内部的导航路径不会越过项目资产根。文件网格每帧直接遍历当前目录，没有维护另一份文件缓存，所以在外部新增文件后通常能马上看到。

![带目录树和文件网格的 Content Browser](README.assets/Pasted%20image%2020260721105118.png)

单击普通文件时，面板会调用 `AssetManager::ImportAsset()`。AssetManager 先确认文件位于项目 `assets/` 内，再按规范化相对路径去重；支持的类型会取得稳定 `AssetHandle` 并写入 `AssetRegistry.yaml`。随后 `SelectionContext` 切换为资产选择，Inspector 显示该资产的信息，原来的实体选择被清除。

`.glimmer` 场景文件走的是另一条路。它没有注册为通用 Asset，双击后由 `EditorLayer` 创建新 Scene 并交给 `SceneSerializer` 反序列化；成功后才替换当前编辑场景，命令历史和旧选择也会清理。

### 拖放为什么仍使用文件路径

Content Browser 对所有普通文件都发送名为 `SCENE_FILE` 的拖放载荷，内容是一条绝对路径。这个名字是早期只拖场景时留下的，现在接收方会根据扩展名和导入后的 AssetType 决定动作：

- 场景拖到视口后打开
- Terrain Material 赋给已有地形，必要时创建地形实体
- `.glsky` 或 `.hdr` 赋给 Sky Light
- 普通图片拖到视口时按高度图创建 Terrain
- Texture、Model 和 Material 拖到组件字段时写入对应 Handle

接收区必须放在 `ImGui::Image()` 或具体属性控件之后，ImGui 才能把整个可见区域识别为 Drop Target。

右键空白处还可以创建文件夹、Material、Terrain Material、Skybox、Scene 和 Shader，也可以生成 Cube、UV Sphere、Plane 几种基础几何。除 Scene 外，新建文件会立即导入 Asset Registry，省去手动刷新步骤。

### 当前的粗糙处

这个面板仍是直接文件系统视图，没有搜索、排序、过滤、重命名和删除工作流。目录树为了判断叶子节点会额外扫描子目录，右侧也会逐帧遍历当前目录；目前资产规模不大，做法简单够用，但不适合直接扩展到很大的内容库。

`SCENE_FILE` 载荷也已经名不副实。后续更稳妥的方案是统一传递带类型的 AssetHandle，仅让未注册的 Scene 保留路径载荷。这样接收方不用重复解析扩展名，也能减少字符串协议散落在多个面板里的情况。

## SpriteRenderer 贴图支持

最早的 Sprite 只有一项颜色，能画 UI 占位和纯色 Quad，却放不进真正的图片。第一次加贴图时，组件里直接保存过 `Ref<Texture2D>`，渲染很方便，保存场景却没法处理 GPU 对象。资产系统接入后，这个字段改成了稳定 Handle。

### 组件只保存可持久化的数据

```cpp
struct SpriteRendererComponent
{
    glm::vec4 Color{ 1.0f };
    AssetHandle TextureHandle{ 0 };
    float TilingFactor = 1.0f;
};
```

`TextureHandle == 0` 或 Handle 无效时，Sprite 回退为纯色。Scene YAML 保存 Color、Texture Handle 和 TilingFactor，加载时再通过 AssetManager 延迟取得 `Texture2D`。组件因此可以安全复制到 RuntimeScene，也不会把 Renderer ID 或内存指针写入场景文件。

### DrawSprite 负责解析最终外观

Scene 的 Edit 和 Play 渲染路径都把 Sprite 交给同一个 `Renderer2D::DrawSprite()`。函数先读取组件自身的颜色、TextureHandle 和 TilingFactor；实体若带有有效 `MaterialComponent`，则构造 `MaterialInstance`，用最终 Material 属性覆盖这三项。

```text
SpriteRenderer defaults
        +
Material + entity overrides（可选）
        |
        v
最终 Color / BaseColorTexture / TilingFactor
        |
        v
AssetManager::GetTexture2D()
        |
        +-- 有纹理 -> textured quad
        +-- 无纹理 -> color quad
```

这个优先级有意让共享 Material 和实体 Override 也能驱动 2D Sprite。代价是 Inspector 里修改 Sprite 自身字段后，如果实体同时绑定了 Material，画面可能没有变化；此时真正生效的是 Material 合并结果。

Renderer2D 把纹理放进批次的 32 个 Texture Slot，slot 0 固定保留白纹理。批次容量或纹理槽用完时才 Flush。Entity ID 仍随四个顶点写入整数附件，所以纯色和贴图 Sprite 都能被鼠标拾取。

### Inspector 中的使用方式

Sprite Renderer 面板可以编辑 Color 和 Tiling，把 Content Browser 中的 `.png`、`.jpg`、`.jpeg`、`.tga` 或 `.bmp` 拖到纹理字段即可导入并保存 Handle；旁边的 `X` 会清空引用。

![SpriteRenderer 使用 Texture Asset](README.assets/Pasted%20image%2020260721144641.png)

这部分 UI 仍有技术债。Color、Tiling、纹理拖放和清除目前直接改组件，没有像 Transform 或 Material 那样进入 `CommandHistory`，所以 `Ctrl+Z` 不能可靠撤销这些操作。后面接入时应把连续数值拖动合并成一条命令，Handle 拖放则记录一次前后组件快照。

## 编辑/播放模式 (Edit/Play Mode)

如果 Play 直接在编辑场景上运行脚本，测试过程中改掉的位置、临时生成的实体和运行时资源都会留在作者数据里。真正需要隔开的其实是场景所有权。Glimmer 进入 Play 时会复制一份 RuntimeScene，停止后整份丢弃。

### 进入 Play 时发生了什么

`EditorLayer` 同时维护三个引用：

```text
m_EditorScene  -> 作者正在编辑的源场景
m_RuntimeScene -> Play 开始时创建的副本
m_ActiveScene  -> 当前面板与渲染使用的场景
```

按下工具栏 Play 或 `Ctrl+P` 后，编辑器先退出临时 Debug Scene，再用 `Scene::Copy(m_EditorScene)` 创建运行时副本。复制过程保留实体 UUID 和受支持组件；Native Script 只复制创建、销毁函数，不复用旧 Instance；Terrain 只复制 Specification，GPU Runtime 会在副本中独立重建。

Hierarchy 与 Inspector 随后切到 RuntimeScene，CommandHistory 指针被移除。切换前若选中了实体，编辑器会保存它的 UUID，在副本中找到同一个逻辑实体并恢复选择。EnTT ID 可能已经变化，因此这里不能沿用拾取用的临时整数 ID。

```cpp
m_RuntimeScene = Scene::Copy(m_EditorScene);
m_ActiveScene = m_RuntimeScene;
m_ActiveScene->OnRuntimeStart();
m_SceneState = SceneState::Play;
```

`OnRuntimeStart()` 目前没有主动工作。Native Script 在第一次 `OnUpdateRuntime()` 遇到尚未实例化的组件时创建对象，依次调用 `OnCreate()` 和当帧 `OnUpdate()`。

### 两种模式的每帧分流

Edit 使用 `EditorCamera`，不更新 Native Script；Play 先运行脚本，再寻找 Primary SceneCamera。找到主相机后，模型、地形、Skybox、Sprite、透明队列和后处理沿用同一套渲染编排，只是 View、Projection 与相机位置来自场景实体。

Play 场景没有 Primary Camera 时，脚本仍会更新，但 Scene 不提交模型、地形和 Sprite 绘制，ShadowRenderer 也会禁用。这个状态不会自动借用 EditorCamera，缺相机应当被当作场景配置问题处理。

编辑器工具在两种模式间有明确分界：

- EditorCamera 输入、Gizmo、相机视锥和鼠标拾取只在 Edit 工作
- Undo/Redo 快捷键只在 Edit 且没有临时 Debug Scene 时工作
- Play 中的 Hierarchy 与 Inspector 面向 RuntimeScene，命令历史关闭

第三条容易误解。面板并没有把所有运行时组件字段锁成只读，部分直接编辑路径仍能修改 RuntimeScene；这些变化只是不会写回 EditorScene，按 Stop 后一起丢弃。

![Edit 模式下使用 EditorCamera 调整场景](README.assets/Pasted%20image%2020260721155626.png)

![Play 模式使用场景主相机运行](README.assets/Pasted%20image%2020260721155635.png)

### Stop 如何恢复编辑现场

停止播放时，RuntimeScene 先调用 `OnRuntimeStop()`。每个已有 Native Script Instance 都会收到 `OnDestroy()`，随后由绑定的销毁函数释放。编辑器把 ActiveScene 切回 EditorScene，释放 RuntimeScene，重新连接面板的 CommandHistory。

选择同样按 UUID 映射回源场景。脚本位移、运行时创建的实体、Terrain Runtime 和手动改过的运行时组件都随副本释放，EditorCamera 的观察位置则一直由编辑器持有，所以停止后还能回到之前的工作视角。

## Compute Shader 基础设施

Compute Shader 最初只是编辑器里的 256×256 渐变测试。那张图的价值很有限，但它确认了文件编译、Image 绑定、Dispatch 和 Barrier 这条最短路径能跑通。后来地形生成、侵蚀、水文与气候模拟都沿着这套接口扩展，早期测试代码反而退出了主流程。

![早期 Compute Shader 渐变输出验证](README.assets/Pasted%20image%2020260722150427.png)

### 接口刻意保持得很小

`ComputeShader` 目前提供 Program 绑定、三维 Dispatch、少量 Uniform 上传和 Storage Image 绑定。公共枚举把访问方式与纹理格式从 OpenGL 常量中隔离出来：

```cpp
enum class ImageAccess { Read, Write, ReadWrite };
enum class ImageFormat { RGBA8, RGBA16F, RGBA32F, R32F };

shader->BindImageTexture(
    binding, texture->GetRendererID(), 0,
    ImageAccess::Write, ImageFormat::R32F);
shader->Dispatch(groupX, groupY, 1);
ComputeShader::Barrier();
```

OpenGL 后端负责 `glBindImageTexture()` 和 `glDispatchCompute()`。Uniform Location 会缓存；文件型 Compute Program 支持轮询热重载，新源码只有在完整编译、链接成功后才替换旧 Program，失败时继续保留上一版本。读取 GLSL 时也会剥离 UTF-8 BOM，避免 `#version` 前出现驱动不接受的字节。

`ComputeShader::Barrier()` 当前统一提交 Image Access、Shader Storage 和 Texture Fetch 三类全局内存屏障。它的范围偏宽，但使用规则简单：一个 Dispatch 产生的 Image 写入要被下一次计算或图形采样读取时，先执行 Barrier。

### 真正难的是资源所有权

只提供 Dispatch 还不够。地形模拟里最容易出错的是同一张纹理在一个阶段内既读又写，或者过早交换前后状态。现在的生成和环境模拟都使用明确的 Ping-Pong：

```text
ReadTexture  --只读--> Compute Pass --只写--> WriteTexture
                                           |
                                      全局 Barrier
                                           |
                                      Swap Read/Write
```

Terrain Generator 先生成 `R32F` Height，再执行有限次 Authoring Erosion，随后派生 Normal/Slope、Analysis 和 Material Weights。GPU Hydrology 用多组 Ping-Pong 保存 Water、Flux、Velocity、Sediment 与 Runtime Height；GPU Climate 则推进 Temperature、Atmospheric Moisture 和 Vegetation Potential，并输出 Rainfall、Evaporation 与 WaterSource。

Climate 与 Hydrology 由同一个固定步协调器驱动，每个子步固定按 Climate、Barrier、Hydrology 的顺序执行。Shadow Pass 和九个 Terrain Chunk 可能在同一帧多次调用 Prepare，因此 Runtime 还用 FrameSerial 保证环境状态只前进一步。

Compute Shader 热重载成功后，相关 Runtime 会按各自规则重新计算或继续使用新 Program。普通渲染帧只采样已经生成的纹理，不会为了显示结果把整张图读回 CPU。

### 当前边界

接口只覆盖项目现有用到的 `int`、`float`、`vec2` Uniform 和四种 Image Format，还没有 SSBO 抽象、间接 Dispatch、异步计算队列或资源状态追踪。Barrier 也由调用方手动放置，漏掉一次不会得到友好的验证信息。

Vulkan 分支仍会断言。将来接入新后端时，Image Layout、Pipeline Barrier 和 Queue Ownership 需要进入后端实现；不能把 OpenGL 的一次全局 `glMemoryBarrier()` 原样理解成跨 API 的同步模型。

## GPU 数据读回 (GPU Readback)

Compute 结果常驻 GPU 最省事，Terrain Shader 可以直接采样 Height、Water 或 Climate 纹理。CPU 只有在验证数值、生成统计或保存结果时才需要读回。把读回放进每个模拟帧，会重新把 GPU 并行流程拖回同步等待。

### 当前可靠路径是同步读回

`Texture::GetImageData()` 由 OpenGL 后端调用 `glGetTextureImage()`。纹理对象知道自身的 Data Format 和 Data Type，因此 `RGBA8` 使用字节，`R32F`、`RG16F`、`RGBA16F` 等浮点格式按 `float` 传输。接口会检查目标指针和缓冲区大小。

```cpp
std::vector<float> water(pixelCount);
waterTexture->GetImageData(
    water.data(),
    uint32_t(water.size() * sizeof(float)));
```

这是阻塞调用。Terrain Generator 的输出验证、Hydrology/Climate 的显式 Readback 和 GPU Contract 都使用它；普通环境模拟帧不读回。Hydrology 创建 Runtime 时也会同步读取一次初始 Height，用于 Reset 和侵蚀下界。

鼠标拾取不走 Texture 接口。它从 Framebuffer 的整数附件同步读取单个像素，触发频率和数据规模都不同。

### PixelBuffer 的现状

`PixelBuffer` 封装了两张 Pixel Pack Buffer，API 名义上提供 `BeginRead()`、`IsReady()`、`Map()` 和 `Unmap()`。OpenGL 实现把 `glGetTextureImage()` 的目标指向 PBO，设计意图是让当前传输与上一份 CPU 读取交错。

但现有实现还不能保证真正异步：

- 只按 `RGB/RGBA + GL_UNSIGNED_BYTE` 计算大小，不支持 Terrain 常用的浮点纹理
- `BeginRead()` 后立刻把刚提交传输的同一张 PBO 标为可 Map
- 没有 `GLsync` Fence，也没有查询 DMA 是否完成
- `glMapBuffer()` 可能在数据未就绪时等待 GPU

更直接的一条事实是，当前引擎没有生产代码调用 `PixelBuffer::Create()`。它是尚未收口的基础设施，不能拿来证明全纹理读回已经无阻塞。

若要继续完成这条路径，应让一张 PBO 接收本帧传输，另一张只在 Fence 已完成时开放映射，并让规格携带 Texture Format 或明确的字节步长。CPU 处理若要跨帧保留数据，还需要在 Unmap 前复制到自己管理的内存；Map 返回的指针只在映射期间有效。

## 多 Pass 渲染管线

早期编辑器把 Framebuffer 的 Bind、Clear 和 Unbind 散落在 `OnUpdate()` 里。只有一张场景颜色图时还能看懂，加入阴影、HDR、Bloom 和显示映射后，目标纹理之间的关系开始变得混乱。RenderPass 先把最基础的生命周期收拢起来，后处理则逐步迁到独立的 `PostProcessRenderer`。

### RenderPass 管理了什么

```cpp
struct RenderPassSpecification
{
    Ref<Framebuffer> Target;
    bool ClearColor = true;
    bool ClearDepth = true;
    glm::vec4 ClearColorValue{ 0.1f, 0.1f, 0.1f, 1.0f };
};
```

`Begin()` 保存当前规格、绑定目标 Framebuffer 并按设置清屏，`End()` 解绑目标并清除 Active 状态。Framebuffer Bind 会同步 OpenGL Viewport。ShadowRenderer 临时绑定级联深度目标后，`RebindCurrentTarget()` 可以恢复外层 Scene Framebuffer 及其尺寸。

这层封装很薄，也有一处容易被名字误导的行为：当 `ClearColor == true` 时，代码调用 `RenderCommand::Clear()`，OpenGL 实现会同时清 Color 和 Depth。因此 `ClearColor=true, ClearDepth=false` 目前仍会清深度；只有关闭 Color 后，`ClearDepth` 才单独决定是否调用 `ClearDepth()`。

RenderPass 只保存一个 Active Pass，不是可嵌套栈。调用 `End()` 时也假定当前确实存在 Pass。它更接近 FBO 生命周期助手，还不是 Render Graph。

### 当前视口帧怎样流动

```text
Directional Shadow Cascades
             |
             v
Scene Pass: RGBA16F Color + EntityID + Depth
  Opaque/Mask -> Terrain -> Skybox -> Sprite -> Transparent
             |
             +--------------------+
             |                    |
             v                    v
Half-res Bloom Extract       Scene Depth
      Ping-Pong Blur              |
             +---------+----------+
                       v
Tone Mapping: Fog -> EV -> ACES -> Gamma
                       |
                       v
                RGBA8 Display FBO
                       |
                       v
                  ImGui Viewport
```

Scene Pass 结束后，`EditorLayer` 只整理 Scene Color、Depth、相机逆 ViewProjection、相机位置和光照输入，再调用引擎侧 `PostProcessRenderer::Execute()`。Bloom 使用两张半分辨率 `RGBA16F` Framebuffer 做软阈值提取与横纵 Ping-Pong 模糊；Tone Mapping 输出到独立的 `RGBA8` Display Framebuffer，视口最终显示这张纹理。

![早期纯色 Pass 验证](README.assets/Pasted%20image%2020260722160621.png)

![加入全屏 Shader 后的后处理 Pass](README.assets/Pasted%20image%2020260722162113.png)

Overlay Pass 代码仍保留在 `EditorLayer`，但整段处于注释状态，不属于当前启用链路。Bloom、Fog、Exposure EV 和 ACES White Point 设置也只是编辑器运行时状态，没有写入 Scene YAML。

### 还缺少的调度能力

现有 Pass 不声明输入输出依赖，不自动安排 Barrier、资源复用或执行顺序，也不会检测同一纹理的读写冲突。PostProcessRenderer 的顺序仍由 C++ 显式编排。这个阶段的收获是把目标 FBO 和清理边界写清楚；若以后增加 TAA、SSR 或更多跨帧资源，再考虑引入真正的 Render Graph 会更合适。

## 高度图地形系统

### 这一章解决了什么

地形最早只需要回答一个问题：一张灰度图，怎样稳定地变成场景里的可编辑几何？当前做法仍沿用这条主线。规则网格提供拓扑，顶点 Shader 采样高度纹理并沿 Y 轴位移，`TerrainRenderer` 再把它接入场景深度、阴影和材质流程。

这个入口很实用。程序化生成器出问题时，可以换回磁盘高度图，先判断故障出在数据生成还是绘制链路；美术也能直接拖入已有图片，不必理解 Compute Shader。

### 数据放在哪里

地形现在是普通 ECS 实体，由 `TransformComponent` 和 `TerrainComponent` 组成。组件只保存可复制、可序列化的 `TerrainSpecification`，其中包括高度图 `AssetHandle`、网格分辨率和高度缩放。网格、纹理引用及模拟对象都放在 `TerrainRuntime`，复制 Scene 或进入 Play 时会重新建立，不写入 YAML。

外部高度图的使用路径很短：

```text
Content Browser 中的图片
    -> 导入为 Texture2D Asset
    -> 拖到 Terrain Inspector，或拖进视口新建 Terrain
    -> TerrainRenderer 按 AssetHandle 解析纹理
    -> 顶点 Shader 采样高度并完成位移
```

将 `Procedural` 关闭后，Inspector 会显示高度图拖放入口。切换图片走编辑器命令，因此可以 Undo/Redo。外部图片模式目前只提供高度本身，程序化路径生成的 Normal/Slope、Analysis 和 Material Weights 不会沿用，这一点在排查材质差异时很容易忽略。

### 网格与采样

`TerrainMesh` 只生成规则网格和 Skirt 顶点，不把高度烘进 Vertex Buffer。`HeightScale` 改变时无需重建网格，Shader 会使用高度图尺寸计算 `u_TexelSize`，并按地形世界尺寸计算 `u_SampleSpacing`。后一个值不能写死为 `1.0`，否则高分辨率图片的坡度和法线会偏掉。

当前地形被拆成固定 `3×3` Chunk，Runtime 为它准备三档共享 LOD Mesh。每帧根据相机距离选择层级，再做迟滞和相邻级差约束；视锥外的 Chunk 不提交，Skirt 负责遮住不同层级交界处的裂缝。这里仍是一张完整高度纹理，没有动态 Chunk 流送。

### 绘制时我刻意保留的边界

Terrain 与普通场景几何共用颜色和深度目标，所以它必须尊重已经写入的深度。早期原型曾在 Terrain Pass 再清一次 Depth，结果地形总能盖住先画的模型。现在清理由场景帧统一负责，`TerrainRenderer` 只提交可见 Chunk。

纹理寻址使用 Clamp to Edge，采样点也按 Texel 尺寸处理边缘。这样做看似琐碎，却解决了高度图左右两端互相取样形成的接缝。地形渲染 Shader、阴影采样、四层 `TerrainMaterial` 以及环境诊断都复用同一份 Runtime Height，后续功能无需各自维护一套高度来源。

### 当前边界

- 外部高度图不会自动生成程序化路径的三张派生图。
- Chunk 布局固定为 `3×3`，目前没有大世界流送和动态细分。
- Runtime 水文、侵蚀与气候修改不会自动烘回图片，也不会随 Scene 保存。

这套实现留下的经验很直接：先把高度数据、几何拓扑和运行时资源拆开。地形功能越往后加，这个边界越省事。

## Shader 实时热重载

### 为什么要做

调 Shader 时，重启编辑器的成本比编译本身更烦。热重载让文件保存后直接得到新画面，语法写错也能继续看着上一个有效版本修改。它缩短的是渲染调试循环，并不改变 Shader 的资产边界。

### 从保存文件到替换 Program

`FileWatcher` 轮询主文件的 `last_write_time`。时间戳变化后会等待 200 ms，确认写入稳定再返回一次变更，避开编辑器保存时的临时文件和连续写入。轮询发生在正常更新流程里，没有后台线程拿着 OpenGL Context 编译 Shader。

```text
保存 .glsl / .comp
    -> FileWatcher 防抖
    -> ReloadIfChanged()
    -> 读取并移除可选 UTF-8 BOM
    -> 编译、链接临时 Program
    -> 成功后交换 Program ID，清空 Uniform 缓存
```

这里需要守住的是延迟交换。新的 Stage 或 Program 只要有一步失败，临时对象就会被清理，原来的 `m_RendererID` 保持不动。修好文件再次保存后，Watcher 还能触发下一次尝试。成功重载会增加 Version；Uniform Location 在重新链接后可能变化，所以旧缓存必须清空。

### 谁负责触发重载

图形 Shader 与 Compute Shader 共用 `ShaderReloadResult`，里面记录本次是否尝试、是否成功以及编译信息，但它们的管理入口不同。

- `ShaderLibrary` 管理编辑器中的文件型图形 Shader，`ShaderPanel` 提供 Auto Reload、Reload All 和单文件 Reload。
- `Renderer2D`、`Renderer3D`、`TerrainRenderer` 与 `ShadowRenderer` 会在各自拥有的渲染阶段检查 Shader。
- `TerrainGenerator`、GPU 水文和气候模块自行轮询所属 Compute Shader，重载成功后按模块规则重建派生结果。

这种分工比把全部 Shader 塞进 `EditorLayer` 更稳。资源的拥有者最清楚重载后需要补什么状态。例如 `Renderer2D` 必须重新上传 `u_Textures` Sampler 数组；否则 Program 虽然编译成功，批次里的纹理槽仍会错。

### 实际调试方式

打开 `Shaders` 面板并保持 Auto Reload 开启，修改已加载的文件即可。失败信息会留在面板和日志中，视口继续使用旧 Program。由内存字符串创建的 Shader 没有源文件路径，不能参与自动监控。

当前 Watcher 只盯主文件，项目也还没有 GLSL `#include` 依赖图。以后若引入公共 Include，需要让一次文件变更能标记全部依赖 Shader；眼下的独立 `.glsl` 与 `.comp` 文件不受这个限制。

开发这部分时最容易犯的错，是把编译成功当成重载完成。Program 交换后的 Uniform、Sampler 以及依赖它生成的纹理，都需要由真正的拥有者恢复。

## 拟真程序化地形生成

### 从随机噪声到可用地貌

第一版生成器能造出起伏，却很像一张铺满均匀噪声的毯子。大陆、丘陵和山脊共用相近的噪声信号，层次挤在一起，Seed 换了，整体气质却没怎么变。这一章记录的工作，就是把 GPU 高度生成整理成一条可重复、可编辑的 Authoring 管线。

`GenerateFBM.comp` 先生成 R32F 高度场。大陆轮廓控制低频分布，丘陵补足缓坡，Ridged fBm 与 Mountain Mask 决定山脉位置，Domain Warp 用来打散过直的边界。沟谷参数只是生成阶段的形态修饰，不承担水量或泥沙守恒；真实 Runtime 水文由后面的独立模拟系统负责。

### 规格由组件持有

`TerrainSpecification` 保存 Seed、频率、Octave、山脉方向、地质混合等参数，也保存高度/网格分辨率与 Compute Shader Handle。Inspector 修改这些值时会把预设切回 `Custom`，再让 Runtime 失效。面板不直接持有 `TerrainGenerator`，Scene 复制和 Edit/Play 隔离因此仍以组件值为边界。

为了快速得到有明显差异的起点，项目提供 `Alpine`、`Plateau`、`Rolling Hills`、`Volcanic` 和 `Eroded Valley` 五个预设。预设只写入规格，之后手动调整仍走同一套生成流程，不存在隐藏的第二套算法。

### 一次生成包含哪些 Pass

```text
Terrain Dirty / 手动 Regenerate / Compute Shader 重载成功
    -> GenerateFBM.comp 写入 R32F Height Ping-Pong
    -> 可选 ThermalErosion.comp 执行有限次数迭代
    -> DeriveTerrainMaps.comp 生成三张 RGBA16F 派生纹理
    -> TerrainRenderer 更新 Runtime 引用和 Generation Version
```

热力侵蚀属于 Authoring 操作，默认有限次执行，只有规格变脏或手动再生成时才运行。它不会随帧率暗中推进。每轮都从 Height Read 读取、向 Height Write 写入，Barrier 后交换两张纹理，避免在一次 Dispatch 中读写同一资源。

派生阶段输出 `Normal/Slope`、`Analysis` 和 `MaterialWeights`。这些数据仍留在 GPU，供地形光照、四层材质混合和诊断视图采样。Scene YAML 只保存生成规格，Height 与派生纹理在加载后重建，这能避免把驱动参数和缓存结果一起保存后逐渐失配。

### 渲染与模拟怎样接上

生成器产出的 Height 是地形后续系统的共同起点。`TerrainRenderer` 用它做顶点位移，固定 `3×3` Chunk 从同一张纹理的不同 UV 区域采样；三档 LOD 改变的是网格密度，不复制高度数据。TerrainMaterial 读取派生权重，Runtime 水文或侵蚀修改 Height 后会按需要刷新派生图。

Authoring 与 Runtime 模拟必须分开看。前者由可序列化参数确定，适合反复生成同一地貌；后者有固定时间步和独立状态集，目前不会写回 `TerrainSpecification`。如果要保存模拟结果，需要单独设计显式 Bake，不能把运行时纹理偷偷塞进 Scene 序列化。

### 验证与现状

设置 `GLIMMER_TERRAIN_VALIDATE=1` 后，生成器会读回 Height 和三张派生图，检查数值范围、Material Weight 归一化以及同一规格重复生成的 Hash。这个入口会同步读回 GPU，只用于受控验证，不放进正常帧循环。

目前仍使用固定范围的高度纹理与固定 `3×3` Chunk，没有动态大世界流送。生成结果也没有磁盘派生缓存或 Bake 格式。对现在的编辑器来说，这个边界够清楚：规格负责复现地貌，Runtime 负责昂贵资源，后续模拟在自己的时间轴上运行。

## UUID 与稳定实体标识

为场景实体加入了独立于 `entt::entity` 的 64 位 UUID。`entt::entity` 只用于当前 Registry 内部索引，UUID 用于场景保存、加载、跨场景复制和未来实体引用。

本次完成：

- 新增线程安全的 `UUID` 类型，并排除无效值 `0`；
- 新增 `IDComponent`，所有通过 `Scene` 创建的实体默认获得 UUID；
- `Entity` 提供 `GetUUID()`；
- `Scene` 提供 `CreateEntityWithUUID()` 和 `FindEntityByUUID()`；
- Scene 内部通过 `unordered_map<UUID, entt::entity>` 进行快速查找；
- 销毁实体时同步移除 UUID 映射；
- YAML 场景格式升级为 `Version: 2`，保存并恢复 UUID；
- 无 Version 的旧场景按 Version 1 加载，并为实体生成新 UUID。

```text
CreateEntity()
    → 生成 UUID
    → 添加 IDComponent
    → 写入 UUID → entt::entity 映射

Serialize()
    → 保存 UUID

Deserialize()
    → CreateEntityWithUUID()
    → 恢复稳定实体身份
```

相关文件：

```text
Glimmer/src/Glimmer/Core/UUID.h/.cpp
Glimmer/src/Glimmer/Scene/Components.h
Glimmer/src/Glimmer/Scene/Entity.h/.cpp
Glimmer/src/Glimmer/Scene/Scene.h/.cpp
Glimmer/src/Glimmer/Scene/SceneSerializer.cpp
```

UUID 是下一步实现 `Scene::Copy`、编辑/运行场景隔离、实体引用和组件运行资源缓存的基础。

## AssetHandle 与基础资产管理系统

在实体拥有稳定 UUID 后，下一步是让场景组件不再直接保存运行期资源指针。此前 `SpriteRendererComponent` 保存 `Ref<Texture2D>`，它只在当前进程中有效，无法稳定写入场景，也让组件承担了纹理加载和 GPU 生命周期职责。

本阶段加入轻量级资产管理基础设施，使用稳定的 `AssetHandle` 连接场景数据与运行期资源：

```text
assets 中的资源文件
    → AssetManager::ImportAsset()
    → 生成或复用 AssetHandle
    → 写入 AssetRegistry.yaml
    → 组件只保存 AssetHandle
    → 渲染时由 AssetManager 解析并缓存 Texture2D
```

### 必要性

直接在组件中保存 `Ref<Texture2D>` 存在以下问题：

1. 智能指针不能持久化到 YAML；
2. 场景复制后容易共享不应共享的运行期状态；
3. 同一路径可能被重复加载，浪费内存和显存；
4. 文件移动后，硬编码路径容易失效；
5. Material、Model、Shader 难以使用统一方式管理。

`AssetHandle` 基于 64 位 UUID。场景只记录稳定句柄，资源路径、类型和加载状态由资产系统管理，从而分离“场景引用什么”和“资源当前如何加载”。

### 核心结构

```cpp
using AssetHandle = UUID;

enum class AssetType
{
    None = 0,
    Texture2D,
    Model,
    Shader,
    Material
};

struct AssetMetadata
{
    AssetHandle Handle{ 0 };
    AssetType Type = AssetType::None;
    std::filesystem::path FilePath;
};
```

- `AssetHandle`：资源的稳定身份；
- `AssetType`：统一的资源分类；
- `AssetMetadata`：记录句柄、类型和相对路径，不持有 GPU 对象；
- `AssetManager`：维护注册表、路径索引和运行期纹理缓存。

### 导入与注册表流程

编辑器启动时以项目 `assets` 目录初始化 `AssetManager`，并读取 `assets/AssetRegistry.yaml`：

```text
ImportAsset(path)
    → 规范化绝对路径
    → 检查文件存在且位于 assets 内
    → 根据扩展名推断 AssetType
    → 查询路径是否已有句柄
        → 已存在：复用句柄
        → 不存在：生成新 AssetHandle
    → 更新内存注册表
    → 按稳定顺序写入 AssetRegistry.yaml
```

注册表采用 YAML，并只保存相对 assets 的路径：

```yaml
AssetRegistry:
  - Handle: 9195328290163695800
    Type: Texture2D
    FilePath: textures/balatro.png
```

这样不会把开发机器的绝对路径写入项目。重复启动和重复导入同一资源时会复用原句柄，稳定排序也能减少无意义的版本控制差异。

### SpriteRendererComponent 迁移

组件由直接保存纹理对象：

```cpp
Ref<Texture2D> Texture;
```

改为保存资产句柄：

```cpp
AssetHandle TextureHandle{ 0 };
```

Renderer2D 在 `DrawSprite` 中按需解析：

```text
SpriteRendererComponent::TextureHandle
    → AssetManager::GetTexture2D(handle)
    → 查询运行期缓存
        → 命中：复用 Texture2D
        → 未命中：根据 Metadata 加载并缓存
    → DrawQuad()
```

句柄无效、注册表中不存在或纹理加载失败时，渲染器回退为纯色 Quad，避免资源缺失导致场景崩溃。

### Properties 面板拖放

Sprite Renderer 属性现在支持：

- 显示当前纹理文件名；
- 使用 `X` 清除纹理句柄；
- 接收 Content Browser 文件拖放；
- 支持 PNG、JPG、JPEG、TGA 和 BMP，扩展名不区分大小写；
- 拖放后先导入资产，再把返回句柄写入组件。

属性面板只编辑 `TextureHandle`，不直接创建 OpenGL 纹理，从而保持编辑器 UI、资产管理和渲染后端低耦合。

### 场景序列化

`SpriteRendererComponent` 的 YAML 数据新增：

```yaml
SpriteRendererComponent:
  Color: [1.0, 1.0, 1.0, 1.0]
  Texture: 9195328290163695800
  TilingFactor: 1.0
```

保存时写入纹理句柄和 `TilingFactor`；加载时仅在字段存在时恢复，因此没有纹理字段的旧场景仍可加载。

| 数据位置 | 职责 |
|---|---|
| Scene YAML | 保存实体引用的 AssetHandle |
| AssetRegistry.yaml | 保存句柄到资源路径和类型的映射 |
| AssetManager 运行期缓存 | 保存已加载的 `Ref<Texture2D>` |

### 文件职责

```text
Glimmer/src/Glimmer/Asset/
  Asset.h                         资产句柄、类型与元数据
  AssetManager.h/.cpp             注册表、导入、查询和纹理缓存

Glimmer/src/Glimmer/Scene/
  Components.h                    SpriteRendererComponent 保存 TextureHandle
  SceneSerializer.cpp             纹理句柄与 TilingFactor 序列化

Glimmer/src/Glimmer/Renderer/
  Renderer2D.cpp                  绘制时解析纹理资产

GlimmerEditor-CyouBranch/src/
  EditorLayer.cpp                 初始化与关闭 AssetManager
  Panels/SceneHierarchyPanel.cpp  Sprite 纹理属性和拖放赋值

GlimmerEditor-CyouBranch/assets/
  AssetRegistry.yaml              项目资产注册表
```

资产系统属于引擎核心，因此放在 `Glimmer/src/Glimmer/Asset`；具体项目的注册表和资源文件属于项目数据，因此放在 `GlimmerEditor-CyouBranch/assets`。

### 验证结果

1. 使用 VS2026 重新生成工程；
2. `Debug | x64` 完整编译成功；
3. 连续启动后 `AssetRegistry.yaml` 内容及哈希保持不变；
4. 同一路径重复导入时复用原句柄；
5. `git diff --check` 通过；
6. 无效纹理句柄安全回退为纯色绘制。

### 当前边界与下一步

当前阶段已解决稳定引用、注册表和纹理缓存，但尚未包含：

1. 资源文件新增、删除、移动和重命名监视；
2. `.meta` 文件和资源导入配置；
3. 异步加载、后台导入和主线程 GPU 上传队列；
4. 资源依赖图、卸载和缺失资产修复；
5. Model、Shader、Material 的完整加载器。

基于该资产系统，下一步已经实现 `Material` 与 `MaterialComponent`。实体只引用材质句柄，材质资产再引用 Shader 和纹理句柄：

```text
Entity
    → MaterialComponent
    → Material AssetHandle
    → MaterialAsset
        → Shader AssetHandle
        → Albedo/Normal/Roughness 等 Texture AssetHandle
        → 可序列化材质参数
```

## Material 资产与实体材质组件

在 AssetHandle 基础设施完成后，材质不再是散落在 `EditorLayer` 或 Shader 调用附近的一组临时参数，而是成为可保存、可复用、可被实体稳定引用的项目资产。

本阶段建立以下数据链：

```text
Entity
    → MaterialComponent::MaterialHandle
    → AssetManager::GetMaterial()
    → Material（.glmat）
        → Shader AssetHandle
        → BaseColor Texture AssetHandle
        → BaseColor / Tiling / Metallic / Roughness
    → Renderer2D::DrawSprite()
```

### 设计目标与职责边界

材质系统需要同时处理两类数据：实体当前使用哪个材质，以及多个实体共享的材质参数。两者不应混在一个组件中，因此采用“轻组件、重资产”的结构：

```cpp
struct MaterialComponent
{
    AssetHandle MaterialHandle{ 0 };
    MaterialOverrides Overrides;
};
```

`MaterialComponent` 保存共享材质句柄和该实体的局部覆盖值。基础颜色、纹理和表面参数仍保存在 `.glmat` 文件及运行期 `Material` 对象中；只有显式启用的 Override 才写入场景组件。同一材质可以被多个实体引用，同时允许个别实体调整外观而不修改共享资产。组件仍不直接持有 Shader、Texture 或 OpenGL 对象。

### Material 核心结构

材质核心实现位于引擎 Renderer 模块：

```cpp
struct MaterialProperties
{
    glm::vec4 BaseColor{ 1.0f };
    AssetHandle BaseColorTexture{ 0 };
    float TilingFactor = 1.0f;
    float Metallic = 0.0f;
    float Roughness = 0.5f;
};

class Material
{
public:
    static Ref<Material> Create(const std::filesystem::path& path);

    bool Reload();
    bool Save() const;

    AssetHandle GetShaderHandle() const;
    MaterialProperties& GetProperties();
};
```

| 字段 | 作用 | 当前渲染状态 |
|---|---|---|
| `ShaderHandle` | 预留材质使用的 Shader 资产 | 尚未驱动渲染管线选择 |
| `BaseColor` | 基础颜色或纹理 Tint | Renderer2D 已使用 |
| `BaseColorTexture` | 基础颜色纹理句柄 | Renderer2D 已使用 |
| `TilingFactor` | UV 平铺倍率 | Renderer2D 已使用 |
| `Metallic` | 金属度 | 已保存，尚未参与 2D Shader |
| `Roughness` | 粗糙度 | 已保存，尚未参与 2D Shader |

Metallic 和 Roughness 目前是为后续 PBR 准备的数据结构，不代表 PBR 已完成。它们需要在后续 3D 材质 Shader、光源、HDR 和颜色空间流程中真正参与计算。

### `.glmat` 材质文件

材质使用 YAML 保存，扩展名为 `.glmat`：

```yaml
Material:
  Shader: 0
  BaseColor: [1.0, 0.2, 0.2, 1.0]
  BaseColorTexture: 0
  TilingFactor: 1.0
  Metallic: 0.0
  Roughness: 0.5
```

Shader 和纹理字段保存 AssetHandle，而不是绝对路径或运行期指针。具体路径继续由 `AssetRegistry.yaml` 管理。

加载流程：

```text
AssetManager::GetMaterial(handle)
    → 验证 Metadata 类型为 Material
    → 查询 MaterialCache
        → 已加载：复用缓存
        → 未加载：Material::Create(path)
    → 解析 .glmat
    → 校正参数范围
    → 放入 MaterialCache
```

加载时执行基础约束：

- `TilingFactor >= 0.01`；
- `Metallic` 限制到 `[0, 1]`；
- `Roughness` 限制到 `[0.04, 1]`。

解析失败时返回空材质，并且不会把无效对象放入缓存。

### AssetManager 材质缓存

AssetManager 新增独立的 Material 缓存：

```text
AssetHandle
    → AssetMetadata
    → .glmat 文件
    → Ref<Material>
```

Material 缓存保存材质参数，Texture 缓存保存运行期 Texture2D。Material 的纹理字段仍然只是 Texture AssetHandle，Renderer 使用材质时再解析纹理，因此 Material 不依赖 OpenGL 等具体图形 API。

### Renderer2D 兼容接入

为了不破坏已有 Sprite 场景，Renderer2D 使用兼容式覆盖：

```text
没有有效 Material
    → 使用 SpriteRendererComponent 的 Color、TextureHandle、TilingFactor

存在有效 Material
    → 使用 MaterialProperties 的 BaseColor、BaseColorTexture、TilingFactor
```

最终仍通过已有批处理 `DrawQuad` 提交。无效材质时保留 Sprite 数据，纹理无效时回退为纯色 Quad。

Metallic 和 Roughness 没有强行加入 Texture Shader，因为当前 Renderer2D 没有光照模型。后续应在 3D Material Pass 中通过 Material UBO 或其他跨 API 参数接口上传。

### Properties 面板工作流

选中带有 SpriteRendererComponent 的实体后，可以：

1. 点击 `+ Add Material` 添加 MaterialComponent；
2. 从 Content Browser 拖入 `.glmat`；
3. 查看当前材质文件名；
4. 编辑 Base Color、Tiling、Metallic 和 Roughness；
5. 将图片拖到 Base Color Texture；
6. 使用 `X` 清除材质或材质纹理。

实体 Inspector 编辑的是 `MaterialComponent::Overrides`，不会调用 `Material::Save()`，因此不会修改共享 `.glmat`。只有在 Content Browser 中选中材质资产后，Asset Inspector 才允许编辑基础材质并写回文件。层级面板使用 `[Mat]` 标记拥有 MaterialComponent 的实体。

### 场景复制与序列化

`Scene::Copy()` 已将 MaterialComponent 纳入组件复制列表。进入播放模式时，Runtime Scene 会保留实体 UUID 和 Material AssetHandle：

```text
Editor Scene
    → Scene::Copy()
    → Runtime Scene
    → 保留 UUID
    → 保留 MaterialHandle
```

Material 阶段曾将场景 YAML 升级为 `Version: 3`。后续加入光源组件后，当前场景格式已经升级为 `Version: 4`：

```yaml
MaterialComponent:
  Material: 13777784352782102236
  Overrides:
    Mask: 5
    BaseColor: [0.2, 0.7, 1.0, 1.0]
    BaseColorTexture: 0
    TilingFactor: 1.0
    Metallic: 0.0
    Roughness: 0.5
```

场景始终保存基础材质句柄；只有存在实体覆盖时才额外写出 `Overrides`。`Mask` 标记哪些字段真正覆盖基础材质，其余值不会参与合并。没有 `Overrides` 节点的旧场景仍按纯共享材质加载，因此保持向后兼容。

### 默认验证材质

项目新增：

```text
GlimmerEditor-CyouBranch/assets/materials/DefaultSprite.glmat
```

该材质使用与 Red Square 原始颜色一致的红色，并默认挂载到 Red Square。这样能够验证材质解析、场景组件和 Renderer2D 覆盖路径，同时保持原测试场景的视觉预期。

### 文件职责

```text
Glimmer/src/Glimmer/Renderer/
  Material.h/.cpp                  材质数据、YAML 加载、保存与重载
  Renderer2D.h/.cpp                材质参数解析与 Sprite 兼容绘制

Glimmer/src/Glimmer/Asset/
  AssetManager.h/.cpp              Material 缓存和句柄解析

Glimmer/src/Glimmer/Scene/
  Components.h                     MaterialComponent
  Scene.cpp                        组件复制和编辑/运行渲染传递
  SceneSerializer.cpp              MaterialComponent YAML 序列化

GlimmerEditor-CyouBranch/src/
  EditorLayer.cpp                  默认材质导入和测试实体挂载
  Panels/SceneHierarchyPanel.cpp   材质组件与参数编辑 UI

GlimmerEditor-CyouBranch/assets/
  materials/DefaultSprite.glmat    默认测试材质
  AssetRegistry.yaml               材质 AssetHandle 注册信息
```

### 验证结果

1. 从 `scripts` 目录运行 `Win-GenerateProject-vs2026.bat`，工程生成成功；
2. VS2026 `Debug | x64` 完整编译成功；
3. Material 源文件已进入核心静态库；
4. 编辑器运行期间未在材质解析或渲染路径提前退出；
5. `.glmat` 成功导入 AssetRegistry；
6. 重复启动后 AssetRegistry SHA256 保持一致；
7. `git diff --check` 通过。

### 历史边界与后续进展

本节建立 Material Asset 时，3D Material Pass、基础 PBR、MaterialInstance 和材质事务尚未完成。后续章节已经补齐这些能力：3D Renderer 会使用材质 ShaderHandle、BaseColor、Metallic 和 Roughness；实体 Override 与共享 `.glmat` 已分离；MaterialHandle、Overrides 和共享 Material Asset 编辑均已接入 Undo/Redo 与失败回滚。

当前仍待完成的是 Normal、AO、Emissive、Metallic-Roughness 等完整纹理通道，Material/Shader 参数布局反射，以及更严格的 sRGB/Linear 导入与验证。后续范围和验收条件以 `Documents/PROJECT_STATUS.md` 为准。

## 统一光源组件与 Light UBO

在 Material 资产和 ECS 引用完成后，下一项基础建设是统一光源数据。此前 `EditorLayer` 使用 `m_LightPos` 保存测试灯光位置，它不属于场景实体，无法序列化、无法随播放场景复制，也迫使每个 Shader 单独上传灯光 Uniform。

本阶段建立以下数据流：

```text
Scene Entity + Transform
    → DirectionalLightComponent / PointLightComponent
    → Scene::UploadLightEnvironment()
    → LightEnvironment
    → Renderer::UploadLightEnvironment()
    → binding 1 Light UBO
    → Terrain Shader
```

### 建设目标

统一光源系统需要满足：

1. 光源属于 Scene，而不是 EditorLayer 临时变量；
2. 光源能够保存、加载并复制到 Runtime Scene；
3. Shader 共享同一份光源数据，不逐个散传 Uniform；
4. 核心接口不暴露 OpenGL 调用；
5. CPU 与 GLSL 的 std140 数据布局可验证；
6. 为后续 Vulkan Descriptor Set 和 PBR 光照保留稳定结构。

### ECS 光源组件

新增两个场景组件：

```cpp
struct DirectionalLightComponent
{
    glm::vec3 Color{ 1.0f };
    float Intensity = 1.0f;
    float AmbientIntensity = 0.05f;
    bool Enabled = true;
};

struct PointLightComponent
{
    glm::vec3 Color{ 1.0f };
    float Intensity = 10.0f;
    float Range = 10.0f;
    bool Enabled = true;
};
```

光源空间属性继续由通用 `TransformComponent` 管理：

- 点光源位置来自 Translation；
- 方向光方向来自 Rotation；
- 方向光使用实体局部 `-Z` 轴作为前向方向；
- Scale 不作为灯光强度或范围参数。

这样 Gizmos 可以直接调整点光源位置和方向光朝向，无需重复实现变换系统。

### LightEnvironment

Renderer 模块新增与图形 API 无关的场景光照描述：

```cpp
struct DirectionalLight
{
    glm::vec3 Direction;
    glm::vec3 Color;
    float Intensity;
    float AmbientIntensity;
    bool Enabled;
};

struct PointLight
{
    glm::vec3 Position;
    glm::vec3 Color;
    float Intensity;
    float Range;
};

struct LightEnvironment
{
    static constexpr uint32_t MaxPointLights = 16;

    DirectionalLight Directional;
    std::vector<PointLight> PointLights;
};
```

当前支持一个有效方向光和最多 16 个有效点光源。Scene 每帧遍历组件，将 ECS 数据转换为 LightEnvironment，再交给 Renderer。

### Scene 收集流程

编辑模式和播放模式执行同一套收集逻辑：

```text
Scene::OnUpdateEditor() / Scene::OnUpdateRuntime()
    → UploadLightEnvironment()
    → 查找第一个 Enabled DirectionalLightComponent
    → 从 Transform 计算世界方向
    → 遍历 Enabled PointLightComponent
    → 限制到 MaxPointLights
    → Renderer::UploadLightEnvironment()
```

Scene 只负责把实体组件转换为通用光源数据，不创建 UBO，也不调用 OpenGL。

### std140 GPU 数据布局

Renderer 将 LightEnvironment 打包为固定尺寸 GPU 数据：

```cpp
struct alignas(16) GPUPointLight
{
    glm::vec4 PositionRange;
    glm::vec4 ColorIntensity;
};

struct alignas(16) GPULightEnvironment
{
    glm::vec4 DirectionIntensity;
    glm::vec4 DirectionalColor;
    glm::vec4 AmbientColorIntensity;
    glm::uvec4 LightCounts;
    GPUPointLight PointLights[16];
};
```

使用 vec4 打包可以避免 `vec3` 在 std140 中产生容易误判的填充：

| GPU 字段 | 内容 |
|---|---|
| `DirectionIntensity.xyz` | 方向光照射方向 |
| `DirectionIntensity.w` | 方向光强度 |
| `DirectionalColor.rgb` | 方向光颜色 |
| `AmbientColorIntensity.rgb` | 环境光颜色 |
| `AmbientColorIntensity.w` | 环境光强度 |
| `LightCounts.x` | 有效点光源数量 |
| `PositionRange.xyz` | 点光源世界位置 |
| `PositionRange.w` | 点光源范围 |
| `ColorIntensity.rgb` | 点光源颜色 |
| `ColorIntensity.w` | 点光源强度 |

CPU 端通过静态断言验证布局：

```cpp
static_assert(sizeof(GPUPointLight) == 32);
static_assert(sizeof(GPULightEnvironment) == 576);
```

### UniformBuffer 上传

Renderer 初始化时创建 binding 1 的 UniformBuffer：

```text
Renderer::Init()
    → UniformBuffer::Create(sizeof(GPULightEnvironment), 1)

每帧：
Renderer::UploadLightEnvironment()
    → 清零并打包 GPU 数据
    → 限制强度和范围的最小值
    → UniformBuffer::SetData()
```

binding 0 已由 Renderer2D Camera UBO 使用，因此 Light UBO 使用 binding 1。OpenGL 创建和上传细节仍封装在 `OpenGLUniformBuffer` 中。

如果场景没有有效方向光，系统保留强度为 `0.03` 的低环境光，避免旧场景完全黑屏，同时不会伪造直接光源。

### Terrain Shader 可视接入

当前帧流程中真正参与 3D 绘制的是 Terrain Pass，因此第一轮验证选择 Terrain Shader：

```glsl
layout(std140, binding = 1) uniform LightEnvironment
{
    vec4 u_DirectionalDirectionIntensity;
    vec4 u_DirectionalColor;
    vec4 u_AmbientColorIntensity;
    uvec4 u_LightCounts;
    PointLightData u_PointLights[16];
};
```

Terrain Shader 目前计算：

- 环境光；
- 方向光漫反射；
- Blinn-Phong 高光；
- 最多 16 个点光源；
- 点光源距离衰减；
- 点光源范围平滑衰减。

点光源衰减同时考虑距离平方和范围边界，超过 Range 的片元不再计算该光源。

### Properties 与默认场景

Properties 面板新增：

- `+ Directional Light`；
- `+ Point Light`；
- Enabled；
- Color；
- Intensity；
- Directional Ambient；
- Point Light Range。

层级面板使用 `[Sun]` 和 `[Point]` 标记光源实体。

默认场景新增两个验证实体：

```text
Sun
    Rotation = (-50, 30, 0)
    DirectionalLightComponent

Point Light
    Position = (0, 12, 0)
    Intensity = 80
    Range = 40
```

旧的 `EditorLayer::m_LightPos` 和 Settings 中独立的 Light Position 控件已删除，避免两套灯光数据来源。

### 场景复制与序列化

光源组件已加入 `Scene::Copy()`，进入播放模式时会保留灯光参数和 Transform。

当前场景格式升级为 `Version: 4`：

```yaml
DirectionalLightComponent:
  Color: [1.0, 1.0, 1.0]
  Intensity: 1.0
  AmbientIntensity: 0.05
  Enabled: true

PointLightComponent:
  Color: [1.0, 1.0, 1.0]
  Intensity: 80.0
  Range: 40.0
  Enabled: true
```

光源组件是可选字段，Version 1～3 场景仍可按原有兼容流程加载。

### 文件职责

```text
Glimmer/src/Glimmer/Renderer/
  LightEnvironment.h               跨 API 的方向光、点光源和场景光照数据
  Renderer.h/.cpp                  std140 打包、Light UBO 创建与上传
  UniformBuffer.h/.cpp             跨 API UniformBuffer 接口

Glimmer/src/Glimmer/Scene/
  Components.h                     ECS 光源组件
  Scene.h/.cpp                     光源收集、变换转换和 Runtime 复制
  SceneSerializer.cpp              Version 4 光源组件序列化

GlimmerEditor-CyouBranch/src/
  EditorLayer.cpp                  默认 Sun 和 Point Light 测试实体
  Panels/SceneHierarchyPanel.cpp   光源组件创建与属性编辑

GlimmerEditor-CyouBranch/assets/shaders/
  Terrain.glsl                     binding 1 Light UBO 可视验证
```

### 验证结果

1. 从 `scripts` 目录运行 VS2026 工程生成脚本成功；
2. `Debug | x64` 完整编译成功；
3. CPU 端 std140 尺寸断言通过；
4. Terrain Shader GLSL 450 编译成功；
5. binding 1 Light UBO 未触发版本或链接错误；
6. 编辑器运行 25 秒，Terrain、Compute、后处理及模型 Shader 均完成加载；
7. 没有 Shader assertion、访问异常或 OpenGL 错误；
8. `git diff --check` 通过。

### 当前边界与下一步

当前完成的是场景光源基础和 Terrain 可视验证，仍有以下边界：

1. 只选择第一个 Enabled 方向光；
2. 点光源固定上限为 16，尚未实现光源剔除或 Tiled/Clustered Lighting；
3. Phong、Toon、Blinn-Phong、Hologram Shader 尚未迁移到 Light UBO；
4. 当前帧流程没有实际调用 `Model::Draw()`；
5. 尚未实现 Spot Light、阴影、IBL 和环境贴图；
6. 尚未将 Camera 数据统一到 3D UBO；
7. 光源 Gizmos 目前复用 Transform 操作，尚无专用范围或方向图标。

下一步应建立真正的 3D Material Pass：恢复并规范模型提交路径，让 Material 的 ShaderHandle、BaseColor、Metallic、Roughness 与 LightEnvironment 在同一渲染流程中生效，再开始基础 PBR。

![[README.assets/Pasted image 20260727101225.png]]
![[README.assets/Pasted image 20260727101324.png]]

## 3D Material Pass 与基础 PBR

在统一光源组件和 Light UBO 完成后，本阶段将模型、材质、Shader 与场景实体接入同一条 3D 渲染流程，使 `Material` 中的 `BaseColor`、`Metallic` 和 `Roughness` 真正参与模型表面光照。

### 建设目标与数据流

旧流程由 `EditorLayer` 保存模型数组和全局 Shader 选择，模型无法稳定序列化，材质句柄也没有决定实际渲染管线。本阶段建立以下数据链：

```text
Scene Entity
    → TransformComponent
    → ModelRendererComponent::ModelHandle
    → MaterialComponent::MaterialHandle
    → AssetManager
        → Model
        → Material
            → ShaderHandle
            → BaseColorTexture
            → BaseColor / Metallic / Roughness
    → Renderer3D
    → LightEnvironment UBO
    → PBRModel.glsl
```

`ModelRendererComponent` 只保存 `AssetHandle`，不持有 Model、Mesh 或 OpenGL 对象。它已接入 `Scene::Copy()`、场景序列化、Properties 拖放、Hierarchy 标记以及 Editor/Runtime 更新路径。场景格式升级为 `Version: 5`：

```yaml
ModelRendererComponent:
  Model: 8553044135784550654
```

### Renderer3D

新增的 `Renderer3D` 位于引擎核心 `Glimmer/Renderer`：

```cpp
Renderer3D::BeginScene(viewProjection, cameraPosition);
Renderer3D::DrawModel(transform, modelHandle, materialHandle, entityID);
```

模型提交流程为：

```text
ModelHandle → AssetManager::GetModel()
MaterialHandle → AssetManager::GetMaterial()
ShaderHandle → AssetManager::GetShader()
    → Shader 热重载检查
    → 上传相机、Transform 和材质参数
    → 绑定材质纹理、Mesh 纹理或白纹理
    → 遍历 Mesh
    → RenderCommand::DrawIndexed()
```

`Model` 现在只负责导入和持有 Mesh，不再直接上传 Shader 参数或发出绘制命令，从而分离资源解析与渲染提交职责。

### 基础 Cook–Torrance PBR

新增 `assets/shaders/PBRModel.glsl`，支持：

- GGX 法线分布；
- Schlick-GGX 几何遮蔽；
- Schlick Fresnel；
- Lambert 漫反射；
- Metallic、Roughness 和 BaseColor；
- BaseColor 纹理与 TilingFactor；
- 一个方向光和最多 16 个点光源；
- 点光源距离及 Range 衰减；
- 线性 HDR 输出，Tone Mapping 和 Gamma 编码由独立显示 Pass 负责。

Shader 还会向整数颜色附件输出实体 ID，保证 3D 模型可参与 Mouse Picking：

```glsl
layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;
```

### 默认验证资产

新增项目测试资源：

```text
assets/shaders/PBRModel.glsl
assets/materials/DefaultPBR.glmat
assets/models/suzanne.obj
```

编辑器启动时导入这三类资产，把稳定句柄写入 `AssetRegistry.yaml`，并创建默认 `PBR Suzanne` 实体：

```text
PBR Suzanne
    TransformComponent
    ModelRendererComponent → suzanne.obj
    MaterialComponent      → DefaultPBR.glmat
```

如果默认材质尚未绑定 PBR Shader，初始化流程会设置 ShaderHandle 并写回 `.glmat`。

### EditorLayer 清理

以下旧状态和入口已删除：

- `m_3DShader`；
- `m_Models`、`m_ModelNames` 和模型索引；
- 全局模型与 Shader 下拉框；
- 启动时重复构造五个 Model 对象的逻辑。

现在模型、材质和 Shader 均由实体组件与 Properties 面板驱动，编辑器层不再拥有第二套 3D 资源管理路径。

### 地形位置的当前处理

下列组件化结构已经实现，不再是未来方案：

```text
Terrain Entity
    → TransformComponent
    → TerrainComponent
        → HeightMapHandle
        → Generation Parameters
        → MaterialHandle
    → TerrainRenderer
```

`TransformComponent` 现在统一控制地形位置、旋转和实体缩放；地形覆盖范围与高度倍率继续由 `TerrainSpecification` 描述，最终在 TerrainRenderer 中与实体 Transform 合成。非均匀缩放时法线必须通过逆转置矩阵变换，否则光照会失真。

### 文件职责

```text
Glimmer/src/Glimmer/Asset/
  AssetManager.h/.cpp              Model、Shader 缓存与句柄解析
Glimmer/src/Glimmer/Renderer/
  Renderer3D.h/.cpp                3D 场景状态、材质解析与模型提交
  Model.h/.cpp                     模型导入和 Mesh 集合
  Material.h/.cpp                  可序列化材质资产
  LightEnvironment.h               跨 API 场景灯光数据
Glimmer/src/Glimmer/Scene/
  Components.h                     ModelRendererComponent
  Scene.cpp                        Editor/Runtime 3D 实体提交
  SceneSerializer.cpp              Version 5 模型组件序列化
GlimmerEditor-CyouBranch/assets/
  shaders/PBRModel.glsl            基础 Cook–Torrance PBR
  materials/DefaultPBR.glmat       默认 PBR 材质
  AssetRegistry.yaml               Model、Material、Shader 稳定句柄
```

### 验证结果

1. VS2026 Premake 工程生成成功；
2. `Debug | x64` 核心库编译成功；
3. `Debug | x64` 编辑器编译和链接成功；
4. PBR Shader、材质和 Suzanne 模型成功导入并持久化句柄；
5. `DefaultPBR.glmat` 成功保存 PBR ShaderHandle；
6. 编辑器完成运行初始化；
7. `git diff --check` 通过。

### 当前边界与下一步

当前完成的是基础 Forward PBR 验证链，仍有以下边界：

1. HDR Render Target 已在下一章节完成，场景主颜色附件升级为 RGBA16F；
2. Tone Mapping 已从模型 Shader 迁移到独立显示 Pass；
3. Texture2D 尚未区分 sRGB 颜色纹理与线性数据纹理；
4. 尚未支持 Normal、Metallic、Roughness、AO 纹理；
5. 尚未实现 IBL、环境贴图、阴影和反射探针；
6. Renderer3D 仍为逐 Mesh 即时提交；
7. Terrain 尚未进入统一的实体化 3D Material Pass。

HDR Framebuffer 与独立 Tone Mapping 已在下一章节完成。后续应建立 TextureColorSpace 与 TextureCube 接口，再接入天空盒、PBR 纹理集和 IBL。

![[README.assets/Pasted image 20260727111039.png]]
![[README.assets/Pasted image 20260727111113.png]]

## 线性 HDR 场景缓冲与独立 Tone Mapping

基础 PBR 验证完成后，原有帧图仍把场景直接绘制到 `RGBA8`，并允许在关闭后处理时直接把场景附件交给 ImGui。该流程会在光照计算结束后立即截断超过 `1.0` 的高亮信息，也让每个材质 Shader 被迫各自处理 Tone Mapping 和 Gamma 编码。

本阶段将颜色流程重构为：

```text
线性场景渲染
    → RGBA16F Scene Framebuffer
        → 3D PBR
        → Renderer2D
        → Terrain
        → 未来 Skybox
    → 固定 Tone Mapping Pass
        → Exposure
        → ACES Filmic
        → 可选 Grayscale
        → Gamma Encode
    → RGBA8 Display Framebuffer
    → ImGui Viewport
```

### 为什么需要 HDR 场景目标

`RGBA8` 每个通道只有 8 位归一化范围，写入时只能保存 `[0, 1]`。PBR 中点光源、高光和未来天空太阳区域经常产生大于 `1.0` 的辐射亮度。如果直接写入 RGBA8，这些值会提前被裁剪，后处理阶段无法区分普通白色和极亮区域。

场景主颜色附件现改为半精度浮点：

```cpp
FramebufferSpecification sceneFramebufferSpec;
sceneFramebufferSpec.Attachments = {
    { FramebufferTextureFormat::RGBA16F },
    { FramebufferTextureFormat::RED_INTEGER }
};
```

附件职责保持明确：

| 附件 | 格式 | 用途 |
|---|---|---|
| Scene Color 0 | `RGBA16F` | 保存线性 HDR 场景颜色 |
| Scene Color 1 | `RED_INTEGER` | 保存 Mouse Picking 实体 ID |
| Scene Depth | `Depth24Stencil8` | 深度和模板测试 |

最终显示目标使用独立规格：

```cpp
FramebufferSpecification displayFramebufferSpec;
displayFramebufferSpec.Attachments = {
    { FramebufferTextureFormat::RGBA8 }
};
```

`m_PostProcessFB` 同时承担“任意后处理”和“最终显示目标”两种含义，容易混淆，因此重命名为 `m_DisplayFramebuffer`。

### 固定 Tone Mapping Pass

HDR 场景不能直接显示到普通 RGBA8 Viewport，因此 Tone Mapping 不再是可整体关闭的视觉特效，而是帧图中固定存在的颜色空间转换步骤：

```cpp
RenderPassSpecification toneMappingPass;
toneMappingPass.Target = m_DisplayFramebuffer;
RenderPass::Begin(toneMappingPass);

Renderer2D::DrawPostProcess(
    toneMappingShader,
    m_Framebuffer->GetColorAttachmentRendererID());

RenderPass::End();
m_FinalSceneTexture =
    m_DisplayFramebuffer->GetColorAttachmentRendererID();
```

原来的开关逻辑为：

```text
开启后处理 → PostProcess FBO
关闭后处理 → 直接显示 Scene FBO
```

现在统一为：

```text
Scene RGBA16F
    → ToneMapping.glsl
    → Display RGBA8
    → Viewport
```

这保证所有显示路径都经过相同的曝光、Tone Mapping 和 Gamma 编码，避免切换开关时画面亮度和颜色空间发生突变。

### ToneMapping Shader

新增 `assets/shaders/ToneMapping.glsl`，集中负责 HDR 到显示空间的转换。

核心流程：

```glsl
float exposureMultiplier = exp2(u_ExposureEV);
vec3 hdrColor = max(sceneColor.rgb, vec3(0.0)) * exposureMultiplier;
vec3 mappedColor = ACESFilm(hdrColor) / ACESFilm(vec3(u_ACESWhitePoint)).r;
vec3 displayColor = pow(mappedColor, vec3(1.0 / 2.2));
```

当前采用 ACES Filmic 近似曲线。相比简单 Reinhard，ACES 能保留更自然的中间调和高光过渡，并为后续天空盒、太阳高亮、Bloom 和曝光控制提供更稳定的显示基础。

Shader 参数：

| Uniform | 作用 |
|---|---|
| `u_SceneTexture` | RGBA16F 场景颜色附件 |
| `u_ExposureEV` | 摄影式曝光档位；每增加 1 EV，线性亮度翻倍 |
| `u_ACESWhitePoint` | ACES 拟合曲线的显示白点归一参考 |
| `u_ApplyGrayscale` | Tone Mapping 后、Gamma 前应用可选灰度效果 |

灰度不再代表整个后处理是否启用，而只是 Tone Mapping Pass 中的一个可选效果。

### 线性输出约定

PBR Shader 已删除内部 Reinhard 和 Gamma 编码：

```glsl
o_Color = vec4(max(result, vec3(0.0)), alpha);
```

它现在只输出线性 HDR 光照结果。Tone Mapping 不属于单个材质，不能分别散落在 PBR、Terrain、Sprite 和未来 Skybox Shader 中。

当前其他场景 Shader 的处理：

- `Texture.glsl`：将颜色纹理和 Tint 的结果近似按 Gamma 2.2 解码到线性空间；
- `Terrain.glsl`：将 Grass、Rock、Snow 调色板颜色转换到线性空间后再参与光照；
- `PBRModel.glsl`：在线性空间执行 BRDF，直接输出 HDR Radiance；
- `ToneMapping.glsl`：唯一负责 Tone Mapping 和 Gamma 编码的显示 Shader。

当前纹理仍使用普通 `RGBA8` GPU 内部格式，因此 Sprite 和 PBR BaseColor 采用 Shader 手工解码。这是可运行的过渡方案，不代表完整的纹理颜色空间资产系统。

### 后端无关的纹理绑定

旧 `Renderer2D::DrawPostProcess()` 使用：

```cpp
std::dynamic_pointer_cast<OpenGLShader>(shader)->BindTexture(...);
```

这让引擎核心 Renderer2D 直接依赖 OpenGL 后端。现在改为：

```cpp
shader->BindTexture("u_SceneTexture", 0, inputTextureID);
```

具体后端通过 `Shader` 虚接口实现纹理绑定。未来 Vulkan 后端可以用 Descriptor Set 实现同一操作，而 Renderer2D 和 Tone Mapping Pass 不需要修改。

### 编辑器控制

Settings 面板新增：

```text
HDR Output
    Exposure (EV)     -10 ～ +10
    ACES White Point  1.0 ～ 32.0
    Grayscale  On / Off
```

`Exposure (EV)` 控制进入 ACES 曲线前的线性亮度倍率，换算关系为 `multiplier = 2^EV`：`0 EV = 1×`、`+1 EV = 2×`、`-1 EV = 0.5×`。它不是灯光强度的替代品：灯光 Intensity 描述场景照明，EV 描述观察和显示映射。`ACES White Point` 将曲线在指定线性亮度处的响应归一为显示白，默认 `11.2`；调低会更早压缩高光，调高会保留更宽的高光范围。

### 天空盒与 IBL 后续关系

TextureCube、Cubemap 资产和可见 Skybox Pass 已在后续章节完成；尚未实现的 Diffuse/Specular IBL 与派生缓存已纳入 `Documents/PROJECT_STATUS.md` 的 P10：

```text
HDR + 基础 PBR
    → TextureCube / Cubemap 资源抽象
    → 可见 Skybox Pass
    → Diffuse Irradiance Map
    → Specular Prefilter Map
    → BRDF LUT
    → 完整 IBL
```

这个顺序的必要性：

1. 天空盒通常包含太阳和高亮环境区域，必须写入 RGBA16F；
2. 天空盒应和场景几何统一经过 Tone Mapping；
3. 可见天空盒和 IBL 应复用同一 Cubemap 资产；
4. 先验证 Cubemap 导入与采样，再生成 Irradiance 和 Prefilter，便于分层排错；
5. TextureCube 的创建必须位于跨 API Texture 抽象和平台后端，不能在 `EditorLayer` 直接调用 OpenGL。

计划中的可见 Skybox Pass 将在不清除已有几何的情况下绘制，并使用移除平移的 View 矩阵，使天空盒只随相机旋转、不随相机位置移动。

### 文件职责

```text
Glimmer/src/Glimmer/Renderer/
  FrameBuffer.h                     RGBA16F 跨 API 附件格式
  Renderer2D.cpp                    全屏绘制与跨 API 输入纹理绑定

Glimmer/src/Platform/OpenGL/
  OpenGLFramebuffer.cpp             GL_RGBA16F 创建、调整尺寸和采样

GlimmerEditor-CyouBranch/src/
  EditorLayer.h                     Display FBO、Exposure EV、White Point、Grayscale 状态
  EditorLayer.cpp                   HDR Scene Pass 与固定 Tone Mapping Pass

GlimmerEditor-CyouBranch/assets/shaders/
  PBRModel.glsl                     线性 HDR PBR 输出
  Texture.glsl                      Sprite 颜色线性化
  Terrain.glsl                      地形调色板线性化
  ToneMapping.glsl                  EV、ACES White Point、Gamma 与可选灰度

Documents/
  PROJECT_STATUS.md                  IBL 后续顺序与验收条件
```

![[README.assets/Pasted image 20260727115626.png]]

### 验证结果

1. `FramebufferTextureFormat::RGBA16F` 已由 OpenGL 后端映射到 `GL_RGBA16F`；
2. Scene FBO 使用 RGBA16F 与 RED_INTEGER 两个颜色附件；
3. Display FBO 使用 RGBA8；
4. VS2026 Premake 工程生成成功；
5. `Debug | x64` 编辑器编译和链接成功；
6. `ToneMapping.glsl` 运行期编译成功；
7. 编辑器持续运行 45 秒，没有 Framebuffer assertion、Shader assertion 或初始化崩溃；
8. 测试结束后相关编辑器和 MSBuild 进程已清理；
9. `git diff --check` 通过。

### 当前边界与下一步

当前 HDR 显示链已经建立，但颜色空间资产基础仍需继续完善：

1. Texture2D 尚未保存 `sRGB`、`Linear` 等颜色空间元数据；
2. 颜色纹理目前由 Shader 手工 Gamma 解码；
3. Normal、Height、Roughness 等数据纹理必须保持线性，不能套用颜色解码；
4. 尚未提供 TextureCube 跨 API 接口；
5. 尚未实现自动曝光、Bloom 和 HDR 调试视图；
6. Terrain 与 Sprite 的颜色空间仍是过渡实现；
7. Display FBO 目前会创建不必要的默认深度附件，后续可在 Framebuffer 规格中允许显式禁用。

下一步建议先加入 `TextureColorSpace` 和纹理用途元数据，再实现 `TextureCube` 与可见天空盒。完成 Cubemap 资源链后，再进入 Irradiance、Prefilter 和 BRDF LUT，建立完整 IBL。

## 天空盒与 SkyLight 资产化

### 实现结果

天空盒已从 `EditorLayer` 内部测试纹理迁移为正式场景资产链：

1. `TextureCube` 表示后端无关的 GPU 立方体纹理；
2. `.glsky` 描述六个方向的图片；
3. `Cubemap` 解析描述并创建 `TextureCube`；
4. `AssetManager` 为 `.glsky` 分配 `AssetHandle` 并缓存资源；
5. `SkyLightComponent` 将 Cubemap 挂载到场景实体；
6. Sky Light 可随 `.glimmer` 保存、加载并复制到播放场景；
7. `.glsky` 可拖到 Properties 或直接拖入 Viewport；
8. 天空盒写入 HDR Scene Framebuffer，并统一经过 Tone Mapping。

默认新场景只创建 `Sun`、`Point Light` 和 `Sky Light`。之前的彩色方块、Suzanne、逻辑节点与测试实体相机已经移除。

### 文件与职责

```text
Glimmer/src/Glimmer/
├─ Asset/AssetManager.*       .glsky 导入、句柄与 Cubemap 缓存
├─ Renderer/TextureCube.*     通用立方体纹理接口
├─ Renderer/Cubemap.*         .glsky 解析与六面图片导入
├─ Renderer/SkyboxRenderer.*  天空盒绘制
└─ Scene/
   ├─ Components.h            SkyLightComponent
   ├─ Scene.*                 Sky Light 查询与场景复制
   └─ SceneSerializer.cpp     Sky Light 场景序列化

Glimmer/src/Platform/OpenGL/OpenGLTextureCube.*
GlimmerEditor-CyouBranch/assets/skyboxes/*.glsky
GlimmerEditor-CyouBranch/assets/textures/skybox/<name>/*
```

核心库只保存通用资产、渲染和场景能力；示例资源、Shader 与编辑器拖放逻辑保留在编辑器项目。

### `.glsky` 描述格式

推荐目录：

```text
assets/
├─ skyboxes/sunset.glsky
└─ textures/skybox/sunset/
   ├─ right.jpg
   ├─ left.jpg
   ├─ top.jpg
   ├─ bottom.jpg
   ├─ front.jpg
   └─ back.jpg
```

标准描述：

```yaml
Cubemap:
  ColorSpace: SRGB
  Right: ../textures/skybox/sunset/right.jpg
  Left: ../textures/skybox/sunset/left.jpg
  Top: ../textures/skybox/sunset/top.jpg
  Bottom: ../textures/skybox/sunset/bottom.jpg
  Front: ../textures/skybox/sunset/front.jpg
  Back: ../textures/skybox/sunset/back.jpg
  MissingFaceColor: [20, 20, 20, 255]
```

图片路径相对于 `.glsky` 所在目录解析，不依赖可执行文件目录或本机绝对路径。普通 JPG/PNG 天空照片使用 `SRGB`，只有明确存储线性数据的图片才使用 `Linear`。

六面映射：

| 字段 | 方向 | OpenGL 面 |
|---|---|---|
| `Right` | +X | `GL_TEXTURE_CUBE_MAP_POSITIVE_X` |
| `Left` | -X | `GL_TEXTURE_CUBE_MAP_NEGATIVE_X` |
| `Top` | +Y | `GL_TEXTURE_CUBE_MAP_POSITIVE_Y` |
| `Bottom` | -Y | `GL_TEXTURE_CUBE_MAP_NEGATIVE_Y` |
| `Front` | +Z | `GL_TEXTURE_CUBE_MAP_POSITIVE_Z` |
| `Back` | -Z | `GL_TEXTURE_CUBE_MAP_NEGATIVE_Z` |

当前 `desert-evening` 示例只有五面，因此使用：

```yaml
Bottom: ""
MissingFaceColor: [52, 38, 26, 255]
```

加载器会在运行时生成同分辨率的纯色底面，不修改原始图片。

### 六面图片导入流程

```text
.glsky
  → AssetManager::ImportAsset()
  → AssetType::Cubemap + AssetHandle
  → AssetManager::GetCubemap()
  → Cubemap::Reload()
  → 解析路径与颜色空间
  → stb_image 解码为 RGBA
  → 检查正方形和尺寸一致性
  → TextureCube::Create() / SetFaceData()
  → OpenGLTextureCube 上传六面
```

导入规则：

- 至少存在一个有效图片面；
- 每张有效图片必须为正方形；
- 所有有效图片分辨率必须一致；
- 空路径使用 `MissingFaceColor`；
- 错误路径会停止创建并输出日志，不会静默回退；
- 同一 `.glsky` 被多个实体引用时复用 AssetManager 缓存，不重复创建 GPU 纹理。

### `SkyLightComponent`

```cpp
struct SkyLightComponent
{
    AssetHandle CubemapHandle{ 0 };
    float Intensity = 1.0f;
    bool Enabled = true;
};
```

- `CubemapHandle` 指向 `AssetType::Cubemap`；
- `Intensity` 在 HDR Tone Mapping 之前调节天空盒强度；
- `Enabled` 控制组件是否参与天空盒查询与绘制。

`Scene::GetSkyLightEntity()` 返回第一个启用的 Sky Light。当前场景建议只保留一个主 Sky Light，多环境混合尚未实现。`Scene::Copy()` 会复制组件，因此编辑场景与播放场景共享只读 Cubemap 资产，但组件状态相互独立。

### 场景序列化

场景格式已提升到 `Version: 6`：

```yaml
SkyLightComponent:
  Cubemap: 14395647676425568118
  Intensity: 1.0
  Enabled: true
```

场景保存 Asset Handle，而不是绝对路径。实际文件由 `AssetRegistry.yaml` 解析：

```yaml
- Handle: 14395647676425568118
  Type: Cubemap
  FilePath: skyboxes/desert-evening.glsky
```

因此 `.glimmer`、`.glsky` 和 `AssetRegistry.yaml` 必须保持一致，不要随意修改已被场景引用的 Handle。

### 编辑器操作

Properties 工作流：

1. 创建或选中实体；
2. 点击 `+ Sky Light`；
3. 从 Content Browser 将 `.glsky` 拖到 Cubemap 属性；
4. 调整 `Enabled` 和 `Intensity`；
5. 保存 `.glimmer`。

也可以直接将 `.glsky` 拖入 Viewport：

- 有选中实体：为其添加 Sky Light 或替换 Cubemap；
- 无选中实体：自动创建 `Sky Light` 实体；
- 完成后自动选中目标实体。

Content Browser 继续使用统一的 `SCENE_FILE` payload 传递路径；接收端导入后，组件只保存 Asset Handle。

### 渲染流程

```text
EditorCamera / Primary Camera
  → Scene::GetSkyLightEntity()
  → AssetManager::GetCubemap(handle)
  → SkyboxRenderer::Draw()
  → HDR Scene FBO (RGBA16F)
  → ToneMapping.glsl
  → Display FBO (RGBA8)
  → ImGui Viewport
```

天空盒 View 矩阵移除平移，因此只随相机旋转；顶点输出使用 `xyww` 将深度放到远平面；绘制时深度函数临时切换为 `LessEqual`，结束后恢复 `Less`。天空盒进入 RGBA16F，与地形和模型统一经过曝光及 Tone Mapping。

### 当前限制与下一步

当前完成的是可见天空盒与场景资产链，完整 IBL 仍需：

1. HDR/EXR 浮点 Cubemap 导入；
2. Diffuse Irradiance Map；
3. Specular Prefilter Map；
4. BRDF LUT；
5. PBR Shader 接入环境光与反射；
6. 多 Sky Light 混合或空间环境探针；
7. `.glsky` 资产热重载；
8. 等距柱状图自动转换为 Cubemap。

建议顺序：

```text
HDR Cubemap
  → Irradiance Convolution
  → Specular Prefilter
  → BRDF LUT
  → PBR IBL
  → Sky Light 热重载与环境探针
```

### 验证结果

- VS2026 Premake 工程生成成功；
- `Debug | x64` 编译和链接成功；
- `.glsky` 正确注册为 `AssetType::Cubemap`；
- 五面图片加缺失面回退可创建完整 TextureCube；
- 编辑器运行无 Shader、Framebuffer 或 Cubemap 断言；
- 移除测试实体后编辑器仍正常启动；
- `git diff --check` 通过。

![[README.assets/Pasted image 20260727151210.png]]
属性面板整改后
![[README.assets/Pasted image 20260727170815.png]]

## 编辑器基础收口

### 建设目标

随着 Terrain、Material、Light 和环境组件持续增加，实体选择、属性编辑和撤销逻辑如果继续堆积在 `EditorLayer` 或层级面板中，会使面板之间互相依赖，也会让后续组件难以复用统一的编辑行为。本阶段先收口编辑器基础设施，使场景参数继续组件化时不必重复实现选择与命令逻辑。

本轮主要完成：

- `SceneHierarchyPanel` 与 `InspectorPanel` 分离显示职责；
- 使用统一 `SelectionContext` 管理 Entity/Asset 选择；
- 建立 `EditorCommandHistory` 与快捷键入口；
- 为实体生命周期建立基于 UUID 的快照和恢复流程；
- 将实体创建、复制、删除和 Add Component 接入 Undo/Redo；
- 让通用 `DrawComponent<T>` 的 Reset/Remove 操作具备命令化能力；
- 隔离 Edit Scene 与 Runtime Scene 的命令历史。

### SelectionContext

`SelectionContext` 只允许一种有效选择类型：

```text
None
Entity -> 保存当前 Entity，清空 AssetHandle
Asset  -> 保存当前 AssetHandle，清空 Entity
```

层级面板选中实体时调用 `SelectEntity`，内容浏览器选中资源时调用 `SelectAsset`。因此 Entity 与 Asset 不会在 Inspector 中同时残留，Inspector 也不需要读取 `EditorLayer` 的私有选中字段。

Inspector 根据选择类型进行分流：

- Entity：显示实体名称与组件属性；
- Asset：显示资源文件名、Handle、路径和资源类型；
- None：显示未选择提示；
- 已失效对象：显示失效提示，不继续访问组件。

### CommandHistory

编辑器命令统一实现 `IEditorCommand`：

```cpp
class IEditorCommand
{
public:
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual const char* GetName() const = 0;
};
```

`EditorCommandHistory` 维护 Undo Stack 与 Redo Stack：

1. `Execute` 执行新命令，将命令压入 Undo Stack，并清空 Redo Stack；
2. `Undo` 调用命令的反向操作，再将命令移动到 Redo Stack；
3. `Redo` 重新执行命令，再将命令放回 Undo Stack；
4. 打开新场景或替换编辑场景时清空历史，防止旧命令继续持有旧 Scene。

对于 ImGui 拖动控件，属性会在拖动过程中实时更新，因此使用 `PushExecuted` 在控件结束编辑时记录“已发生”的操作，避免每一帧产生一条命令。Transform、Terrain、Directional/Point/Sky Light、Camera 与 Material 连续属性目前都采用该事务边界。

快捷键如下：

| 操作 | 快捷键 |
| --- | --- |
| Undo | `Ctrl+Z` |
| Redo | `Ctrl+Y` |
| Redo | `Ctrl+Shift+Z` |

### EntitySnapshot 与 UUID 恢复

不能在 Undo 命令中长期保存 `entt::entity`：实体销毁后 Handle 已失效，而且 Handle 可能被 registry 重新利用。因此实体命令使用 `EntitySnapshot` 保存稳定 UUID 和可序列化组件数据。

当前快照覆盖：

- `TagComponent`、`TransformComponent`；
- Sprite、Model 与 Material 组件；
- `TerrainComponent`；
- Directional Light、Point Light 与 Sky Light；
- `CameraComponent`；
- Native Script 的实例化/销毁函数绑定。

恢复流程为：

```text
Undo/Redo
  -> Scene::FindEntityByUUID
  -> 若不存在则 CreateEntityWithUUID
  -> 恢复基础组件
  -> 恢复可选渲染、地形、光照和相机组件
  -> 重新同步 SelectionContext
```

`TerrainComponent` 的复制构造与复制赋值都只复制 `TerrainSpecification`，不会复用旧的 `TerrainRuntime` GPU 对象。地形复制、场景切换或命令恢复后由 `TerrainRenderer` 按需重新建立运行时资源。

Native Script 快照只保存脚本工厂绑定，不复制正在运行的 `ScriptableEntity::Instance`，避免两个实体共同持有同一个脚本实例。

### 实体生命周期命令

以下层级面板操作已接入命令历史：

- Create Entity：Undo 销毁新实体，Redo 使用原 UUID 恢复；
- Duplicate Entity：保存复制结果的完整快照，Undo/Redo 不重新随机生成 UUID；
- Delete Entity：删除前捕获快照，Undo 恢复组件和选择状态；
- Add Component：保存组件初始值，Undo 移除，Redo 恢复。

命令内部每次操作前都会通过 UUID 重新查找实体，并检查组件是否存在，避免使用已经失效的 Entity Handle 或重复添加/删除组件。

### 通用组件菜单

`InspectorPanel::DrawComponent<T>` 统一提供组件标题、折叠内容和右键菜单。Reset 与 Remove Component 不再直接修改 registry，而是创建 `LambdaEditorCommand`：

- Reset 保存修改前组件和默认组件，支持双向恢复；
- Remove 在删除前复制组件，Undo 时使用原始数据重新添加；
- Transform 不允许删除，但可以 Reset；
- 组件命令只持有 `Ref<Scene>`、UUID 和值快照，不保存组件引用。

目前 Add Component 已覆盖 Camera、Sprite Renderer、Model Renderer、Material、Terrain、Directional Light、Point Light 和 Sky Light。其余仍使用手写 TreeNode 的属性区域会在下一步迁移到 `DrawComponent<T>`，迁移后即可统一显示 Reset/Remove 菜单。

### Edit/Play 模式边界

进入 Play 模式后，`SceneHierarchyPanel` 和 `InspectorPanel` 会切换到 Runtime Scene。为防止编辑器历史混入临时运行时对象，本轮增加以下约束：

- Play 时暂时向两个面板注入空 CommandHistory；
- Play 时禁用全局 Undo/Redo 快捷键；
- Stop 后重新绑定 Editor Scene 的 CommandHistory；
- 编辑场景原有历史不会捕获或长期持有 Runtime Scene。

这样运行时调试修改仍然是临时状态，停止播放后恢复编辑场景，不会出现 Undo 修改错误 Scene 的情况。

### 文件职责

| 文件 | 职责 |
| --- | --- |
| `src/Editor/EditorCommand.h/.cpp` | 命令接口、双栈历史、实体快照与恢复 |
| `src/Panels/SelectionContext.h` | Entity/Asset 互斥选择状态 |
| `src/Panels/SceneHierarchyPanel.h/.cpp` | 实体列表与实体生命周期命令入口 |
| `src/Panels/InspectorPanel.h/.cpp` | 选择分流、通用组件 UI 与组件命令入口 |
| `src/EditorLayer.cpp` | 面板依赖注入、快捷键和 Edit/Play 状态编排 |

这些文件均位于 `GlimmerEditor-CyouBranch`，属于编辑器工作流，不进入核心 `Glimmer` 运行时库。Scene、Entity、UUID 和组件数据仍由核心库提供。

### 验证结果

- VS2026 `Debug | x64` 编译和链接成功；
- 通过已打开的 Glimmer VS 实例构建，失败项目为 0；
- 连续第二次 VS 热构建耗时约 0.1 秒，增量状态正常；
- `git diff --check` 通过；
- 未运行 Premake，未删除 `bin` 或 `bin-int`；
- Entity/Asset 选择互斥；
- 实体创建、复制、删除与组件添加可进入 Undo/Redo 历史；
- 播放模式不会执行编辑场景 Undo/Redo。

建议交互验证顺序：创建实体 -> 添加 Terrain 或 Light -> `Ctrl+Z` 连续撤销 -> `Ctrl+Y` 连续恢复；随后删除实体并撤销，确认 UUID、组件和 Inspector 选择状态均恢复。

### 当前边界与下一步

当前命令系统已经覆盖实体和组件结构变化，但属性事务尚未全部统一：

- Transform 连续拖动已经只生成一条命令；
- Terrain、Directional Light、Point Light、Sky Light 和普通组件属性仍需统一接入属性事务；
- `.glmat` 修改的是共享 Material Asset，需要建立 Asset Command，而不是错误地当作实体组件值处理；
- Gizmos Transform 操作也应在开始拖动时快照、结束拖动时提交一条命令；
- 后续可增加历史容量限制、命令合并和保存后的 Dirty 标记。

下一步将抽取通用 Inspector 属性编辑事务，先覆盖 Terrain 与 Light，再单独建设 Material Asset 的保存、Undo 和 Redo 流程。

## MaterialInstance 与实体材质 Override

### 建设目的

此前多个实体引用同一个 `.glmat` 时，实体 Inspector 直接修改 `Material::GetProperties()` 并调用 `Material::Save()`。由于 `AssetManager` 会按 `AssetHandle` 缓存并共享同一个 `Material` 对象，对任意实体调整颜色、纹理、金属度或粗糙度都会修改原始材质资产，并同步影响所有引用它的实体。

本阶段将材质数据拆成两层：

```text
Material Asset（.glmat，共享基础值）
    -> MaterialComponent::MaterialHandle
    -> MaterialOverrides（实体局部值）
    -> MaterialInstance（运行时合并结果）
    -> Renderer2D / Renderer3D
```

这样既保留 `.glmat` 的复用能力，也允许实体拥有局部外观差异。`MaterialInstance` 不复制 Shader、Texture 或 GPU 对象，只在提交渲染时解析最终参数。

### 核心数据结构

`MaterialOverride` 使用位掩码标记单个属性是否覆盖基础材质：

```cpp
enum class MaterialOverride : uint32_t
{
    None             = 0,
    BaseColor        = 1 << 0,
    BaseColorTexture = 1 << 1,
    TilingFactor     = 1 << 2,
    Metallic         = 1 << 3,
    Roughness        = 1 << 4
};

struct MaterialOverrides
{
    uint32_t Mask = 0;
    MaterialProperties Values;

    bool IsEnabled(MaterialOverride property) const;
    void SetEnabled(MaterialOverride property, bool enabled);
    void Clear();
    bool Empty() const;
};
```

当前允许实体覆盖：

| 属性 | 用途 |
| --- | --- |
| `BaseColor` | 实体局部基础颜色或 Tint |
| `BaseColorTexture` | 实体局部基础颜色纹理 |
| `TilingFactor` | 实体局部 UV 平铺倍率 |
| `Metallic` | 实体局部金属度 |
| `Roughness` | 实体局部粗糙度 |

Shader 仍由基础 `.glmat` 决定，不允许实体覆盖。Shader 会影响管线、顶点布局和参数布局，把它作为普通实例参数会导致渲染状态难以归类，也不利于后续排序与合批。

`MaterialComponent` 现在同时保存共享材质引用和局部覆盖数据：

```cpp
struct MaterialComponent
{
    AssetHandle MaterialHandle{ 0 };
    MaterialOverrides Overrides;
};
```

### MaterialInstance 合并流程

`MaterialInstance` 位于核心库 `Renderer` 目录。构造时先复制基础 `MaterialProperties`，再只应用 `Mask` 中启用的字段：

```text
AssetManager::GetMaterial(MaterialHandle)
    -> 取得共享 Material
    -> 复制基础 MaterialProperties
    -> 应用启用的 MaterialOverrides
    -> 约束参数范围
    -> 得到本次绘制使用的最终属性
```

合并后继续保持以下约束：

- `TilingFactor >= 0.01`；
- `Metallic` 位于 `[0, 1]`；
- `Roughness` 位于 `[0.04, 1]`。

未启用的字段始终继承基础材质。因此修改 `.glmat` 后，所有未覆盖字段仍会使用最新的共享值；只有明确覆盖的字段保持实体自己的值。

### Inspector 编辑边界

实体和资产采用两条不同的编辑路径：

```text
Hierarchy 选中 Entity
    -> Entity Inspector
    -> 编辑 MaterialComponent::Overrides
    -> 不调用 Material::Save()

Content Browser 选中 .glmat
    -> Asset Inspector
    -> 编辑共享 MaterialProperties
    -> Material::Save()
    -> 所有继承该字段的实体同步更新
```

实体 Material 面板为每个可覆盖属性提供启用开关。第一次启用时会复制当前基础值，避免控件突然跳到 `MaterialProperties` 的默认值。拖入纹理会自动启用 `BaseColorTexture` Override；`Reset Overrides` 会清除全部位标记，使实体重新完整继承基础材质。

更换或移除实体的基础 `.glmat` 时会清空旧 Overrides，防止原材质的局部参数意外套用到结构或语义不同的新材质上。

Asset Inspector 会提示当前操作修改的是共享资源，避免把资产编辑误认为实体局部编辑。

### Renderer2D 与 Renderer3D 接入

`Scene` 在编辑模式和运行模式的 2D、3D 绘制路径中都会把 `MaterialComponent::Overrides` 传给 Renderer。

Renderer2D 解析后的颜色、Tiling 和纹理仍通过现有 Quad 顶点数据与纹理槽提交：

- 不同 `BaseColor` 不会打断合批；
- 不同 `TilingFactor` 不会打断合批；
- 已存在于当前批次的纹理会复用纹理槽；
- 单批超过可用纹理槽或索引容量时才执行 Flush；
- Renderer2D 当前仍固定使用 `TextureShader`，`.glmat` 的 ShaderHandle 尚未参与 2D 管线选择。

Renderer3D 使用合并后的 ShaderHandle 和 PBR 属性上传 Uniform，并解析最终基础颜色纹理。当前 Renderer3D 仍是逐模型、逐 Mesh 提交，本阶段没有新增额外拆批；后续真正建设 3D Instancing 时，应按 Shader、RenderState、Mesh、Material 和纹理组合生成 RenderKey，再把 Transform、EntityID 和可实例化材质参数写入 Instance Buffer 或 Material Buffer。

`MaterialInstance` 本身不创建新的 GPU Material，也不会复制纹理。当前额外 CPU 成本主要是每个实体提交时进行一次属性复制和位掩码合并；实体数量显著增大后，可以增加版本号、Dirty 标记和解析结果缓存。

### 场景复制与序列化

场景 YAML 仅在存在覆盖时写出 `Overrides`：

```yaml
MaterialComponent:
  Material: 13777784352782102236
  Overrides:
    Mask: 21
    BaseColor: [0.2, 0.7, 1.0, 1.0]
    BaseColorTexture: 0
    TilingFactor: 2.0
    Metallic: 0.1
    Roughness: 0.6
```

`Mask` 是实际生效字段的唯一依据。完整写出 `Values` 可以保持格式稳定，也便于以后启用某一字段时恢复已保存的数据。

兼容策略如下：

- 旧场景没有 `Overrides`：覆盖集合保持为空，行为与原先一致；
- 新场景有 `Overrides`：读取 Mask 和所有候选值；
- `Scene::Copy()`、实体复制和 `EntitySnapshot` 按值复制 `MaterialComponent`，Override 会自然进入 Runtime Scene、Duplicate 和 Undo 恢复流程；
- `.glmat` 继续只保存共享基础值，不包含任何实体 UUID 或场景局部数据。

### 其它 Asset 组件审计

本轮同时检查了可以挂载到实体的其它 AssetHandle 组件：

| 组件 | 资源引用 | 实体局部数据 | 是否需要 Instance/Override |
| --- | --- | --- | --- |
| `SpriteRendererComponent` | `TextureHandle` | Color、TilingFactor | 不需要；组件没有反向修改 Texture 资产 |
| `ModelRendererComponent` | `ModelHandle` | 当前无共享模型参数编辑 | 不需要 |
| `SkyLightComponent` | `CubemapHandle` | Enabled、Intensity | 不需要；Cubemap 只读，环境强度已组件化 |
| `TerrainComponent` | HeightMap、Shader 等句柄 | TerrainSpecification | 暂不需要；生成参数属于实体，底层资产未被反向修改 |
| `MaterialComponent` | `MaterialHandle` | MaterialOverrides | 需要；基础材质包含可被多个实体共享的可编辑参数 |

判断是否需要 Asset Instance 的标准不是“组件是否保存 AssetHandle”，而是“实体是否需要修改资产内部数据且不能影响其它引用者”。Texture、Model 和 Cubemap 当前都是只读引用；如果后续为它们增加每实体采样器、子网格可见性或环境旋转等设置，应优先把这些设置放入组件局部数据，而不是修改共享资产对象。

### 文件职责

| 文件 | 职责 |
| --- | --- |
| `Glimmer/src/Glimmer/Renderer/MaterialInstance.h/.cpp` | Override 位掩码、局部值和最终材质属性合并 |
| `Glimmer/src/Glimmer/Scene/Components.h` | 在 MaterialComponent 中保存材质句柄与 Overrides |
| `Glimmer/src/Glimmer/Scene/Scene.cpp` | 编辑/运行场景向 2D、3D Renderer 传递 Overrides |
| `Glimmer/src/Glimmer/Scene/SceneSerializer.cpp` | Material Overrides 的兼容序列化与反序列化 |
| `Glimmer/src/Glimmer/Renderer/Renderer2D.h/.cpp` | 将最终颜色、纹理和 Tiling 接入 Sprite 批处理 |
| `Glimmer/src/Glimmer/Renderer/Renderer3D.h/.cpp` | 将最终 PBR 参数接入模型绘制 |
| `GlimmerEditor-CyouBranch/src/Panels/SceneHierarchyPanel.cpp` | 实体 Material Override 编辑界面 |
| `GlimmerEditor-CyouBranch/src/Panels/InspectorPanel.cpp` | 共享 `.glmat` Asset Inspector 与保存入口 |

核心数据合并与渲染逻辑位于 `Glimmer`；实体/资产编辑交互位于 `GlimmerEditor-CyouBranch`，没有把 ImGui 或编辑器状态引入运行时核心库。

### 验证结果

- 新增核心源文件后重新运行 VS2026 Premake 脚本，工程文件成功包含 `MaterialInstance.h/.cpp`；
- `Debug | x64` 完整编译和链接成功；
- 同一 Visual Studio 实例再次增量构建成功，失败项目为 0，耗时约 0.1 秒；
- `git diff --check` 通过；
- 未删除 `bin` 或 `bin-int`，保留 Visual Studio 增量构建缓存；
- Entity Inspector 不再调用 `Material::Save()`；
- `.glmat` 的共享写入只保留在 Asset Inspector。

建议交互验证：为两个实体挂载同一个 `.glmat`，只给其中一个实体启用 BaseColor 或纹理 Override；另一个实体和原始 `.glmat` 应保持不变。随后保存并重新打开场景，确认 Override 恢复；最后点击 `Reset Overrides`，确认该实体重新继承共享材质。

### 当前边界与后续方向

1. Material Override 的逐属性编辑尚未接入统一 Undo/Redo 属性事务；
2. Renderer2D 暂不支持按 `.glmat` ShaderHandle 切换兼容批次；
3. Renderer3D 尚未建立 RenderQueue、RenderKey、材质排序和 Instanced Draw；
4. MaterialInstance 当前按绘制临时解析，尚无 Dirty/version 缓存；
5. Normal、AO、Emissive 和更多 PBR 纹理加入后，需要扩展 Override Mask 和 Inspector，但应保持同一合并模型；
6. 共享 `.glmat` 的 Asset Inspector 修改后续应进入 Asset Command，而不是组件命令。

下一步建议优先将 Material Override 和 Material Asset 编辑分别接入组件属性命令与 Asset Command，然后再建设 3D RenderQueue。这样 Undo/Redo、共享资产语义和后续合批边界能够保持一致。

## 项目品牌与 Windows 应用图标

### 建设目标

本轮将散落在 `tmp/logo/` 中的 Logo 源图迁移为正式项目资源，并为所有 Windows 可执行项目提供统一的应用图标。目标包括：

- 品牌源图和派生图标具有稳定、可追踪的目录；
- Sandbox 与两个编辑器使用同一个窗口、任务栏和 EXE 图标；
- 图标直接嵌入可执行文件，不依赖运行时工作目录或额外资源复制；
- 后续新增应用时只需复用同一份资源脚本；
- `tmp/` 继续只保存可删除的中间文件，不承担正式资源职责。

### 资源目录

```text
resources/
├── branding/
│   ├── GlimmerAppIcon.png          # 512×512 透明应用图标
│   ├── GlimmerAppIcon-Source.png   # 应用图标高分辨率源图
│   ├── GlimmerLogo-Crystal.png     # 水晶切面品牌方案
│   └── GlimmerLogo-Minimal.png     # 简洁金属品牌方案
└── windows/
    ├── Glimmer.ico                 # Windows 多尺寸图标（16–256 px）
    └── Glimmer.rc                  # 将图标嵌入 EXE 的资源脚本
```

`GlimmerAppIcon.png` 是从方形高分辨率源图确定性裁切和缩放得到的透明 PNG，没有重新生成或改变 Logo 设计。`Glimmer.ico` 包含 16、24、32、48、64、128 和 256 px 图像，兼顾资源管理器、窗口标题栏、任务栏和高 DPI 显示。

### Windows 资源接入

GLFW 的 Win32 后端会在可执行文件中查找名为 `GLFW_ICON` 的图标资源。共享资源脚本定义如下：

```rc
GLFW_ICON ICON "../resources/windows/Glimmer.ico"
```

图标由 Windows Resource Compiler 在构建阶段写入 EXE。运行时不需要调用 `stbi_load()`，也不需要通过相对路径加载 PNG，因此从 Visual Studio、资源管理器或其它工作目录启动程序时行为一致。

### Premake 工程接入

`Sandbox`、`GlimmerEditor` 和 `GlimmerEditor-CyouBranch` 的 `premake5.lua` 均在 `files` 中包含共享资源脚本：

```lua
files {
    "src/**.h",
    "src/**.cpp",
    "../resources/windows/Glimmer.rc"
}
```

新增 Windows `ConsoleApp` 或 `WindowedApp` 项目时应复用这一路径，不要复制并维护项目私有的 `.ico` 或 `.rc`。Premake 会生成对应的 `<ResourceCompile>` 项，Visual Studio 构建时自动调用资源编译器。

修改 Logo、ICO、RC 或 Premake 后，重新生成 VS2026 工程：

```bat
scripts\Win-GenerateProject-vs2026.bat
```

生成后再构建目标配置。不要手动修改 `.vcxproj`，因为生成文件会在下一次运行 Premake 时被覆盖。

### 文件职责

| 文件 | 职责 |
| --- | --- |
| `resources/branding/GlimmerAppIcon-Source.png` | 应用图标的高分辨率原始版本 |
| `resources/branding/GlimmerAppIcon.png` | README、界面或宣传场景使用的标准透明 PNG |
| `resources/branding/GlimmerLogo-Crystal.png` | 水晶切面品牌展示图 |
| `resources/branding/GlimmerLogo-Minimal.png` | 简洁金属品牌展示图 |
| `resources/windows/Glimmer.ico` | Windows EXE、窗口和任务栏使用的多尺寸图标 |
| `resources/windows/Glimmer.rc` | 声明 GLFW 约定资源名并将 ICO 嵌入应用 |
| 各应用的 `premake5.lua` | 将共享 RC 文件加入具体可执行项目 |

### 验证结果

- 重新运行 VS2026 Premake，三个应用工程均生成 `ResourceCompile` 项；
- VS2026 `Debug | x64` 全解决方案编译和链接成功；
- 从 `GlimmerEditor-CyouBranch.exe` 成功提取到关联图标，确认资源已实际嵌入 EXE；
- Sandbox、GlimmerEditor 和 GlimmerEditor-CyouBranch 共用同一图标资源；
- 原 `tmp/logo/` 已在资源迁移完成后清理；
- `git diff --check` 通过。

### 当前边界与维护约定

1. 当前 `.rc/.ico` 接入只负责 Windows；未来接入 Linux 或 macOS 时应分别补充桌面文件图标和应用 Bundle 图标，不应复用 Win32 RC；
2. 更新应用图标时必须同步更新标准 PNG 与多尺寸 ICO，并至少验证 16、32 和 256 px 显示效果；
3. 正式品牌资源统一保存在 `resources/branding/`，不要重新放入 `tmp/`；
4. 所有 Windows 应用共享 `Glimmer.rc`，避免出现资源 ID、图标版本或视觉风格分叉；
5. README 和其它 Markdown 文档引用品牌图片时使用仓库相对路径，保证 GitHub 和本地预览均可显示。

## 材质编辑事务与 Undo/Redo

### 建设目标

MaterialInstance 已经区分共享 `.glmat` 与实体局部 Override，但此前 Inspector 仍直接修改内存：数值拖动会产生大量中间状态，共享材质每帧变化都立即保存，实体 Override 和纹理操作无法撤销，保存失败也没有可靠的历史栈语义。

本次改造建立两类事务边界：

- `MaterialComponent` 事务负责实体的 MaterialHandle 与完整 Overrides；
- `MaterialState` 事务负责共享 Material 的 ShaderHandle 与全部 MaterialProperties，并同步 `.glmat` 文件。

### 失败感知的 CommandHistory

`IEditorCommand::Execute()` 与 `Undo()` 现在返回 `bool`。`EditorCommandHistory` 只有在操作成功后才移动命令：

- 新命令 Execute 失败时不进入 Undo 栈，也不清空现有 Redo 栈；
- Undo 失败时命令仍留在 Undo 栈；
- Redo 失败时命令仍留在 Redo 栈；
- 用户排除文件锁定、权限等问题后，可以再次执行原操作。

`LambdaEditorCommand` 继续服务于不会失败的实体生命周期操作。新增的 `ValueEditorCommand<T>` 保存 Before/After，并通过统一 Apply 回调恢复任一状态；`EditorValueTransaction<T>` 在 ImGui Item 激活时捕获 Before，在 `IsItemDeactivatedAfterEdit` 时结束事务。

### 实体 MaterialComponent 事务

以下 Inspector 操作现在都以完整 `MaterialComponent` 快照进入 Undo/Redo：

- 更换或清除 MaterialHandle；更换材质时一并清理 Overrides，Undo 会同时恢复旧 Handle 和旧 Overrides；
- 启用或禁用 BaseColor、BaseColorTexture、TilingFactor、Metallic、Roughness Override；
- 连续编辑 BaseColor、TilingFactor、Metallic 和 Roughness；
- BaseColorTexture 拖放、清除以及自动启用纹理 Override；
- `Reset Overrides`。

连续拖动期间场景保持实时预览，但只在控件结束编辑时生成一条命令。命令通过实体 UUID 重新查找目标，不依赖可能失效的 EnTT 临时句柄。

### 共享 Material Asset 事务

`MaterialState` 同时保存 ShaderHandle 和 MaterialProperties。共享 Asset Inspector 的 Shader、BaseColorTexture 和所有数值修改都使用完整状态命令。

连续拖动时只改变缓存中的 Material，以便所有继承实体实时更新；控件结束编辑后才保存一次 `.glmat` 并推入历史。Undo/Redo 都执行相同的“设置状态并保存”流程，因此内存缓存与磁盘文件保持一致。

如果保存失败，Inspector 会显示错误，内存恢复到操作前状态，CommandHistory 不移动。进入 Play 后共享 Material Inspector 变为只读，避免 RuntimeScene 操作写回全局资产；实体组件仍只修改 RuntimeScene 副本，Stop 后被丢弃。

### 安全落盘

`Material::Save()` 不再直接截断目标 `.glmat`。新流程为：

1. 将完整 YAML 写入同目录临时文件并检查 Flush 结果；
2. 将原文件改名为备份；
3. 将临时文件改名为正式文件；
4. 替换失败时恢复备份，成功后清理备份。

这保证普通写入失败或文件锁定不会破坏上一份有效材质文件。若备份恢复本身失败，日志会保留明确错误和备份路径供人工恢复。

### 文件职责

| 文件 | 职责 |
| --- | --- |
| `Glimmer/src/Glimmer/Renderer/Material.h` | `MaterialState` 与完整状态捕获/恢复 |
| `Glimmer/src/Glimmer/Renderer/Material.cpp` | `.glmat` 解析与安全替换保存 |
| `GlimmerEditor-CyouBranch/src/Editor/EditorCommand.*` | 失败感知历史、值命令与连续编辑事务 |
| `GlimmerEditor-CyouBranch/src/Panels/InspectorPanel.*` | 共享 Material Asset 事务和保存反馈 |
| `GlimmerEditor-CyouBranch/src/Panels/SceneHierarchyPanel.cpp` | 实体 MaterialComponent/Overrides 事务接入 |

### 验证结果

- VS2026、MSVC v145、`Debug | x64` 全解决方案连续两次构建成功；
- 临时原生 C++ 冒烟测试使用实际 Material 和 EditorCommandHistory，验证 Execute → 文件重载 → Undo → 文件重载 → Redo；
- 测试使用无删除共享权限的 Windows 文件句柄锁定 `.glmat`，Redo 正确失败，内存和磁盘均保持旧状态，命令留在 Redo 栈；解锁后同一 Redo 成功；
- `GlimmerEditor-CyouBranch` 在 Intel Iris Xe、OpenGL 4.6 下完成全部 Shader/Compute Shader 初始化并稳定运行 8 秒；
- `git diff --check` 通过；
- 构建仍包含项目既有的 C4244、C4267 和 `strncpy` C4996 警告，本次未扩大范围处理。

### 后续演进结果

1. Terrain、Directional/Point/Sky Light 与 Camera 的连续控件已复用 `ValueEditorCommand` 和 `EditorValueTransaction` 完成迁移；
2. Material 以外共享 Asset 尚无统一 Dirty 状态、退出保存提示或 Asset Command；
3. MaterialInstance 最终状态缓存、版本管理和完整状态比较已在 Renderer3D RenderQueue/Instancing 阶段落地；
4. 3D Opaque/Mask/Transparent RenderQueue、状态排序与 Opaque Instancing 已在后续阶段实现。

## 3D Opaque RenderQueue 与状态排序

### 建设目标

此前 `Renderer3D::DrawModel()` 在遍历 Scene 时立即解析资产、绑定 Shader、绑定纹理并逐 Mesh 绘制。多个实体共享 Shader、Material 和 Texture 时仍会重复绑定，且没有统一提交边界支持后续 Instancing、透明队列或可见性处理。

本次将模型渲染拆为三个阶段：

1. `BeginScene` 保存 ViewProjection/Camera 并清空本帧队列和统计；
2. `SubmitModel` 解析资产和 MaterialInstance，将每个 Mesh 展开为 RenderItem；
3. `EndScene` 排序 Opaque Queue、缓存 GPU 状态并统一执行。

### RenderItem 与 RenderKey

每个 RenderItem 保存：

- Mesh、Shader 和最终 Texture 的强引用，保证队列执行前资源有效；
- 合并 Overrides 后的 MaterialProperties；
- Transform 和 EntityID；
- 是否使用真实 BaseColorTexture；
- 用于排序的 RenderKey。

RenderKey 采用以下优先级：

```text
ShaderHandle → MaterialHandle → Texture RendererID → Mesh 地址 → EntityID
```

Shader、Material 和 Texture 排在前面以减少昂贵状态切换；Mesh 地址在资源生命周期内稳定；EntityID 提供最终全序，使同一 Scene 中改变 ECS 提交顺序不会改变不透明队列执行顺序。该键只用于运行时帧内排序，不作为持久化 ID。

### 状态缓存与 Uniform 上传

执行阶段只在 Shader 真正变化时调用 Bind，并上传场景级 ViewProjection、CameraPosition 和 BaseColorTexture Slot。每个 Item 继续上传 Transform、EntityID、BaseColor、Metallic、Roughness、TilingFactor 和纹理存在标记，保证不同 Overrides 不会错误复用参数。

Texture2D 只在 GPU RendererID 变化时重新绑定。为使缓存有效，`OpenGLRendererAPI::DrawIndexed` 不再在每次 Draw 后调用 `glBindTexture(GL_TEXTURE_2D, 0)`；资源解绑不再由低层 Draw 命令隐式决定，而由下一位状态所有者覆盖。

无效 Model、Material、Shader、空 Mesh 或零索引 Mesh 不进入队列，并计入 SkippedModels 或直接跳过，不导致渲染崩溃。

### Scene 接入顺序

编辑和运行场景均使用相同流程：

```text
Renderer3D::BeginScene
  → 遍历 ModelRendererComponent
  → Renderer3D::SubmitModel
  → Renderer3D::EndScene
  → TerrainRenderer
  → Renderer2D
```

因此模型相对 Terrain、Sprite、Skybox 和 Tone Mapping 的 Pass 顺序保持不变。EntityID 仍作为逐 Item Uniform 写入整数附件，鼠标拾取协议没有变化。

### 统计面板

Renderer3D Statistics 提供：

- SubmittedModels、SubmittedItems 和 SkippedModels；
- DrawCalls、ShaderBinds、TextureBinds；
- ImmediateModeShaderBinds 和 ImmediateModeTextureBinds 估算；
- SavedShaderBinds 和 SavedTextureBinds。

`GlimmerEditor-CyouBranch` 的 Stats 面板同时显示 Renderer2D 与 Renderer3D 数据。绑定估算使用改造前行为：每个有效 Model 绑定一次 Shader，每个 Mesh 绑定一次 Texture。

### 验证结果

- 临时原生 OpenGL 宿主加载同一个最小模型和 DefaultPBR Material 三次，并以 `30,10,20` 与 `20,30,10` 两种 EntityID 提交顺序执行；两次统计一致；
- 每轮得到 3 个 SubmittedItems、3 个 DrawCalls，ShaderBinds 从立即模式估算 3 降为 1，TextureBinds 从 3 降为 1；
- 同轮提交无效 ModelHandle，SkippedModels 增加 1 且无崩溃；
- Debug 运行时断言验证队列已排序、每个 RenderItem 都被执行、绑定次数不高于旧模式估算；
- VS2026、MSVC v145、`Debug | x64` 全解决方案构建成功；
- 完整编辑器在 Intel Iris Xe/OpenGL 4.6 下完成 Terrain、Skybox、Renderer2D、Tone Mapping 和 Compute Shader 初始化并稳定运行 8 秒；
- `git diff --check` 通过。

### 资产审计发现

当前 `GlimmerEditor-CyouBranch/assets/AssetRegistry.yaml` 包含多个 Model Handle，但新检出中缺少对应的 Wavefront `.obj` 文件。根因是 Visual Studio 通用忽略规则 `*.obj` 同时误伤了模型资源。

`.gitignore` 已增加 `!**/assets/**/*.obj`，今后恢复或新增的模型可以正常进入 Git。当前缺失模型仍需从原设备、远程历史或备份找回；RenderQueue 对这类失效 Handle 会安全跳过并计入统计。

### 当前边界与下一步

1. 当前只处理不透明队列，没有 BlendMode、透明分类或反向距离排序；
2. Opaque Queue 已支持严格兼容的 Instancing Batch，不同最终材质状态不会错误合并；
3. MaterialInstance 已有 version/Dirty 与最终属性缓存；
4. 下一主线是建立 AlphaMode 和独立 Transparent RenderQueue。

## 项目工作文档同步约定

为保证不同设备和不同 Codex 会话读取到一致的项目状态，仓库使用三份职责互补的工作文档：

| 文档 | 负责内容 |
| --- | --- |
| `Documents/PROJECT_STATUS.md` | 当前主线、后续任务、完成里程碑、验收证据与技术债 |
| `ARCHITECTURE.md` | 当前已经落地的模块职责、依赖关系、生命周期、数据流与实现边界 |
| `README.md` | 功能演进、使用和维护方式、实现笔记、验证结果与知识库 |

每次完成代码、资源、构建或工作流任务时，都要同步审查这三份文档：

1. 将任务结果和验证证据写入 `PROJECT_STATUS.md`，并保持“当前主线”只有一个；
2. 如果模块职责、依赖或跨层数据流发生变化，同步修改 `ARCHITECTURE.md`，且只记录已经实现的事实；
3. 如果功能行为、使用方式或维护流程发生变化，在本节上方、`## KB` 之前新增或更新相应 README 功能章节；
4. 没有内容变化的文档无需制造无意义修改，但必须确认它仍与源码和另外两份文档一致；
5. 三份文档发生冲突时，以当前源码为准，并在同一任务中完成修正。

## 生态系统路线整合

原独立的“程序化地形与环境模拟路线图”已经按文档职责拆分并融合：可执行阶段、依赖顺序和验收条件进入 `Documents/PROJECT_STATUS.md`；已经落地的模块职责和长期架构边界进入 `ARCHITECTURE.md`；本 README 继续记录用户可见行为、实现过程和验证方式。后续不再维护并行路线文件，避免 M0/M1 等已完成事项再次被误当作下一步。

当前可作为后续生态建设基础的能力包括：

- TerrainComponent、TerrainRuntime、TerrainRenderer 与外部/程序化 HeightMap；
- Graphics/Compute Shader 热重载、SimulationGrid、GPU Ping-Pong 和数据读回；
- Material/MaterialInstance、基础 Cook–Torrance PBR、Light UBO；
- HDR Scene Buffer、ACES Tone Mapping、TextureCube、SkyLight 与可见天空盒；
- UUID、Scene Copy、场景序列化、Edit/Play 隔离、SelectionContext 和 Undo/Redo 基础。

未来每个阶段除自身验收外，还遵守以下公共验证约束：

1. 使用独立 Lab 场景验证，不向默认编辑场景永久写入测试实体；
2. 程序化生成在相同 Seed 和参数下可复现，只在 Dirty 或显式请求时 Dispatch；
3. Compute Pass 不得无保护地读写同一纹理，结果必须无 NaN/Inf；
4. 水流、侵蚀和气候模拟使用固定时间步，保持状态非负，并记录质量守恒误差；
5. GPU Runtime 和派生缓存不写入场景文件，场景只保存业务参数与 AssetHandle；
6. 根据改动范围验证构建、编辑器首帧、场景保存/重载、Edit → Play → Stop、Undo → Redo 和 `git diff --check`；
7. 构建完成后保留 `bin` 与增量缓存，除非用户明确要求清理。

## 3D Instancing 与 MaterialInstance 缓存

在已有 Opaque RenderQueue 状态排序之上，Renderer3D 现在可以把兼容的重复模型合并为实例化绘制。目标不是盲目按模型名合批，而是保证 Mesh、Shader、纹理和最终材质参数完全一致时才共享一次 DrawCall。

### 实例输入与渲染接口

`BufferElement` 新增 `PerVertex / PerInstance` 输入频率。OpenGL VertexArray 不再让每个 VertexBuffer 从 attribute 0 重新开始，而是持续分配位置；Mat3/Mat4 会拆成多个列属性，实例元素使用 `glVertexAttribDivisor(..., 1)`。

公共 `RendererAPI` / `RenderCommand` 增加 `DrawIndexedInstanced`，OpenGL 后端封装 `glDrawElementsInstanced`。Renderer3D 维护最多 1024 项的动态 Instance Buffer，每项包含：

```cpp
struct InstanceData
{
    glm::mat4 Transform;
    glm::ivec4 EntityData; // x = EntityID
};
```

PBRModel Shader 使用 location 4–7 接收实例矩阵，location 8 接收 EntityID。`u_UseInstancing` 在实例和普通绘制路径间切换，因此单物体、不同材质拆批和鼠标拾取仍共用同一 Shader。Shader 在初次链接及热重载后检查这三个输入；不满足契约的 Shader 不会报错或强行实例化，而是自动逐项 DrawIndexed。

### 严格兼容合批

Opaque Queue 的排序键加入最终 MaterialProperties 的浮点位模式。连续 RenderItem 只有同时满足以下条件才会合批：

- Mesh、Shader 和实际绑定纹理相同；
- BaseColor、BaseColorTexture、TilingFactor、Metallic、Roughness 完全相同；
- 是否使用 BaseColor Texture 的状态相同。

因此两个实体即使引用同一 `.glmat`，只要 Override 的最终结果不同就会拆批；不同 Override 写法若最终状态完全一致则可以安全合并。超过 1024 项的 Batch 自动拆成多个实例 DrawCall。

### MaterialInstance 缓存

Material 和 MaterialOverrides 增加运行期 version/Dirty。Inspector 修改共享材质或实体 Override 时会推进版本。Renderer3D 使用 `(EntityID, MaterialHandle)` 缓存最终 ShaderHandle 和 MaterialProperties，并保存完整 MaterialState、Overrides 和最后使用帧：

- 基础材质和 Overrides 都未变化时直接命中；
- 任一最终输入变化时重新解析 MaterialInstance；
- 每次仍比较完整状态，保证 Undo/Redo 恢复旧版本号或遗漏 Dirty 时不会使用过期缓存；
- 120 帧未使用的项自动回收。

Stats 面板新增 Batch/Instance Count、Instanced/Individual Draws、Saved Draws 和 Material Cache Hit/Miss，可直接观察重复模型带来的收益。

### 验证结果

- 临时真实 OpenGL 场景提交 3 个相同 Cube/DefaultPBR：`3 Items → 1 Instanced Draw`，节省 2 次 DrawCall；
- 第三个实体启用不同 Roughness Override：`3 Items → 2 Draws`，证明差异材质会拆批；
- 再加入 2 个使用不兼容 Phong Shader 的模型：`5 Items → 4 Draws`，其中兼容的两个 Cube 合为 1 Draw，Phong 两项逐个回退；
- PBR 实例 Transform/EntityID Shader 完成真实驱动编译和运行，最终无测试注入编辑器稳定运行 8 秒；
- VS2026、v145、`Debug | x64` 全解决方案构建成功；相同命令二次增量构建约 3 秒且未重新编译源码；重新运行 VS2026 Premake 后，既有 SPIRV-Cross samples/tests 排除规则正确进入工程；
- `git diff --check` 通过；测试实体和日志已移除，`bin` 与 `bin-int` 增量缓存保留。

## Transparent RenderQueue 与材质 AlphaMode

在 Opaque RenderQueue 和 3D Instancing 稳定后，模型材质现在可以明确选择 `Opaque`、`Mask` 或 `Blend`。RGBA 图片不再因为所有模型共用不透明执行路径而把全透明区域写入深度和 EntityID；完整编辑器会在 Skybox 之后单独绘制 Blend 对象。

### 材质字段与编辑方式

`.glmat` 新增两个可选字段：

```yaml
Material:
  Shader: 15365846500528399802
  BaseColor: [1, 1, 1, 1]
  BaseColorTexture: 9195328290163695800
  TilingFactor: 1
  Metallic: 0
  Roughness: 0.5
  AlphaMode: Blend
  AlphaCutoff: 0.5
```

- `Opaque`：忽略 BaseColor/Texture 的 Alpha 分类，参加状态排序和 3D Instancing；
- `Mask`：有效 Alpha 小于 AlphaCutoff 时丢弃片元，保留不透明深度写入；
- `Blend`：进入独立透明队列，开启标准 Alpha 混合、深度测试，关闭深度写入。

旧 `.glmat` 没有这两个字段时按 `Opaque` 和 `0.5` 加载。共享 Material Inspector 可以直接修改 Alpha Mode/Cutoff；实体 MaterialComponent 也可以分别启用这两个 Override。两种入口都沿用完整状态 Undo/Redo，场景 YAML 会保存 Override Mask 和对应值。

### 双队列与 Pass 顺序

`Renderer3D::SubmitModel` 解析最终 MaterialInstance 后按 AlphaMode 分类：

```text
SubmitModel
  ├─ Opaque / Mask → FlushOpaqueAndMask
  │                   └─ 状态排序与兼容 Instancing
  └─ Blend          → TransparentQueue
                      └─ Skybox 后由 EndScene 远到近普通绘制
```

完整编辑器的场景 Pass 顺序为：

```text
Opaque/Mask Models → Terrain → Skybox → Sprite Batch → Transparent Models
```

完整编辑器调用 Scene Update 时延迟整个 Sprite Pass：Scene 只保存 ViewProjection 和待执行标记，不开始 Batch 或遍历 Sprite；EditorLayer 绘制 Skybox 后再调用 `Scene::FlushSpritePass`，此时才执行 Renderer2D 的 Begin、Sprite 遍历、Submit 和 End。这样纹理槽/索引容量触发的自动 Flush 也不可能发生在 Skybox 前。RenderDoc 曾确认旧顺序是 Sprite Draw 早于 Skybox，透明像素因此先与 Clear Color 混合；新的调用链让 Sprite Alpha 直接与 Skybox 颜色混合。没有 Skybox 编排的旧宿主继续使用默认的立即绘制行为。

TransparentQueue 以实体 Transform 原点到相机的平方距离由远到近稳定排序；距离相同时以 RenderKey 保证确定顺序。透明对象首版不参与 Instancing，避免把需要排序的实例错误合并。结束透明 Pass 后会恢复 Blend 禁用、DepthWrite 启用、DepthFunc Less 和标准 BlendFunc。

### Shader、Alpha 与拾取约定

PBRModel 使用以下有效 Alpha：

```text
effectiveAlpha = BaseColor.a × BaseColorTexture.a
```

- Mask 在 `effectiveAlpha < AlphaCutoff` 时执行 `discard`，被裁剪区域不写颜色、深度或 EntityID；
- Blend 在 `effectiveAlpha <= 1/255` 时执行 `discard`，避免完全透明像素覆盖拾取附件；
- 其余 Blend 片元按远到近绘制，较近可见片元最终写入 EntityID；
- 自定义 3D Shader 若要完整支持 AlphaMode，需要消费 `u_AlphaMode` 和 `u_AlphaCutoff` 并遵守相同约定。

RendererAPI/RenderCommand 提供 Blend Enable、Blend Function、Depth Write 和 Depth Function 控制。OpenGL 初始化默认禁用混合并启用深度写入；Renderer2D 在自己的 Batch 内显式启用并恢复 Alpha 混合，Skybox 使用只读深度，Transparent 使用只读深度加 Alpha 混合，各阶段均恢复规范默认状态。

### 统计与验证

Renderer3D 统计面板新增 Opaque、Mask、Transparent Item 数和 Transparent DrawCall。真实 OpenGL 烟测使用 `GlimmerEditor-CyouBranch/assets/textures/balatro.png`，该 RGBA 图片实际包含 0～255 的 Alpha：

- 旧材质缺少 Alpha 字段时成功按 Opaque/0.5 加载；
- 新材质字段完成保存、重载，实体 AlphaMode/AlphaCutoff Override 完成场景 YAML 往返；
- 2 个相同 Opaque、1 个 Mask、2 个不同距离 Blend 共得到 `5 Items / 4 Draws`；
- 两个 Opaque 合并为 1 次 Instanced Draw，两个 Blend 保持 2 次普通 Draw；
- PBRModel、Skybox、Terrain 和 Compute Shader 在 Intel Iris Xe / OpenGL 4.6 下完成真实驱动编译；
- VS2026、v145、`Debug | x64` 全解决方案构建成功；移除测试注入后完整编辑器稳定运行 8 秒。

当前边界：透明排序使用实体原点而不是 Mesh Bounds 中心；尚不支持透明 Instancing、双面材质、Order Independent Transparency 或自定义 Shader 自动注入 Alpha 逻辑。

## 可扩展 Debug 面板与 GPU Instancing Lab

当前完整编辑器在 `Window → Debug` 提供独立诊断窗口。DebugPanel 是后续渲染、资源、Scene 和 Terrain 测试的统一宿主；首版包含 Renderer3D Overview 和 Rendering 页签下的 GPU Instancing Lab，测试逻辑由独立 `InstancingLabTool` 管理，不写入 Renderer3D 或默认编辑场景。

### 临时场景边界

点击 Generate 后，Lab 创建一个只存在于内存中的 Scene，并通过 EditorLayer 的受控回调临时切换 `m_ActiveScene`：

```text
EditorScene 保持引用
  ↓
InstancingLabTool 创建临时 Scene 和真实 ECS 实体
  ↓
EditorLayer 将 ActiveScene 指向 Lab
  ↓
Scene → Renderer3D 走正常收集、材质缓存、排序和绘制链路
  ↓
Exit / New / Open / Play / Editor Detach
  ↓
释放 Lab Scene，恢复 EditorScene
```

Lab 激活期间不会记录 Undo/Redo，并禁止场景保存；Scene Hierarchy 只显示 Lab 提示，不逐行枚举大量测试实体，避免 ImGui CPU 开销干扰 Renderer3D 压力结果。Debug 面板关闭只隐藏窗口，不隐式销毁测试；明确退出 Lab 或触发生命周期清理后才释放临时 Scene。

### Instancing 预设

- **Maximum Instancing**：全部实体强制 Opaque，并共享 Model、Material 和最终参数，验证最大合批以及 1024 实例分块；
- **Material Split**：实体交替覆盖 Roughness 0.2/0.8，排序后形成两个兼容批次组，验证 MaterialOverrides 拆批；
- **Transparent Comparison**：全部实体覆盖为 Blend，验证 TransparentQueue 保持普通 Draw、Instanced Draw 为 0。

Model 和 Material 默认使用 `assets/models/geos/Cube.obj` 与 `assets/materials/DefaultPBR.glmat`，也可以从 Content Browser 拖入 Debug 面板替换。Count XYZ、Spacing 和 Origin 控制三维网格；总实体数有 100000 的安全上限。默认 `50×1×50` 生成 2500 个实体，单 Submesh 最大合批的理论值为：

```text
2500 Entities / Items
→ 1024 + 1024 + 452
→ 3 Instanced Draw Calls
→ 2497 Saved Draw Calls
```

### 理论值与实际值

Lab 按实体数、Model Submesh 数、预设分组和每批 1024 上限计算理论统计，并与当前帧 `Renderer3D::Statistics` 对照：

- Submitted Items；
- Draw Calls；
- Instanced Draw Calls；
- Individual Draw Calls；
- Instance Count；
- Material Cache Hit/Miss 与 Saved Draw Calls。

生成后首个尚未完成渲染的 UI 帧显示 Pending；当前帧 Items 匹配后，全部统计一致且没有 Skipped Model 时显示 PASS，否则显示 FAIL。Select First/Middle/Last 可选择代表实体，并继续使用整数 EntityID 附件验证同一次 Instanced Draw 中的拾取差异。

### 实现与验证

DebugPanel 只负责窗口和分类，`InstancingLabTool` 拥有参数、临时 Scene、理论统计与代表 UUID；EditorLayer 只负责 ActiveScene、Hierarchy/Inspector Context 和 CommandHistory 边界。后续类似测试应作为独立 Tool 加入 Debug 面板，而不是扩张 EditorLayer 或把测试分支写进 Renderer3D。

重新运行 VS2026 Premake 后，新 Debug/Panel 源文件已加入工程；VS2026、v145、`Debug | x64` 全解决方案构建成功，完整编辑器稳定运行 8 秒，`git diff --check` 通过。

## PBR 材质纹理通道扩展与 Material Lab

基础 Cook–Torrance PBR 原本只有 BaseColor Texture 与 Metallic/Roughness 标量。本阶段补齐模型材质常用的 Normal、AO 和 Emissive 纹理，并把同一字段契约贯通共享 `.glmat`、实体 MaterialOverrides、Inspector、Scene YAML、MaterialInstance 缓存、Renderer3D 排序/合批和 PBRModel Shader。

### 材质字段与默认行为

```cpp
struct MaterialProperties
{
    glm::vec4 BaseColor{ 1.0f };
    AssetHandle BaseColorTexture{ 0 };
    AssetHandle NormalTexture{ 0 };
    AssetHandle AOTexture{ 0 };
    AssetHandle EmissiveTexture{ 0 };

    float Metallic = 0.0f;
    float Roughness = 0.5f;
    float NormalScale = 1.0f;
    float AOStrength = 1.0f;
    glm::vec3 EmissiveColor{ 1.0f };
    float EmissiveStrength = 0.0f;
};
```

旧 `.glmat` 和旧 Scene 没有这些字段时仍可加载：新增 Texture Handle 默认为 0，Normal/AO 强度默认为 1，EmissiveStrength 默认为 0，因此旧材质不会自行发光，也不会改变原有法线和环境光结果。Metallic/Roughness 当前仍是标量；独立贴图或 ORM 打包通道尚未实现。

### 颜色空间契约

| 通道 | TextureColorSpace | 语义 | 原因 |
| --- | --- | --- | --- |
| BaseColor | sRGB | Color | 采样时由 GPU 解码为线性颜色，再进入 BRDF |
| Normal | Linear | Normal | RGB 保存方向数据，不能执行 Gamma 解码 |
| AO | Linear | Data/Height | 单通道遮蔽系数属于数值数据 |
| Emissive | sRGB | Color | 发光贴图是颜色输入，解码后在线性 HDR 空间累加 |

共享 Material Inspector 和实体 Override Inspector 在贴图拖放时写入对应元数据；元数据变化会使 AssetManager 清除该 Texture Handle 的 GPU 缓存。Renderer3D 绘制时只读取与 slot 契约兼容的纹理，不在渲染循环中修改 AssetRegistry，避免共享资产状态因 Draw Call 产生隐式变化。

### Renderer3D 纹理与合批契约

四类纹理使用固定纹理单元：

```text
slot 0 → BaseColor
slot 1 → Normal
slot 2 → AO
slot 3 → Emissive
```

Renderer3D 将四个 Texture GPU ID、纹理存在状态以及所有最终材质参数写入 RenderKey/MaterialSortKey。只有 Mesh、Shader、四类纹理和最终 MaterialProperties 全部相同的连续项才能进入同一个 Opaque Instancing Batch；任一实体启用不同贴图或强度 Override 都会正确拆批。缺少纹理时绑定白纹理作为安全占位，但 `u_Has*Texture` 会阻止 Shader 采样该 slot。

### Normal、AO 与 Emissive 计算

顶点阶段向片元阶段传递 World Normal 与 World Tangent。片元阶段先执行 Gram–Schmidt 正交化并构造 TBN，将法线贴图从切线空间转换到世界空间：

```glsl
vec3 tangent = normalize(v_WorldTangent
    - normal * dot(v_WorldTangent, normal));
vec3 bitangent = normalize(cross(normal, tangent));
vec3 tangentNormal = texture(u_NormalTexture, uv).xyz * 2.0 - 1.0;
tangentNormal.xy *= u_NormalScale;
normal = normalize(mat3(tangent, bitangent, normal) * tangentNormal);
```

模型加载器对退化 UV 和零切线增加稳定正交基回退，避免 `normalize(vec3(0))` 产生 NaN。当前 Tangent 仍是 `vec3`，镜像 UV 所需的 Handedness 留待后续扩展。

AO 只调制尚未包含遮挡信息的环境项，不重复压暗方向光和点光源的直接光：

```glsl
float ao = mix(1.0, texture(u_AOTexture, uv).r, u_AOStrength);
vec3 result = albedo * ambientColor * ambientIntensity * ao;
```

Emissive 不参与 BRDF，也不受光源方向影响；EmissiveColor 与 sRGB 纹理解码后直接加入线性 HDR Radiance，再统一经过 Exposure 与 ACES Tone Mapping：

```glsl
result += linearEmissiveColor * emissiveSample * u_EmissiveStrength;
```

Opaque、Mask、Blend、透明 Alpha discard 和整数 EntityID 输出继续使用原有路径；Normal/AO/Emissive 不改变透明深度策略和拾取语义。

### PBR Material Lab

打开 `Window → Debug → Rendering → PBR Material Lab`，点击 Generate 后会创建隔离的临时 Scene，并排生成六个 UV Sphere：

1. Normal Map；
2. Ambient Occlusion；
3. Emissive；
4. Dielectric Smooth；
5. Metal Smooth；
6. Metal Rough。

可以从 Content Browser 拖入 Sphere Model、Material 及三类测试纹理。数字按钮用于选择对应球体；面板会对比预期 RenderItem 与 Renderer3D 实际 `RenderedItems`，无跳过项时显示 PASS。PBR Lab 与 Instancing Lab 互斥，共享临时场景、禁止保存和退出恢复边界。

自动回归可在启动编辑器前设置：

```powershell
$env:GLIMMER_PBR_LAB_AUTORUN = "1"
```

该入口会自动生成六球场景，并在系统临时目录执行两项往返验证：旧式最小 `.glmat` 加载后保存/重载全部新字段，以及包含新 MaterialOverrides Mask/Values 的 Scene YAML 保存/加载。临时文件在验证后删除。

本次验证结果：Premake VS2026 生成成功；VS2026 v145 `Debug | x64` 全解决方案构建成功；自动 Lab 稳定运行 8 秒，日志记录 `PBR Material/YAML roundtrip PASS` 与 `PBR Material Lab PASS: rendered 6/6 items`，测试进程正常结束且无临时文件、日志或编辑器进程残留。

## 无窗口回归测试与 Windows 一键验证

编辑器内的 Instancing/PBR Lab 适合验证真实 OpenGL 绘制，但依赖窗口、GPU 和完整资源环境。为了让新设备拉取仓库后能够先验证数据层和构建链，本阶段增加独立的 `GlimmerRegressionTests` 控制台目标。它不创建 Application、Window 或 Renderer Context，失败时直接返回非零进程退出码。

### 当前测试范围

测试文件统一创建在系统临时目录，并在进程退出时清理，不会修改 `assets` 或默认编辑场景。当前覆盖：

- 旧式最小 `.glmat` 缺少新字段时恢复兼容默认值；
- 完整 MaterialState 的 ShaderHandle、四类纹理、PBR 标量、Emissive 与 AlphaMode 保存/重载；
- MaterialOverrides 只替换启用字段，并验证 Roughness、NormalScale、AOStrength、Emissive 和 AlphaCutoff 的运行时 Clamp；
- 固定 UUID 的最小 Scene 保存/加载，验证 Tag、Transform、ModelHandle、MaterialHandle 和 Overrides Mask/Values；
- 加载后的 `UUID → entt::entity` 索引能够通过 `FindEntityByUUID` 恢复稳定实体身份。

正常执行任一断言失败都会返回退出码 1。测试程序还保留显式失败入口，用于验证脚本或 CI 是否正确传播失败：

```powershell
.\scripts\Verify-Windows.bat -SkipGenerate -SkipBuild -ForceTestFailure
$LASTEXITCODE # 预期为 1
```

### 新设备标准流程

首次拉取建议直接包含全部子模块：

```powershell
git clone --recurse-submodules <repository-url>
cd Glimmer
```

如果仓库已经拉取，但依赖目录为空：

```powershell
git submodule update --init --recursive
```

安装 Visual Studio 2026 的“使用 C++ 的桌面开发”工作负载后，在仓库根目录运行：

```powershell
.\scripts\Verify-Windows.bat
```

脚本依次执行：

```text
检查递归 Git 子模块
→ 检查 Assimp Debug 生成头与静态库，缺失时自动构建
→ vendor/bin/premake/premake5.exe vs2026
→ 生成 GlimmerEngine.slnx 与各 vcxproj
→ MSBuild Debug | x64 全解决方案
→ GlimmerRegressionTests.exe
→ 以退出码报告最终结果
```

脚本优先从 PATH 或 Visual Studio Installer 的 `vswhere.exe` 查找 MSBuild。非标准安装位置可以显式传入：

```powershell
.\scripts\Verify-Windows.bat `
    -MSBuildPath "D:\Microsoft Visual Studio\2026\MSBuild\Current\Bin\MSBuild.exe"
```

已经生成并构建过工程时，可以只运行回归：

```powershell
.\scripts\Verify-Windows.bat -SkipGenerate -SkipBuild
```

`.bat` 不包含 `pause`，会原样返回 PowerShell/测试进程的退出码，适合本地终端和后续 CI；它同时显式使用 `ExecutionPolicy Bypass`，避免新电脑默认策略阻止仓库内验证脚本。

VS2026 Premake 当前生成的解决方案入口是 `GlimmerEngine.slnx`。仓库中的旧 `.sln` 可能由 VS2022 或更早版本产生，不应据此判断新测试目标是否已经进入解决方案。

### 与 Debug Lab 的关系

无窗口测试只覆盖确定性的状态、合并、YAML 往返和编辑命令状态迁移；Shader 编译、Framebuffer、DrawCall、Instancing 和纹理语义仍由 DebugPanel Lab 或完整编辑器验证。两种测试互补，但都遵守相同隔离原则：测试实体只存在于临时 Scene，不向 `m_EditorScene` 或默认 `.glimmer` 文件永久写入。

初始测试目标验证使用统一脚本完成 Premake VS2026 生成、MSBuild 18.8.2 `Debug | x64` 全解决方案构建和测试执行；Terrain 生命周期与预设回归随后将正常断言扩展到 46 项并保持全部 PASS，`--force-failure` 仍返回退出码 1。

## Terrain 生命周期与 Inspector 编辑事务收口

### 收口目标

Terrain 的可保存配置与 GPU 运行时资源必须严格分离。实体复制、场景保存/加载、Edit → Play 或 Undo/Redo 只能传递 `TerrainSpecification`；`TerrainGenerator`、网格与高度纹理属于目标 Scene 自己的 `TerrainRuntime`，不得跨实体或跨 Scene 共享。

同时，Terrain、Light 和 Camera 的连续参数需要与 Transform、Material 保持一致：拖动时实时看到结果，释放控件后只生成一条可逆命令，而不是每帧向 Undo Stack 写入一条记录。

### Terrain 复制与运行时失效

`TerrainComponent` 同时定义复制构造和复制赋值，二者只复制规格并清空 Runtime：

```cpp
TerrainComponent(const TerrainComponent& other)
    : Specification(other.Specification) {}

TerrainComponent& operator=(const TerrainComponent& other)
{
    if (this != &other)
    {
        Specification = other.Specification;
        Runtime.reset();
    }
    return *this;
}
```

这一约束覆盖四条路径：

```text
Duplicate Entity ─┐
Scene::Copy       ├─> 复制 TerrainSpecification
EntitySnapshot    ┤   清空 TerrainRuntime
Undo / Redo       ┘   下一次 Draw 时按需重建 GPU 资源
```

场景 YAML 仍只保存 `TerrainSpecification`。反序列化完成后 `Runtime` 为空，`TerrainRenderer` 根据 HeightMapResolution、MeshResolution、资源 Handle 和 Dirty 状态延迟创建或重建资源，因此保存文件中不会出现 OpenGL ID、纹理对象或生成器指针。

### 连续属性事务

Inspector 使用 `EditorValueTransaction<T>` 记录控件激活瞬间的完整组件值；拖动期间组件继续直接更新以提供实时预览；控件释放后，通过 `PushExecuted` 写入一条已经发生的 `ValueEditorCommand<T>`：

```cpp
if (ImGui::IsItemActivated())
    transaction.Begin(valueBeforeWidget);

if (ImGui::IsItemDeactivatedAfterEdit())
{
    const T before = transaction.GetBefore();
    transaction.Reset();

    history.PushExecuted(
        std::make_unique<ValueEditorCommand<T>>(
            commandName, before, valueAfterWidget, apply));
}
```

当前已覆盖：

- Terrain 的生成模式、分辨率、高度、全部 Noise 参数和 Offset；
- Directional Light 的启用、颜色、直接光强度与环境强度；
- Point Light 的启用、颜色、强度与范围；
- Sky Light 的启用、强度与 Cubemap 替换；
- Camera 的 Primary、投影类型、FOV、裁剪面、正交尺寸与固定宽高比。

高度图和 Cubemap 拖放属于离散操作，直接生成一条命令。Terrain 命令在 Apply/Undo/Redo 时会经过组件复制赋值，从而同时恢复完整规格并使旧 Runtime 失效。`Regenerate` 只刷新派生运行时数据，不改变可序列化状态，因此不进入 Undo Stack。

### 面板所有权

旧 `TerrainPanel` 从未接入当前 EditorLayer，并自行持有 `TerrainGenerator*`、Noise 设置和 Dirty 状态，会形成第二套参数来源，现已删除。正式 Terrain 参数唯一入口是组件 Inspector；长期 Debug 面板仍可以加入地形诊断工具，但只能观察或创建隔离测试场景，不能成为正式场景数据的所有者。

EditorLayer 继续只编排 Editor/Runtime/Active Scene、Framebuffer、Render Pass、相机与面板生命周期，不持有 TerrainMesh、HeightMap 或 Terrain Shader 的业务状态。

### 验证结果

- `scripts\Verify-Windows.bat` 完整通过 VS2026 Premake 生成与 MSBuild 18.8.2 `Debug | x64` 全解决方案构建；
- P6 完成时 35 项无窗口断言全部 PASS；P7 加入预设测试后扩展为 46 项；
- 完整 `GlimmerEditor-CyouBranch` 在项目工作目录下稳定运行 8 秒并完成 Shader/Compute 初始化；
- Terrain Transform 与 Specification 在实体复制后保持一致，副本 Runtime 为空；
- Scene YAML 往返保留全部 Terrain 规格且不持久化 Runtime；
- Edit → Play 的 `Scene::Copy` 不共享 Runtime，运行场景修改不会污染编辑场景；
- Terrain 值命令完成 Apply → Undo → Redo，且一次连续编辑只对应一次 Undo。

## 山脉生成、派生图与 Authoring Erosion

### 新增操作

选中 Terrain 实体后，Inspector 的 `Preset` 可以直接切换以下地貌：

- `Alpine`：方向明确、连续分布的高山山链；
- `Plateau`：具有阶地和宽阔顶部的高原；
- `Rolling Hills`：低起伏、适合植被场景的丘陵；
- `Volcanic`：带主锥体和火山口的中心地貌；
- `Eroded Valley`：沟谷更强、侵蚀轮次更多的山谷地貌；
- `Custom`：保留当前全部手调参数。

选择预设会一次性更新 Seed、Noise、HeightScale 和 Authoring Erosion，并作为一条命令支持 Undo/Redo。继续调整任一地貌参数后，Preset 自动变为 `Custom`。

新增参数包括：

| 参数 | 作用 |
| --- | --- |
| `Mountain Direction` | 旋转各向异性山链的主方向 |
| `Mountain Width` | 控制山链横向宽度与延展比例 |
| `Plateau Strength` | 将连续高度向稳定台地过渡 |
| `Enable Thermal Erosion` | 是否在基础高度生成后运行有限次热侵蚀 |
| `Thermal Iterations` | Authoring 阶段迭代次数，范围 0～128 |
| `Talus` | 允许保留的局部坡差阈值 |
| `Thermal Strength` | 每轮搬运强度，上限 0.5 |
| `Channel Erosion` | 基础生成阶段的沟谷刻蚀强度，不等同于 Thermal Erosion |

`Regenerate` 会显式使当前 Terrain Runtime 失效。Inspector 底部显示 Generation Version 和本次 Compute Dispatch 数；默认 Alpine 为 `1 + 28 + 1 = 30` 次 Dispatch。

### 三段式 Compute 管线

```text
Terrain Dirty / Regenerate / Compute Shader 热重载
  ↓
GenerateFBM.comp
  └─ 大陆、丘陵、方向性山链、台地/火山/沟谷预设
  ↓
ThermalErosion.comp × N
  └─ ReadTexture → WriteTexture → Barrier → Swap
  ↓
DeriveTerrainMaps.comp
  ├─ Normal.xyz + Slope
  ├─ Curvature + Flow Potential
  └─ Grass + Soil + Rock + Snow Weights
  ↓
Terrain Shader 采样 Height、派生法线和四层权重
```

生成高度使用 R32F `SimulationGrid`。Thermal Erosion 的每轮 Dispatch 都严格只读当前纹理、只写另一张纹理，之后执行 Memory Barrier 并交换读写索引，因此不存在同纹理无保护读写。

侵蚀属于有限次 Authoring 操作：只有 Terrain 为 Dirty、Compute Shader 热重载成功或用户点击 `Regenerate` 时才运行。普通渲染帧只采样上次生成结果，不会隐式继续改变地形。未来固定时间步 Runtime Erosion 必须使用另一套状态与调度器。

### 派生图布局

派生图均为 `RGBA16F` 运行时纹理：

| 纹理 | 通道布局 | 当前用途 |
| --- | --- | --- |
| Normal/Slope | RGB 为编码后的对象空间法线，A 为坡度 | Terrain 顶点法线与后续分层材质 |
| Analysis | R 为曲率，G 为局部 Flow Potential，B 为高度 | 后续湿度、积雪和侵蚀可视化 |
| Material Weights | RGBA 为 Grass、Soil、Rock、Snow | 当前基础颜色混合及下一阶段 TerrainMaterial |

四层权重在 Compute 阶段归一化。派生图只存在于 `TerrainRuntime`，不写入场景 YAML；场景只保存 Preset、Noise、Authoring 参数以及三个 Compute Shader Handle，加载后按需重建。

### Shader 热重载与失效

`TerrainGenerator` 同时监听：

- `GenerateFBM.comp`；
- `ThermalErosion.comp`；
- `DeriveTerrainMaps.comp`。

任一 Shader 成功热重载都会把 Terrain 标记为 Dirty，并重新执行完整三段管线；编译失败继续保留上一有效 Program，不替换现有地形结果。

### 验证结果

- VS2026 / MSBuild 18.8.2 `Debug | x64` 全解决方案构建成功；
- 46 项无窗口断言全部 PASS，五类预设重复应用结果一致且参数处于安全范围；
- Intel Iris Xe、OpenGL 4.6 下 Generate、Thermal Erosion、Derive Maps 三个 Compute Shader 均成功编译；
- 默认 Alpine 每次生成执行 30 次 Dispatch；
- 相同 Seed 与参数连续生成两次，Height 与全部派生图组合哈希均为 `4345498711584764525`；
- GPU 读回确认全部值无 NaN/Inf、Height/派生通道范围合法，Grass/Soil/Rock/Snow 权重和为 1；
- 显式验证入口为环境变量 `GLIMMER_TERRAIN_VALIDATE=1`，仅验证模式执行第二次生成与同步读回，正常编辑流程没有额外 Dispatch 或 Readback。

## TerrainMaterial 四层 Triplanar PBR

P7 已经从高度图派生 Grass、Soil、Rock、Snow 四通道权重，但当时 Terrain Shader 只用四种固定颜色做可视化。现在地形拥有独立的 `TerrainMaterial` 资产：它不复用普通模型材质的 `.glmat`，而以 `.glterrainmat` 保存四层纹理和混合参数。

### 新增操作

在 Content Browser 空白处右键，选择：

```text
Create Asset → Terrain Material (.glterrainmat)
```

选择该资产后，Inspector 可以编辑：

- 全局 `Triplanar Sharpness`、`Weight Contrast`；
- Height、Slope、Curvature、Moisture 四类影响强度；
- Grass、Soil、Rock、Snow 各自的 Base Color、Tiling、Metallic、Roughness、Normal Scale 和 AO Strength；
- 每层独立的 Albedo、Normal、AO 纹理。

Albedo 拖入后登记为 `sRGB + Color`，Normal 为 `Linear + Normal`，AO 为 `Linear + Data`。点击 `Save Terrain Material` 原子保存文件，`Reload from Disk` 丢弃内存修改并重新读取磁盘。Terrain 实体的组件区域可以拖入或清除 `.glterrainmat`；也可以直接把该资产拖进 Viewport：选中对象是 Terrain 时替换其材质，否则创建一个使用该材质的新 Terrain。

编辑器默认启动场景会创建一个 Alpine 程序化 Terrain，但其 `TerrainMaterialHandle` 保持为 0。Terrain Shader 和三个 Compute Shader 仍会生成高度及派生图，材质阶段使用内建 Grass/Soil/Rock/Snow 颜色与 PBR 数值，不解析 `.glterrainmat`，也不会加载四层 Albedo/Normal/AO 具体纹理。将 `assets/materials/DefaultTerrain.glterrainmat` 拖到该 Terrain 即可启用完整纹理，清空 Terrain Material 槽位则回到基础颜色路径。

仓库自带 `assets/materials/DefaultTerrain.glterrainmat`，并已配置四套 ambientCG 1K PNG PBR 资源：Grass001、Ground054、Rock027 和 Snow005。Grass/Soil/Rock 使用 Color、NormalGL、AmbientOcclusion；Snow005 原始套装没有 AO，因此 Snow 层保持空 AO Handle 并稳定回退为 1。下载包中的 Roughness、Displacement 和 NormalDX 当前保留在资源目录，但不会注册为 TerrainMaterial 运行时输入。

默认世界空间 Tiling 分别为 Grass `0.7`、Soil `0.4`、Rock `0.35`、Snow `0.55`。所有纹理层的 Base Color 设为白色，避免再次乘色改变已经校准的 Albedo；后续仍可用 Base Color 进行艺术化染色。

### 资产与场景边界

```yaml
TerrainMaterial:
  Version: 1
  TriplanarSharpness: 4.0
  WeightContrast: 1.15
  HeightInfluence: 0.65
  SlopeInfluence: 1.0
  CurvatureInfluence: 0.35
  MoistureInfluence: 0.65
  Layers:
    - Name: Grass
      BaseColor: [0.18, 0.48, 0.12]
      AlbedoTexture: 0
      NormalTexture: 0
      AOTexture: 0
      Tiling: 0.12
      Metallic: 0.0
      Roughness: 0.88
      NormalScale: 1.0
      AOStrength: 1.0
```

Scene YAML 只在 `TerrainComponent` 中保存 `TerrainMaterialHandle`。四层配置属于共享资产；Height、Normal/Slope、Analysis 和 Material Weights 仍是 `TerrainRuntime` 的派生 GPU 纹理，不会写进场景文件。

`.glterrainmat` 使用独立 `TerrainMaterial` YAML 根、AssetType 和 AssetManager 缓存，因此不会向普通 `.glmat` 增加地形专用字段，也不会进入 `MaterialInstance + MaterialOverrides` 继承链。

### 四层混合规则

Compute 阶段给出的四通道基础权重会在 Fragment Shader 中继续结合地貌上下文：

```glsl
float slope = 1.0 - clamp(normal.y, 0.0, 1.0);
float curvature = analysis.r * 2.0 - 1.0;
float moisture = clamp(
    (1.0 - height) * 0.45
    + flowPotential * 0.70
    + max(-curvature, 0.0) * 0.25,
    0.0, 1.0);

weights.grass *= 1.0 + moisture * moistureInfluence * (1.0 - slope);
weights.soil  *= 1.0 + moisture * moistureInfluence;
weights.rock  *= 1.0 + slope * 3.0 * slopeInfluence;
weights.snow  *= 1.0 + highAltitude * (1.0 - slope) * 3.0 * heightInfluence;
weights = normalizeWeights(pow(weights, vec4(weightContrast)));
```

结果是陡坡优先岩石，高海拔且平缓的位置增强积雪，低地、汇流和凹地增强湿度，再驱动土壤和植被。参数不是重新生成高度图的开关，只影响材质阶段，因此编辑后可以立即观察结果。

### Triplanar Mapping

规则地形网格只有平面 UV；如果直接用它采样，近乎垂直的山壁会把纹理压成狭长条。Triplanar Mapping 改用世界坐标分别投影到三个平面：

```glsl
vec3 projectionWeights = pow(abs(worldNormal), vec3(sharpness));
projectionWeights /= projectionWeights.x
    + projectionWeights.y + projectionWeights.z;

vec4 xProjection = texture(map, worldPosition.zy * tiling);
vec4 yProjection = texture(map, worldPosition.xz * tiling);
vec4 zProjection = texture(map, worldPosition.xy * tiling);
vec4 sampleValue = xProjection * projectionWeights.x
    + yProjection * projectionWeights.y
    + zProjection * projectionWeights.z;
```

面朝哪个轴，就更多使用与该轴垂直的投影；转折处按法线平滑混合。Albedo、Normal 和 AO 使用同一世界空间尺度，因而不依赖 Terrain 网格 UV，也不会随网格分辨率改变贴图密度。

### PBR 与纹理槽

四层各自计算线性 Albedo、世界空间细节法线、Metallic、Roughness 和 AO，再按最终权重混合。光照沿用 Model 的 Cook–Torrance GGX/Smith/Schlick 直接光；AO 只调制环境项，最终颜色保持在线性 `RGBA16F` Scene Buffer 中，之后统一经过 ACES Tone Mapping。

```text
Slot 0      Height
Slot 1      Normal / Slope
Slot 2      Curvature / Flow Potential
Slot 3      Grass / Soil / Rock / Snow Weights
Slot 4–6    Grass Albedo / Normal / AO
Slot 7–9    Soil Albedo / Normal / AO
Slot 10–12  Rock Albedo / Normal / AO
Slot 13–15  Snow Albedo / Normal / AO
```

Renderer 只绑定符合语义契约的 Texture Asset。贴图为空或语义不匹配时不会误采样：Albedo 回退 Base Color，Normal 回退派生/几何法线，AO 回退 1。

### 验证结果

- VS2026 / MSBuild 18.8.2 `Debug | x64` 全解决方案构建成功；
- 53 项无窗口断言全部 PASS，覆盖 TerrainMaterial 四层参数/纹理保存重载、注册表类型/缓存隔离、独立 YAML 根和 Scene TerrainMaterialHandle 往返；
- Intel Iris Xe、OpenGL 4.6 下新版 Terrain Shader 与 Generate/Thermal Erosion/Derive Maps 三个 Compute Shader 均编译成功；
- `GLIMMER_TERRAIN_VALIDATE=1` 下默认 Alpine 仍为 30 次 Dispatch，两次 GPU 输出哈希均为 `4345498711584764525`。
- Grass/Soil/Rock/Snow 的 11 张运行时纹理已逐项验证文件、注册表和 Handle 引用；直接运行既有编辑器二进制时没有纹理缺失或语义拒绝日志，本次纯资源配置不要求重新编译。
- 默认 Alpine Terrain 保持 `TerrainMaterialHandle = 0` 后，VS2026/MSBuild 18.8.2 `Debug | x64` 全解决方案构建成功；RTX 4060/OpenGL 4.6 下 Terrain 与 Generate/Thermal Erosion/Derive Maps 均成功编译，启动代码不会导入默认 TerrainMaterial 或其 11 张运行时纹理。
- VS 启动时的撕裂/显示异常最终确认来自显卡切换与独显选择，不是 Terrain Shader。完整纹理路径最坏约执行 36 次层纹理采样，加上 Height/派生图后接近 40 次/像素；该数字仅作为后续 Top-2 层裁剪和质量分级的性能优化基线。

## 方向光 Shadow Map 与 CSM

P9 已加入 Model 与 Terrain 共用的 Directional Shadow Map，并扩展为最多四级 CSM。Scene 在正常 HDR 颜色 Pass 前，从第一个启用且勾选 `Cast Shadows` 的方向光生成多张纯深度图；PBRModel 和 Terrain 随后根据片元的视空间深度选择级联，判断当前片元是否被遮挡。

### 新增操作

选择 Directional Light 后，Inspector 提供：

- `Cast Shadows`：启用或关闭方向光阴影；
- `Shadow Resolution`：512、1024、2048、4096；
- `Cascade Count`：1～4，级联越多，近景有效阴影分辨率越高，但深度 Draw Call 也会增加；
- `Shadow Distance`：阴影覆盖到相机前方的最远距离；
- `Shadow Bias`：基础深度偏移，用于平衡 Shadow Acne 与 Peter Panning。
- `Split Lambda`：0 为均匀分割，1 为对数分割；默认 0.65，在近景精度和远景覆盖之间折中。
- `Cascade Blend`：0～0.30，控制相邻级联在 Split 两侧的重叠比例；默认 0.10，用于消除级联硬切换。

打开 `Window → Debug → Overview`，在 `Directional Shadows` 下勾选 `Visualize Cascades`，可用固定颜色检查当前片元所属级联：第 1～4 级依次为红、绿、蓝、黄。Split 重叠区会按实际 `Cascade Blend` 权重在两种颜色之间渐变，因此可直接观察覆盖范围、分界位置和过渡宽度。该开关只在本次运行中生效，不保存到场景，也不改变阴影深度、透明度或实体拾取结果。

同一区域的 `GPU Time` 显示整段 CSM Shadow Pass 的 GPU 耗时。首次运行会短暂显示 `pending`；OpenGL 使用四槽 Time Query 延迟读取，只有结果已经可用时才更新毫秒值，不会为了刷新面板调用阻塞式 GPU Readback。

这些设置会进入 Scene YAML。Shadow Framebuffer、Depth Texture 和 Light VP 只属于运行时资源，不会写入场景。

### 渲染流程

```text
Scene 找到首个启用且 CastShadows 的 Directional Light
  → TerrainRenderer::Prepare 生成/复用地形 Height 与派生图
  → 根据 Camera Frustum 与 Practical Split 计算 1～4 个级联
  → 按 Cascade Blend 扩展相邻级联并建立重叠区
  → 每级执行包围球稳定化与 Shadow Texel Snap
  → 使用 Mesh/Terrain Bounds 对当前级联执行保守六平面剔除
  → 依次绑定各级 Depth32F Framebuffer
  → Opaque/Mask Model 按 Mesh + Mask 状态排序并构造每级 Instancing Batch
  → Blend Model 默认跳过 Shadow Pass，避免错误的实心投影
  → ShadowDepth.glsl 实例化绘制兼容 Model，独立绘制 Terrain
  → 恢复 Scene Framebuffer 与 Viewport
  → 正常 Opaque / Terrain / Skybox / Sprite / Transparent
  → PBRModel 与 Terrain 执行 3×3 PCF，并在 Split 重叠区混合相邻级联
```

Shadow 深度资源使用无颜色附件的 `Depth32F` Framebuffer；Framebuffer 后端会为 depth-only FBO 设置 `GL_NONE` Draw/Read Buffer。Shadow Pass 只清理深度，不污染主 Scene 的 HDR Color、Entity ID 或 Depth。

接收阶段先根据视空间深度选择级联，再把世界位置乘以对应 Light VP 并映射到 `[0, 1]`，随后比较当前深度和对应 Shadow Map。进入 Split 两侧的重叠区时，两级各执行一次 PCF，并用 `smoothstep` 从近级平滑过渡到远级：

```glsl
float slopeBias = max(
    u_ShadowBias * (1.0 - max(dot(normal, lightDirection), 0.0)),
    u_ShadowBias * 0.25);

int cascadeIndex = SelectCascade(abs((u_ShadowCameraView * vec4(worldPosition, 1.0)).z));
float nearVisibility = SampleCascadeVisibility(boundary, worldPosition, normal, lightDirection);
float farVisibility = SampleCascadeVisibility(boundary + 1, worldPosition, normal, lightDirection);
float blend = smoothstep(split - width, split + width, viewDepth);
return mix(nearVisibility, farVisibility, blend);
```

对周围 `3×3` Texel 求平均得到软化后的可见度，仅调制方向光直接照明；Ambient、Emissive 和 Point Light 不会被错误乘上方向光阴影。

Mask 材质进入 Shadow Pass 时不再按完整三角形轮廓写深度。Scene 会同时提交实体的 MaterialHandle 和 Overrides，ShadowRenderer 通过 `MaterialInstance` 得到最终材质，再让 ShadowDepth 使用和 PBRModel 一致的裁剪条件：

```glsl
float textureAlpha = u_HasBaseColorTexture != 0
    ? texture(u_BaseColorTexture, v_TexCoord * u_TilingFactor).a
    : 1.0;

if (clamp(u_BaseColorAlpha * textureAlpha, 0.0, 1.0) < u_AlphaCutoff)
    discard;
```

因此树叶、铁丝网或镂空贴图的透明区域不会写入 Shadow Map，接收面上会得到对应的镂空阴影。模型贴图优先使用最终 Material BaseColorTexture，未设置时沿用 Mesh 自带纹理；纹理语义仍必须是 sRGB Color。ShadowDepth 分别从 location 3 读取 Model UV、从 location 1 读取 Terrain Height UV，透明裁剪不会破坏地形顶点位移。

Blend 材质采用明确的首版策略：不进入 Directional Shadow Queue。半透明表面没有单一正确的二值深度，直接写入会投出与透明度无关的实心轮廓；因此当前优先避免错误阴影，而不是伪装成透射阴影。Opaque 与 Mask 仍正常投影。未来若需要玻璃彩色透射、随机抖动或透射率累积，应作为独立的 Transparent Shadow 能力实现，而不是混入现有 Depth-only CSM。

每个级联不再逐模型立即 Draw。通过 Frustum 测试的子网格先进入 Shadow Queue，随后按 Mesh 和最终 Mask 状态排序；Opaque 只要求 Mesh 相同，Mask 还要求 BaseColor Texture、BaseColor Alpha、AlphaCutoff 与 TilingFactor 全部一致。兼容批次把 Transform 写入动态 Instance Buffer：

```cpp
RenderCommand::DrawIndexedInstanced(
    shadowVertexArray,
    static_cast<uint32_t>(instanceTransforms.size()),
    mesh->GetIndexCount());
```

单次最多上传 1024 个实例，超出后自动分块。Shadow VAO 与主 Renderer3D VAO 分开缓存：它复用 Mesh 的逐顶点缓冲和索引缓冲，但挂载自己的 location 4～7 Transform Buffer，因此两个渲染器不会覆盖彼此的实例属性。单项批次自动回退普通 Draw，Terrain 继续保持独立深度 Draw。

### 独显性能对照操作

1. 在 `Window → Debug → Rendering → GPU Instancing Lab` 选择 Model 与 Material；
2. 使用 `Maximum Instancing`，从较小的 `Count XYZ` 开始生成，再逐步增加实体数；
3. 调整相机使待测实体处于稳定构图，设置 `Warmup Frames` 与 `Samples / Configuration`；默认值为每组预热 15 帧、采集 30 个新 GPU Query 结果；
4. 点击 `Start Shadow Benchmark`。工具会自动测试 `1/2/4 Cascades × 1024/2048/4096 Resolution` 共 9 组配置，测试期间不要移动相机、缩放窗口或切换显卡设置；
5. 在结果表读取每组 `Avg/Min/Max ms`、`Draws` 和 `Saved`；测试可随时取消，关闭 Debug 窗口或切换页签不会中断；
6. 回到 `Overview` 开启 `Visualize Cascades` 检查覆盖与 Blend，再关闭调试色观察 Acne、Peter Panning 和移动相机时的级联跳变。

若要集中完成视觉检查，将 Preset 改为 `Shadow Visual Validation` 后点击 Generate。工具会忽略 Count/Spacing，固定生成 8 个模型实体：长距离地面、橙色 Opaque 对照板、使用 `balatro.png` Alpha 的 Mask 镂空板、不会投射实心阴影的青色 Blend 对照板，以及沿相机深度分布的四个彩色标记。相机会自动框定测试区；`Frame Cascade Range` 恢复长距离级联构图，`Frame Casters` 切换到 Opaque/Mask/Blend 近景。

视觉面板提供以下纯运行时控制：

- `Visualize Cascades`：用红、绿、蓝、黄显示实际级联与重叠过渡；
- `Shadow Bias`：逐步降低直到出现表面条纹即为 Acne 边界，逐步升高并观察阴影是否脱离物体即为 Peter Panning；
- `Split Lambda`：观察四个深度标记附近的级联覆盖重新分配；
- `Cascade Blend`：从 0 增大，确认硬分界变为连续过渡；
- `Shadow Distance`：确认超出距离的物体不再接收方向光阴影。

该预设仍是临时内存 Scene，不进入场景保存、Undo/Redo 或资产文件；退出 Lab 后恢复原编辑场景。

也可以从 `GlimmerEditor-CyouBranch` 工作目录启动可重复的视觉入口：

```powershell
$env:GLIMMER_SHADOW_VISUAL_AUTORUN = ''1''
$env:GLIMMER_SHADOW_VISUAL_CLOSEUP = ''1''          # 可选：投影物近景
$env:GLIMMER_SHADOW_VISUALIZE_CASCADES = ''1''      # 可选：级联着色
..\bin\Debug-windows-x86_64\GlimmerEditor-CyouBranch\GlimmerEditor-CyouBranch.exe
```

GTX 1050/OpenGL 4.6 实际窗口验证中，全景构图显示深度标记跨越不同级联，重叠边界保持连续；近景构图中 Opaque 接触阴影稳定、`balatro.png` Mask 保持镂空轮廓，默认 Bias 下未见明显大面积 Acne 或 Peter Panning。青色 Blend 板自身正常透明绘制，但地面不出现对应的实心矩形阴影，与回归策略一致。

每次配置切换后的预热会排空异步 Query 延迟；采样仅在 GPU 返回新结果时推进，不会重复使用面板中缓存的上一帧数值。结果只存在于当前临时 Lab，不写入场景或资产。测试时仍需保持窗口分辨率、相机、模型数量、Shadow Distance 与驱动设置一致。GPU Time 只统计 Shadow Pass，不包含 Scene Color、Terrain Compute、Tone Mapping 或 ImGui，因此适合比较级联数、分辨率与实例数量对阴影本身的影响。

需要在固定机器上重复采样时，可从 `GlimmerEditor-CyouBranch` 工作目录启动无人值守入口：

```powershell
$env:GLIMMER_SHADOW_BENCHMARK_AUTORUN = '1'
..\bin\Debug-windows-x86_64\GlimmerEditor-CyouBranch\GlimmerEditor-CyouBranch.exe
```

该入口固定使用 `50×1×50` Maximum Instancing 场景、15 帧预热和每组 30 个样本。OpenGL Context 启动日志会给出 Vendor、Renderer 和 Version；完成后日志依次输出 9 行 `Shadow Benchmark Result` 并正常关闭编辑器，便于确认实际使用的 GPU 并复制结果。不要同时设置 `GLIMMER_PBR_LAB_AUTORUN`。

### 当前边界与验证

- 当前支持 1～4 级 CSM、Practical Split、Shadow Texel Snap、可调重叠混合与运行时级联调试着色；
- Mesh 在构造时缓存局部 AABB；每个级联会把 Model 子网格 Bounds 变换到 Light VP Clip Space，8 个角点全部位于同一平面外才剔除；Terrain 使用网格 XZ 范围与 HeightScale 构造保守 Bounds；
- Debug → Overview 的 `Directional Shadows` 区域显示 Cascades、Candidate/Rendered、Frustum Culled、Draw Calls、Instanced/Individual、Instances、Saved Draws 与非阻塞 GPU Time；可通过移动相机或生成重复模型确认剔除、合批及耗时，也可启用 `Visualize Cascades` 检查分级和重叠过渡；
- Debug → Rendering 的 Instancing Lab 可自动完成 9 组 CSM 性能采样并显示 Avg/Min/Max；计时样本带单调序号，只有新的异步 Query 结果才会被纳入统计；
- Model Shadow Pass 已按每个级联独立合批；不同 Mesh 或不同最终 Mask 状态会正确拆批，Terrain 保持独立提交；
- Alpha Mask 已按最终 MaterialInstance 的 BaseColor Alpha、纹理 Alpha、TilingFactor 与 AlphaCutoff 裁剪 ShadowDepth；Blend 默认跳过 Shadow Pass，尚未实现抖动、彩色透射或透射率累积阴影；
- Terrain 在 Shadow Pass 前显式 Prepare，因此首帧即可使用生成后的高度参与投影；
- VS2026 `Debug | x64` 全解决方案构建成功；立即重复同配置构建只执行增量项目检查，没有重新编译源文件；64 项断言与最终汇总全部 PASS，包含阴影设置往返、四类 Bounds 剔除、Shadow Saved Draw 计算，以及 Opaque/Mask/Blend 投影策略；
- 新增级联调试着色、Alpha Mask 与 Shadow Instancing 后，Intel Iris Xe/OpenGL 4.6 下 ShadowDepth、PBRModel、Terrain 和三个 Terrain Compute Shader 均重新编译成功；PBR Material Lab 渲染 6/6 项且没有跳过模型，默认地形的 Height UV 与 Compute 路径正常。自动测试不判定颜色和投影轮廓，仍需手动勾选 `Visualize Cascades` 检查分区，并用带 Alpha 的 BaseColor Texture + Mask 材质确认透明区域不产生阴影。
- PBR Lab 的紧凑布局得到 `24 candidates / 24 rendered / 0 culled / 4 draw calls / 4 instanced / 20 saved / 4 cascades`：每个级联把 6 个相同 Mesh 合并为一次 Draw，并确认保守测试没有误删投影。把模型移出 Shadow Frustum 后可在 Debug → Overview 观察 `Frustum Culled` 增加。
- OpenGL Time Query 已在 Intel Iris Xe 上非阻塞返回，PBR Lab 四级 Shadow Pass 得到一次 `0.278 ms` 验证样本；该数字只验证计时范围和读取链路，正式性能结论必须按上面的固定场景步骤在 RTX 4060 上采集多组稳定值。
- 自动 Shadow Benchmark 接入后，VS2026 `Debug | x64` 最终编辑器目标构建成功，61 项无窗口回归断言全部 PASS。固定 2500 实体、15 帧预热、每组 30 样本的无人值守测试确认 OpenGL Renderer 为 `NVIDIA GeForce RTX 4060 Laptop GPU`，结果如下：

| Cascades | Resolution | Avg ms | Min ms | Max ms | Draws | Saved |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1024 | 0.058 | 0.055 | 0.065 | 3 | 2260 |
| 2 | 1024 | 0.665 | 0.580 | 0.682 | 4 | 2276 |
| 4 | 1024 | 1.066 | 1.060 | 1.090 | 5 | 2563 |
| 1 | 2048 | 0.965 | 0.961 | 0.980 | 3 | 2261 |
| 2 | 2048 | 1.451 | 1.449 | 1.467 | 4 | 2277 |
| 4 | 2048 | 2.414 | 2.400 | 2.444 | 5 | 2562 |
| 1 | 4096 | 1.666 | 1.663 | 1.677 | 3 | 2260 |
| 2 | 4096 | 3.182 | 3.178 | 3.194 | 4 | 2278 |
| 4 | 4096 | 6.313 | 6.304 | 6.319 | 5 | 2562 |

同场景 Iris Xe 的 `4096 × 4` 平均为 `11.224 ms`，RTX 4060 为 `6.313 ms`，最高档约快 1.78 倍；RTX 各组 Min/Max 也更集中。定量性能由固定基准覆盖，级联边界、Mask 轮廓、Acne、Peter Panning 与 Blend 策略则由 GTX 1050 的全景/近景实际视口验收覆盖，P9 至此完成。

## Renderer2D 空批次残留修复

实体移除 `SpriteRendererComponent` 后，下一帧仍会正常执行空的 Sprite Pass。旧实现把此时的 `QuadIndexCount = 0` 继续传入 `DrawIndexed`，但渲染 API 中的零参数表示“使用 VertexArray 的完整 IndexBuffer”，因此上一帧留在动态 VBO 中的 Quad 会被预生成索引重新绘制，并使用 0 号白纹理槽显示为白块。

修复后，空批次在 Renderer2D 边界直接跳过：

```cpp
void Renderer2D::Flush()
{
    if (s_Data.QuadIndexCount == 0)
        return;

    // Bind textures and issue the actual batch draw...
}
```

这样不会改变 `DrawIndexed(0)` 对模型等其他调用方的既有行为，同时保证添加、移除或 Undo/Redo `SpriteRendererComponent` 后，空 Sprite 帧不提交 Draw Call，也不会重画上一帧的残留顶点。

验证：VS2026 `Debug | x64` 编辑器目标构建成功；55 项无窗口断言及最终汇总全部 PASS；默认 Alpine 场景在 Intel Iris Xe/OpenGL 4.6 下稳定启动，未出现 OpenGL 或 Shader 错误。

## tmpTerrain 地质地貌迁移实验

本次没有直接复制 `tmp/tmpTerrain` 的 HLSL，而是把其中可独立验证的 Worley 地质块、陡峭区遮罩、裂谷和大尺度趋势重写进现有 `GenerateFBM.comp`。因此新效果继续复用 Terrain Entity、Compute Shader 热重载、Runtime Dirty、Thermal Erosion、派生图、TerrainMaterial、CSM 和场景序列化，不新增第二套地形生命周期。

### 可编辑参数

Terrain Inspector 的 `Geological Features` 区域新增：

- `Geology Blend`：原始 Glimmer 高度与新增地质塑形之间的混合量；设为 0 会跳过新增分支并恢复原生成公式；
- `Geology Scale`：控制 Worley 地质块和裂谷结构的尺度；
- `Rift Strength`：控制狭长低地/裂谷对高度的削减；
- `Trend Strength`：控制沿 Mountain Direction 形成的大尺度高低趋势。

修改任一参数会把预设切换为 Custom，并通过现有 Inspector 事务生成单次 Undo/Redo 命令；Terrain Runtime 被标记 Dirty，下一次 Prepare 重新生成 Height、Normal/Slope、Analysis 和 Material Weights。四项参数随 Scene YAML 保存。重新选择 Alpine、Plateau、Rolling Hills、Volcanic 或 Eroded Valley 时会载入各自较保守的地质参数，其中 Alpine/Eroded Valley 较强，Rolling Hills/Volcanic 较弱。

### Compute 流程

```text
现有 Domain Warp / Continental / Ridged Mountain / Channel
  → 可选 Worley 地质块与 Steep Region Mask
  → 狭长 Rift 削减
  → Mountain Direction 驱动的大尺度 Trend
  → Geology Blend 与原高度混合
  → 既有 Preset 特化
  → Thermal Erosion Ping-Pong
  → Normal / Curvature / Flow Potential / Material Weights
```

原型中的水流、泥沙、蒸发和气象耦合没有迁移。其单个 `TerrainData_CS` 在写入 WaterFlow 后只执行 Workgroup Barrier，却立即读取相邻 Workgroup 的结果，不能保证跨组可见性。正式接入时应按固定时间步拆成 Rain/Evaporation、Flux、Water Update、Velocity、Sediment、Erosion/Deposition 与 Derive Maps 等独立 Dispatch，每一步使用明确的 Ping-Pong 资源和全局 Memory Barrier。

### 验证

- GTX 1050/OpenGL 4.6 下 GenerateFBM、ThermalErosion、DeriveTerrainMaps 与 Terrain Shader 全部编译成功；
- 默认验证场景执行 30 次 Dispatch，两轮输出均通过有限值、范围和四层权重归一化检查，确定性 Hash 为 `16881604791310884879`；
- VS2026 `Debug | x64` 全解决方案构建成功；
- 64 项无窗口回归断言全部 PASS，新增参数已覆盖 Scene YAML 往返和五类 Terrain Preset 范围。

## TerrainMaterial Top-2 采样与 GPU 基准

四层 TerrainMaterial 的原始质量路径会对 Grass、Soil、Rock、Snow 全部执行 Albedo、Normal、AO 的三平面读取，最坏接近 40 次纹理采样/像素。此次优化不改变高度、派生权重或 PBR 光照公式，而是在最终混合权重已经包含高度、坡度、曲率与湿度修正后，先选出贡献最高的两层，再进入具体纹理采样。

### 质量档位

Debug → Overview → Terrain 的 Sampling 提供四档：

- Full 4 Layers：完整四层采样，作为最高质量与性能基线；
- Top 2 Layers：只保留贡献最高的两层，两层仍读取 Albedo、Normal 和 AO；
- Top 2 + Dominant Normal/AO：两层读取 Albedo，但 Normal/AO 只读取主导层；
- Auto Distance：性能档位；近景使用 Top-2 完整细节，远景使用主层 Normal/AO。Detail Distance 默认为 80 世界单位，阈值前后各 15% 组成 smoothstep 过渡带，次要层法线和 AO 不会在单个距离点硬切。

当前默认改为 Full 4 Layers。地貌派生权重在进入材质采样前已经结合 Height、Slope、Curvature、Flow/Moisture 并归一化；Full-4 让 Grass、Soil、Rock、Snow 四层都按连续权重贡献 Albedo、Normal、AO、Metallic 和 Roughness。Top-2 会把较弱的两层归零，再对主、次两层重新归一化，因此原本宽而细腻的多层过渡可能变窄；Dominant/Auto 还会减少或随距离淡出次层 Normal/AO，容易使表面细节随相机距离变化。Full-4 视觉最稳定、材质交界最自然，因此作为编辑器默认画质基线；其余档位继续用于低端设备和性能比较。

TerrainMaterialHandle 为 0 时仍走无具体材质纹理的内建颜色路径。它既保持默认编辑器启动轻量，也可作为“不加载 11 张材质纹理”的基础对照；分配 DefaultTerrain.glterrainmat 后才进入完整纹理路径。

### Renderer 与诊断边界

TerrainRenderer 现在由 Scene 通过 BeginScene/EndScene 包围 Color Pass，并持有独立的非阻塞 GPUTimer。OpenGL 后端轮转 Time Query，只有结果可用时才读取；Debug UI 显示最近的 GPU ms、样本号、Terrain DrawCall 和实际绑定的材质纹理数，不会为统计等待 GPU。

TerrainSamplingBenchmarkTool 位于编辑器 Debug 目录，只读取 TerrainRenderer Statistics 并切换运行时采样模式，不持有 Scene、Entity 或 EditorLayer 内部状态。每次模式切换后先接收 15 个唯一 GPU Query 预热样本，再记录 30 个唯一耗时样本，避免重复统计旧结果。手动使用步骤：

1. 给 Terrain 分配包含具体纹理的 TerrainMaterial；
2. 固定相机和视口尺寸；
3. 打开 Window → Debug → Overview；
4. 点击 Start Terrain Benchmark，期间不要移动相机或缩放窗口；
5. 在面板或日志中比较 Full-4、Top-2 和 Dominant Normal/AO。

无人值守验证可设置 GLIMMER_TERRAIN_SAMPLING_BENCHMARK_AUTORUN=1 后启动编辑器。该入口只在本次诊断运行中给默认 Terrain 分配 DefaultTerrain、固定 EditorCamera，完成三档测试后正常退出，不保存场景。GLIMMER_TERRAIN_SAMPLING_VISUAL_MODE=0..3 可用同一固定场景单独打开四档画面，供截图对照。

### GTX 1050 验证

同一编辑器窗口、分辨率、DefaultTerrain、Alpine 地形和固定相机下，OpenGL 4.6 / NVIDIA GeForce GTX 1050 的结果为：

| Sampling Mode | Samples | Average | Minimum | Maximum | 相对 Full-4 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Full 4 Layers | 30 | 10.681 ms | 10.449 ms | 11.510 ms | 基线 |
| Top 2 Layers | 30 | 6.026 ms | 5.790 ms | 6.661 ms | 降低 43.6% |
| Top 2 + Dominant Normal/AO | 30 | 4.226 ms | 4.006 ms | 5.122 ms | 降低 60.4% |

Full-4、Top-2 和 Auto 固定视口截图已逐档检查，山体轮廓、草/岩混合和 Triplanar 投影方向一致，未发现新增条带接缝。Terrain 图形 Shader 与 GenerateFBM、ThermalErosion、DeriveTerrainMaps 均在 GTX 1050 上成功编译。Premake VS2026 重新生成、Debug x64 全解决方案构建和 64 项无窗口回归全部通过，构建输出目录未清理。

默认档位调整后新增回归断言，确认 TerrainRenderer 运行时状态和 Statistics 都以 Full-4 初始化；当前 97 项具体无窗口断言全部 PASS，VS2026 `Debug | x64` 编辑器目标构建成功，GTX 1050 / OpenGL 4.6 默认场景持续运行并成功加载 Terrain 与相关 Compute Shader。该选择明确偏向画质：既有基准中 Full-4 为 `10.681 ms`，Top-2 为 `6.026 ms`，因此性能受限时仍应手动选择 Top-2、Dominant 或 Auto，而不是误认为默认调整没有成本。

## HDR 环境 Cubemap 与 Mip Chain 基础

P10 的第一阶段先统一“环境源如何进入 Renderer”，尚未提前实现完整 IBL。现在 SkyLight 可以继续引用传统六面 `.glsky`，也可以直接使用 Radiance `.hdr` 等距柱状图；两种来源最终都得到遵守同一六面方向约定、带完整 Mip Chain 的 TextureCube。

### 导入与描述格式

直接把位于项目 `assets` 内的 `.hdr` 拖到 Viewport 或 Sky Light 的 Cubemap 属性即可。AssetManager 会将其注册为 `AssetType::Cubemap`，而不是普通 Texture2D。

如果需要一个可命名、可调整目标分辨率的环境资产，可以创建 `.glsky` 并使用：

```yaml
Cubemap:
  Source: "../textures/environment.hdr"
  Resolution: 512
```

`Source` 相对 `.glsky` 所在目录解析。Source 为空时仍使用原来的 `Right/Left/Top/Bottom/Front/Back` 六面字段，因此旧资产无需迁移。Content Browser、Viewport 拖放和 Cubemap Asset Inspector 均识别 `.hdr`；Inspector 会显示来源类型、实际源路径、格式、面尺寸、Mip 数和 Runtime 版本，并支持 Reload。

### 核心数据流

```text
.hdr / .glsky Source
  → AssetManager Cubemap Handle
  → Cubemap::Reload
  → EnvironmentMapLoader::LoadEquirectangularHDR
  → FloatImageData（线性 RGBA float）
  → 双线性 Equirectangular-to-Cubemap 转换
  → TextureCube RGBA16F
  → GenerateMipmaps，直到 1×1
  → SkyboxRenderer 可见背景
```

`EnvironmentMapLoader` 位于引擎核心 Renderer，而不是 EditorLayer 或 OpenGL 平台层。它处理经度循环、纬度钳制和 `+X/-X/+Y/-Y/+Z/-Z` 六面方向；OpenGL 后端只负责不可变存储、指定 Mip/面的数据上传和 Mip 生成。HDR 像素不会先压到 0～1，也不会执行 sRGB 解码，最终使用线性 `RGBA16F`。

TextureCube 规格新增显式 `MipLevels`，完整链的级数为 `floor(log2(faceSize)) + 1`。传统六面 LDR Cubemap 也会生成完整链并使用 Trilinear Min Filter。当前自动生成的是普通颜色下采样 Mip，只用于稳定可见天空盒采样并准备资源接口；它不能替代按 Roughness 卷积的 Specular Prefilter。

### Reload 与后续派生缓存

Cubemap 成功加载后记录实际源路径、是否 HDR 和递增 Runtime Version。该版本不是新的持久资产 ID；它用于后续把派生 IBL 缓存键定义为：

```text
Source Cubemap Handle + Runtime Version + Generation Parameters
```

这样 HDR 文件热重载或生成参数变化时，只失效对应 Irradiance、Prefilter 和 BRDF LUT，正常帧不会重复卷积。Diffuse Irradiance 与 Specular Prefilter 已在后续章节完成；当前还需继续实现 BRDF LUT。

### 验证

- VS2026 Premake 工程重新生成成功，`Debug | x64` 全解决方案构建成功；
- 71 项无窗口回归全部通过，覆盖完整 Mip 级数、六面中心方向、经纬图实际采样方向、HDR 高亮值保持、Radiance 文件解码和 `.hdr` 资产类型；
- 方向测试首次运行发现北极连续坐标在行号钳制后仍使用旧插值权重，现已改为先钳制连续纬度坐标再计算双线性权重；
- NVIDIA GeForce GTX 1050 / OpenGL 4.6 短时启动验证通过，Skybox、ToneMapping、PBR、Terrain 与三个 Compute Shader 均加载成功，无 OpenGL、Framebuffer 或 Shader 断言；
- 构建输出保留，未删除 `bin` 或 `bin-int`。

## Diffuse Irradiance 环境漫反射

P10 第二阶段已经让 SkyLight 不再只是背景。模型和地形现在会从同一个 Cubemap 派生低频环境漫反射，因此关闭 Directional Light 后，非金属表面仍能接收到来自天空不同方向和颜色的柔和照明。

### 运行链路

```text
Scene 中第一个启用的 SkyLightComponent
  → LightEnvironment：Cubemap Handle + Intensity
  → AssetManager::GetCubemap
  → EnvironmentLighting 派生缓存
  → TextureCube 浮点六面读回
  → EnvironmentMapLoader 余弦加权卷积
  → 32×32 RGBA16F Diffuse Irradiance
  ├─ Renderer3D / PBRModel：slot 8
  └─ TerrainRenderer / Terrain：slot 20
```

Scene 只提交可序列化的 Cubemap Handle 和 SkyLight Intensity；卷积数据、GPU TextureCube 和缓存统计不进入场景文件。EditorLayer 没有新增 OpenGL 调用或 IBL 算法。

### 漫反射积分

Diffuse Irradiance 保存的是法线半球上的：

```text
E(N) = ∫ L(ω) × max(dot(N, ω), 0) dω
```

实现使用确定性 Hammersley 序列进行余弦重要性采样，默认输出 `32×32` 六面图，每像素 64 个样本。对于常量环境，结果应为 `π × Radiance`，对应回归测试已验证。Shader 再使用 Fresnel-Schlick-Roughness 计算：

```text
Diffuse IBL = (1 - F) × (1 - metallic) × albedo × irradiance / π
              × AO × SkyLightIntensity
```

没有有效 SkyLight/Irradiance 时，Shader 保留原方向光 Ambient 回退。金属材质所需的环境镜面反射已由下一章节的 Specular Prefilter 补入；在 BRDF LUT 完成前，其能量与掠射角响应仍是阶段性近似。

### 派生缓存和失效

缓存键已经落实为：

```text
Cubemap AssetHandle
+ Cubemap Runtime Version
+ Irradiance Resolution
+ Irradiance Sample Count
```

同一活动键在后续帧直接复用，不执行 TextureCube 读回或卷积。点击 Cubemap Inspector 的 Reload 会递增 Runtime Version；修改生成参数也会形成新键。生成新版本后，同一源环境的旧版本内存项会被移除。当前缓存仅存在于进程内，重新启动编辑器仍会生成一次；持久化磁盘缓存和 LRU 内存预算留作后续基础建设。

### 如何查看效果

1. 场景中保留启用的 Sky Light，并给它分配 `.glsky` 或 `.hdr`；
2. 给模型使用 PBRModel 材质，或者观察 Terrain；
3. 暂时关闭 Directional Light 的 Enabled；
4. 非金属区域应保留随法线方向变化的环境颜色，而不是退化为统一黑色；
5. 调整 Sky Light Intensity，应同时改变可见天空盒和 Model/Terrain 环境漫反射强度。

### 验证

- VS2026 `Debug | x64` 的 Cyou 编辑器与回归测试项目增量构建成功；
- 74 项无窗口回归全部通过，新增覆盖常量环境余弦积分和缓存键的复用/版本/参数失效；
- NVIDIA GeForce GTX 1050 / OpenGL 4.6 下，默认 `32×32 / 64 samples` Irradiance 在相邻日志秒内完成且只记录一次生成；
- PBR Material Lab 成功渲染 6/6 模型，PBRModel、Terrain、ShadowDepth 和既有 Compute Shader 均编译通过；
- 未删除 `bin`、`bin-int`，构建产物保留。

## Specular Prefilter 粗糙度环境反射

P10 第三阶段加入了按粗糙度分级的环境镜面反射。模型和地形不再直接从可见 Skybox 的普通 Mip 猜测反射，而是从同一源 Cubemap 生成经过 GGX 卷积的专用 Prefilter Mip Chain。

### 生成与渲染链路

```text
SkyLight Cubemap
  → EnvironmentLighting 检查独立 Specular 缓存键
  → 读取一次线性浮点六面源数据
  → EnvironmentMapLoader：Hammersley + GGX 重要性采样
  → 64×64 RGBA16F Specular Prefilter，共 7 层 Mip
  ├─ Renderer3D / PBRModel：slot 9
  └─ TerrainRenderer / Terrain：slot 21

EnvironmentLighting 初始化
  → Hammersley + GGX 可见性积分
  → 64×64 RG16F Split-Sum BRDF LUT，128 samples
  ├─ Renderer3D / PBRModel：slot 10
  └─ TerrainRenderer / Terrain：slot 22

Fragment Shader
  → reflect(-V, N)
  → textureLod(prefilter, reflection, roughness × maxLod)
  → Prefilter × (F0 × BRDF.x + BRDF.y)
  → AO × SkyLightIntensity
```

Mip 0 直接采样源环境，保留低 Roughness 材质需要的清晰反射；Mip 1～6 逐级提高 GGX Roughness。材质 Roughness 越大，Shader 选择的 LOD 越高，太阳等集中高亮会扩散为更宽、更柔和的反射。派生链是专用卷积结果，不等同于 Skybox 的普通颜色下采样 Mip。

### 缓存边界

Diffuse 与 Specular 共用统一键结构，但由派生图类型隔离：

```text
Cubemap AssetHandle
+ Cubemap Runtime Version
+ Derived Map Type
+ Resolution
+ Sample Count
```

因此修改 Irradiance 参数不会误命中 Prefilter，修改 Prefilter 参数也不会强制重建仍然有效的 Irradiance。只有任一派生图缺失时才读回源 Cubemap，单次更新可复用这份 CPU 浮点数据完成所需生成；正常帧只绑定缓存纹理。同一 Handle Reload 后，旧 Runtime Version 项会被移除。

### Split-Sum BRDF LUT

BRDF LUT 与具体 HDR 环境无关，只描述 `N·V` 和 Roughness 对 GGX 镜面 BRDF 的 scale/bias。它在 Renderer 初始化时由 `EnvironmentMapLoader` 在 CPU 上预积分一次，上传为线性 `RG16F Texture2D`；切换或 Reload SkyLight 不会重新生成。Shader 已按标准 Split-Sum 公式消费：

```text
Specular IBL = PrefilteredEnvironment(R, roughness)
             × (F0 × BRDF.x + BRDF.y)
```

初版曾使用 `128×128 / 256 samples`，在 GTX 1050 的 Debug 构建中增加约 5 秒启动等待。二维 LUT 足够平滑且使用双线性采样，因此默认调整为 `64×64 / 128 samples`，实测在日志相邻秒内完成；设置接口仍允许后续离线生成更高质量版本。

### 如何查看效果

1. 给场景 Sky Light 分配含明显太阳或高亮区域的 `.hdr`；
2. 使用 PBRModel 或观察 Terrain，保持相机能看到环境高亮的反射方向；
3. 将 Metallic 调高以弱化漫反射，再从低到高调整 Roughness；
4. 低 Roughness 应看到较集中反射，高 Roughness 应平滑扩散；
5. 同一进程日志只应出现一次 BRDF LUT；同一环境持续运行时只出现一次 Diffuse 和一次 Specular 生成记录。

### 验证

- 78 项无窗口回归全部 PASS；除 Prefilter 测试外，新增验证 BRDF LUT 全部值有限且有界，并正确响应 Roughness 与掠射角 Fresnel；
- NVIDIA GeForce GTX 1050 / OpenGL 4.6 下生成 `64×64`、7 层、每像素 64 样本的 Prefilter，运行日志只记录一次生成；
- PBR Material Lab 渲染 6/6；`64×64 / 128 samples` BRDF LUT 在 GTX 1050 / OpenGL 4.6 下生成一次，`PBRModel`、`Terrain`、`ShadowDepth` 和三条地形 Compute Shader 均成功加载；
- 回归测试项目使用非增量链接重建，修复此前损坏的增量链接测试 EXE；未删除 `bin`、`bin-int`，编辑器构建产物保留。

## Terrain 固定 3×3 Chunk、LOD 与剔除

P11 把原先整块提交的 Terrain 拆成固定 `3×3` 区域，但不改变场景中的 `TerrainSpecification`、整体世界尺寸或 HeightMap 内容。在稳定 Chunk 坐标与剔除边界后，Color Pass 会为每块选择三档距离 LOD，并用 Skirt 遮盖不同分辨率接缝。

### 共享网格与坐标

`TerrainChunkLayout` 是 Terrain 核心目录中的纯 CPU 布局工具。对于原始 `MeshResolution`，共享 Chunk Mesh 分辨率按以下方式计算：

```text
SharedMeshResolution = ceil(MeshResolution / 3)
ChunkWorldSize       = TerrainWorldSize / 3
ChunkUVScale         = (1/3, 1/3)
```

九个 Chunk 不各自创建 Mesh，也不复制 Height、Normal/Slope、Analysis、Material Weights 或 TerrainMaterial 纹理。`TerrainRuntime` 只创建 LOD0、LOD1、LOD2 三份共享模板，分辨率约为 `1 / 1/2 / 1/4`；绘制每块时选择其中一份并上传 `UVOffset / UVScale / LocalOffset / LocalScale`。三档网格都会映射到相同的 Chunk 世界尺寸和全局 Height UV，因此改变密度不会缩小区域或重复地形。

```text
TerrainComponent / TerrainRuntime
  ├─ 一套 Height + Derived Maps + TerrainMaterial 绑定
  ├─ 三份 Shared TerrainMesh（LOD0 / LOD1 / LOD2）
  └─ TerrainChunkLayout[9]
       ├─ Color Pass：剔除后按距离选择 LOD Mesh
       └─ Shadow Pass：逐块 Bounds 测试后固定使用 LOD0
```

ShadowDepth 使用与颜色通道相同的 Chunk UV 和局部变换。Color Pass 与 `ShadowRenderer` 都不再把 Terrain 当成一个整体 Bounds：每块根据局部 XZ 范围和 HeightScale 建立 AABB，乘上 Terrain 实体 Transform 后分别测试 Camera ViewProjection 或当前 Cascade Light VP，剔除后才提交该块绘制。

判定使用共用的八角点保守算法：只有 AABB 八个角点全部落在同一个 Clip Plane 外侧时才剔除。横跨视锥边界的 Chunk 会继续绘制，以避免因包围盒部分可见而误删地形。

### 距离 LOD 与接缝

Color Pass 以 Chunk 世界中心到相机的距离选择 LOD0/1/2，默认中档和远档阈值为 `90 / 180` 世界单位。首次绘制直接按距离选择；之后保留每块上帧级别，只有越过阈值外侧 5 单位后才切换，避免相机在阈值附近轻微移动时反复跳级。选择完成后再约束四方向相邻块，使其最多相差一级。

不同密度边缘会形成 T-Junction。每份 `TerrainMesh` 因此在四边复制一圈顶点，Shader 用 `a_Skirt` 标记把它们向下延伸；竖向裙边覆盖潜在缝隙，但不改变表面 Height/Normal 采样。Shadow Pass 固定使用 LOD0，并同样绘制 Skirt，避免阴影轮廓随相机距离变化。

### 如何观察

打开 `Debug → Overview → Terrain`：完整可见时 Candidate 应为 9，Submitted 与 Frustum Culled 之和始终为 9，Shared Meshes 为 3。`LOD Chunks (0 / 1 / 2)` 显示本帧实际提交的各级数量，`Submitted Triangles` 会随远处块使用低级网格而下降。拖动 `LOD Distances` 可以立即调整中/远阈值；把两值调小应看到更多 LOD2，把两值调大则更多 LOD0。移动相机跨过阈值时检查边界，不能出现能看到天空盒或 Clear Color 的裂缝。

勾选 `Visualize Terrain LODs` 后，LOD0、LOD1、LOD2 分别覆盖为红、绿、蓝。它适合同时观察九块的分级、阈值迟滞和相邻级差；关闭后立即恢复正常 TerrainMaterial。无人值守或固定相机检查可以在启动前设置 `GLIMMER_TERRAIN_LOD_VISUALIZE=1`，该环境变量不保存到场景。

### 验证

- 88 项无窗口回归覆盖共享网格向上取整、九块完整覆盖、视锥判定，以及 LOD 分辨率、距离阈值、迟滞和相邻级差；
- VS2026 `Debug | x64` 回归测试与整解决方案构建成功；
- 最终 EXE 使用项目工作目录在 Intel Iris Xe / OpenGL 4.6 下持续运行 15 秒，Terrain、ShadowDepth、GenerateFBM、ThermalErosion 与 DeriveTerrainMaps 均成功加载，无断言、崩溃或 Shader 错误；
- LOD 调试着色通过 `GLIMMER_TERRAIN_LOD_VISUALIZE=1` 在同一真实 OpenGL 环境中启动验证；日志确认模式启用且新版 Terrain Shader 成功编译；
- 固定相机最终截图显示近、中、远区域连续呈现红、绿、蓝三档；不同颜色交界处未露出天空盒或 Clear Color，未观察到孤立越级 Chunk，P11 接缝与 LOD 分布验收通过；
- Color Pass Chunk 剔除接入后，最终 EXE 默认场景仍正常显示连续 Terrain，未发生可见 Chunk 误删；Camera Frustum 的内/外/边界/实体 Transform 判定由新增回归断言覆盖；
- 未删除 `bin`、`bin-int`，构建产物继续保留。

## Scene Depth 距离雾

P12 第一阶段在现有 HDR Scene Pass 和 Tone Mapping 之间接入距离雾。Scene Framebuffer 除 `RGBA16F` 场景颜色和整数 EntityID 外，显式保留一张可采样的 `Depth24Stencil8`；后处理阶段使用当前相机的逆 ViewProjection 将屏幕 UV 与深度还原到世界位置，再计算相机到该片元的真实世界距离。

雾在曝光和 ACES Filmic 之前混合，因此 Fog Color 与 Scene HDR Color 都处于线性空间，只经过一次 Tone Mapping 和 Gamma。深度接近 1 的像素代表没有场景几何，当前策略保留 Skybox 原色，避免把天空误当成远平面实体。

```glsl
vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
vec4 world = u_InverseViewProjection * clip;
world /= world.w;

float distanceToCamera = length(world.xyz - u_CameraPosition);
float rangeWeight = smoothstep(startDistance, endDistance, distanceToCamera);
float opticalWeight = 1.0 - exp(-density * max(distanceToCamera - startDistance, 0.0));
float fogWeight = rangeWeight * opticalWeight;
linearColor = mix(linearColor, fogColor, fogWeight);
```

### 使用方法

打开 `Settings → Distance Fog`：

- `Enabled`：开启或关闭距离雾；
- `Density`：控制距离增加时的指数衰减速度；
- `Start / End`：控制近景保护区和完全进入远景雾的范围；
- `Color`：线性 HDR 雾色。
- `Color Source`：选择 Manual、Sky Light 或 Directional Light；Sky Light 按当前视线方向读取环境 Cubemap 的模糊低 Mip，Directional Light 使用首个启用主光的 Color×Intensity；来源不可用时回退 Manual；
- `Height Fog`：开启指数高度密度；`Base Height` 是参考雾层高度，`Height Falloff` 越大，雾随世界高度上升衰减越快。

默认参数为 Density `0.012`、Start `60`、End `260`。调试启动可以设置 `GLIMMER_DISTANCE_FOG_VISUALIZE=1`；该开关和全部雾参数当前只存在于编辑器会话，不保存到 Scene YAML。

高度雾不是简单地用片元终点高度乘权重，而是沿相机到片元的整条射线积分指数密度：

```glsl
float cameraDensity = exp(-falloff * (cameraY - baseHeight));
float denominator = falloff * (fragmentY - cameraY);
float heightIntegral = abs(denominator) > epsilon
    ? cameraDensity * (1.0 - exp(-denominator)) / denominator
    : cameraDensity;
```

因此俯视低谷时整段低空路径会积累更多雾，高处山脊和相机附近细节相对清晰；相机穿过 Base Height 时公式连续。实现对指数输入和积分结果进行了钳制，避免极端调试参数生成 Inf/NaN。

### 验证

- VS2026 `Debug | x64` 整解决方案构建成功，88 项无窗口回归全部 PASS；
- Intel Iris Xe / OpenGL 4.6 下 ToneMapping、Terrain、ShadowDepth、GenerateFBM、ThermalErosion 与 DeriveTerrainMaps 均成功编译；
- 固定 Terrain 相机截图确认近景保留原材质对比度，远景逐步向雾色衰减，天空深度不参与世界位置雾化；无 Shader 错误、断言或崩溃；
- 高度雾 + SkyLight 色源固定相机截图确认环境色调一致、远处低地衰减增强且近景高处仍保留细节；重新构建回归目标后 88 项无窗口测试继续全部 PASS；
- 下一阶段校准 ACES/曝光并评估 Bloom，当前实现仍不代表完整大气散射。

## HDR Bloom 后处理

P12 的首版 Bloom 复用现有 HDR Scene Color，不向材质或光源增加专用发光标记。超过软阈值的高亮区域进入半分辨率 `RGBA16F` Ping-Pong 缓冲，经过水平/垂直高斯模糊后加回 Scene HDR。

```text
Scene RGBA16F
  → Bloom Extract（EV 后亮度阈值，保留未曝光 Radiance）
  → Half Resolution RGBA16F
  → Horizontal / Vertical Gaussian Blur
  → Scene + Bloom
  → Distance / Height Fog
  → 2^EV
  → ACES White Point
  → Gamma
```

提取阶段使用 Threshold 和 Soft Knee。Threshold 决定明确进入 Bloom 的显示亮度，Soft Knee 在阈值附近建立平滑过渡，避免高光边缘突然截断。阈值判断乘当前 EV，使用户调节曝光时 Bloom 感知阈值保持一致；输出仍保存原始 HDR Radiance，因此合成后只统一乘一次 EV。

Settings 的 `Bloom` 区域提供：

- `Enabled`：完全跳过或执行 Bloom Pass；
- `Threshold`：高光提取阈值，默认 `1.0`；
- `Soft Knee`：阈值过渡宽度，默认 `0.5`；
- `Intensity`：加回 Scene HDR 的强度，默认 `0.08`；
- `Blur Passes`：半分辨率水平/垂直模糊次数，默认 `6`，范围 `1～12`。

### 验证

- VS2026 `Debug | x64` 整解决方案构建成功，88 项无窗口回归全部 PASS；
- Intel Iris Xe / OpenGL 4.6 下 BloomExtract、BloomBlur、ToneMapping、Terrain、ShadowDepth 与三条 Terrain Compute Shader 均成功编译；
- 固定 Terrain 相机画面中，默认参数只在太阳等 HDR 高光周围产生柔和扩散，地形中间调没有整体泛白；高度雾继续衰减远处 Bloom，没有出现光晕穿透雾层；
- 当前为经典双缓冲高斯 Bloom；Mip Pyramid/Kawase、Lens Dirt 和自动曝光联动属于后续优化，不是首版范围。

## TAA 接入评估

P12 对 Temporal Anti-Aliasing 做了管线级评估，但没有加入只有历史颜色 Alpha 混合的简化版本。当前 Glimmer 已有 Scene HDR、Depth、EntityID、Bloom 和 Tone Mapping，但缺少完整 TAA 所需的时域数据：

- 相机投影没有 Halton 等亚像素 Jitter；
- Renderer 不保存上一帧 ViewProjection；
- Scene FBO 没有 Motion Vector/Velocity 附件；
- Entity/Renderer3D Instancing/Sprite 不保存上一帧 Transform；
- 没有 HDR History Ping-Pong、Resize/Play/Stop/场景切换/相机跳变失效规则；
- 没有邻域 Clamp、反遮挡判断和透明响应 Mask。

只使用当前 Depth 重建世界位置并投影到上一帧，最多只能正确处理静态地形和静态相机运动。移动模型、GPU Instancing、Sprite、Blend 透明物体及编辑器 Gizmo 会缺少自身速度，产生拖影或历史残留。因此本阶段不引入会降低编辑器可靠性的“伪 TAA”。

后续正式 TAA 的最低接入顺序为：

```text
Projection Jitter
→ Current / Previous Clip Position
→ RG16F Velocity Attachment
→ HDR History Ping-Pong
→ Depth Disocclusion + Neighborhood Clamp
→ Reactive Mask for Transparent / Emissive
→ History Reset on resize, scene/camera/state changes
```

EntityID 不参与历史混合，拾取仍读取当前帧整数附件；TAA 只处理 HDR Scene Color，并且应在 Bloom 提取之前稳定当前场景颜色。达到上述边界后再重新评估接入。

## 后处理渲染器职责收拢

P12.1 将原本直接写在 `EditorLayer` 中的 Bloom 与 Tone Mapping 执行逻辑迁入引擎侧 `PostProcessRenderer`。这次调整不改变画面公式、默认参数或 Settings 操作，只收拢资源所有权和 Pass 边界，避免后续水文模拟继续扩大编辑器协调层。

```text
EditorLayer
  ├─ 渲染 Scene HDR / EntityID / Depth
  ├─ 收集 Camera、SkyLight、DirectionalLight 输入
  └─ PostProcessRenderer::Execute(input)
       ├─ Bloom Extract
       ├─ Half-res Ping-Pong Blur
       ├─ Fog / EV / ACES / Gamma
       └─ 输出 Display Texture
```

`PostProcessRenderer` 现在负责 Display Framebuffer、两张 Bloom Framebuffer、三张后处理 Shader 引用、Viewport Resize 和 Pass 执行；其 `PostProcessSettings` 集中保存 Bloom、雾、曝光、ACES 与灰度参数。Editor Settings 仍直接编辑这些纯运行时参数，它们不会写入 Scene YAML。Shader 继续加入同一个 `ShaderLibrary`，原有自动/手动热重载工作流不变。

### 验证

- Premake 已将新增的 `PostProcessRenderer.cpp` 纳入 Glimmer 静态库工程；
- VS2026 `Debug | x64` 整解决方案构建成功；
- 88 项无窗口回归全部 PASS；
- 最终编辑器以项目工作目录和 `GLIMMER_DISTANCE_FOG_VISUALIZE=1` 启动并持续运行 15 秒，无提前退出；现有 Settings 操作、Viewport 输出和 Scene EntityID 拾取边界保持不变。

## 固定步长水文 CPU 参考模型

P13A 首先建立不依赖窗口和 GPU 的 `TerrainHydrologyRuntime`，用小网格固定算法契约，再迁移到 Compute Shader。它与有限次数的 Authoring Thermal Erosion 完全分离：Height 是水文初始化时的只读快照，Water、四向 Flux 和 Velocity 是独立运行时字段，不会写入 Scene YAML。

```text
旧 Height + 旧 Water + 旧 Flux
  → 四邻域水面高差
  → Left / Right / Down / Up Flux
  → 按当前可用水量缩放总出流
  → 汇总邻居入流与自身出流
  → 新 Water + Velocity
  → 固定步完成
```

首版使用封闭边界，边缘不会把水排出网格。每格的总出流如果超过当前水量在本步内能提供的体积，会统一缩放四个方向，因此 Water 不会因为一次过大的高差而变为负数。降雨以 `深度/秒` 加入每个单元，并计入质量统计。

### 固定步长操作语义

- `Play`：允许 `Advance(frameDelta)` 将帧时间累积并执行固定步；
- `Pause`：帧时间不推进模拟；
- `SingleStep`：只在暂停状态执行一个固定步；
- `Reset`：恢复初始化 Height/Water，清空 Flux、Velocity、累计时间和统计；
- `MaxSubsteps`：限制单帧追赶次数，超出的完整步时间记入 `DroppedTime`，避免卡顿后发生“螺旋式补帧”。

统计包含 Step Count、Simulated Time、Accumulator、Dropped Time、Water/Rainfall Volume、Mass Error、最小/最大水深、最大速度和有限性。当前阶段只有代码接口和无窗口测试，没有新增编辑器操作；下一阶段才会接入 GPU Water/Flux/Velocity Ping-Pong、Terrain Height 初始化和 DebugPanel 控制。

### 验证

- 相同 1 秒模拟分别以 `0.04×25` 与 `0.01×100` 帧输入，执行相同固定步数并得到一致 Water/Velocity；
- 封闭边界无降雨时质量误差小于 `1e-5`，水深非负且所有水量/速度有限；
- 三格山峰测试中水从高水面流向左右低处，盆地测试中低格蓄水多于两侧高格；
- 8 条新增水文断言与原有测试合计 96 项具体无窗口断言全部 PASS；VS2026 `Debug | x64` 整解决方案构建成功。

## GPU 水文与 Debug 可视化

P13A 第二阶段将 CPU 参考模型的字段和固定步语义迁移到 GPU。程序化 Terrain 的 Height 保持只读，水文状态采用三组独立 Ping-Pong：

| 字段 | 格式 | 用途 |
| --- | --- | --- |
| Water | `R32F × 2` | 当前/下一步水深 |
| Flux | `RGBA16F × 2` | 左、右、下、上四向流量 |
| Velocity | `RGBA16F × 2` | XY 保存地形平面速度，ZW 预留 |

```text
Height(Read) + Water(Read) + Flux(Read)
  → HydrologyFlux.comp
  → Flux(Write)
  → Barrier + Swap
  → HydrologyUpdate.comp
  → Water(Write) + Velocity(Write)
  → Barrier + Swap
```

两次 Dispatch 之间使用全局 Memory Barrier，避免把 Workgroup 内屏障误当成整张纹理完成信号。普通 PNG/JPG 高度图不是 `R32F` Storage Texture，因此首版只对程序化 Terrain 建立 GPU 水文状态。

### 如何观察

打开 `Debug → Overview`，在 Terrain 区域底部找到 `Runtime Hydrology`：

1. 勾选 `Visualize Water Depth`；初始 Water 为 0，因此画面不变；
2. 将 `Rainfall` 保持默认 `0.020 depth/s`，勾选 `Play`；
3. 等待数秒，低地和沟谷会逐渐出现蓝色覆盖；深蓝表示蓄水，偏亮青色表示流速更高；
4. 取消 `Play` 后点击 `Single Step`，每次只推进一个固定步，便于观察边界变化；
5. 点击 `Reset` 会把 Water、Flux、Velocity、累加器和统计全部清零；
6. 暂停后点击 `Validate / Readback`，查看 Water Volume/Error、Depth Min/Max、Max Speed 和 `Finite: PASS`。

`Validate / Readback` 检查当前场景正在运行的水文状态。`Run GPU Contract` 则执行一个独立的受控验证：临时创建 `3×1` 的高-低-高盆地，用同一 GPU 水文实例先后按 `0.04×25` 和 `0.01×100` 两种帧划分运行 100 个固定步，并自动检查：

- Water/Velocity 有限且 Water 非负；
- 相对质量误差不超过 `2e-3`；
- 中央低地水深显著高于两侧高地；
- 两种帧划分的最终 Water 最大差值不超过 `5e-4`。

跨设备或无人值守验证可在启动编辑器前设置 `GLIMMER_HYDROLOGY_VALIDATE=1`。验证只执行一次并输出 `GPU hydrology contract validation PASS/FAIL`，正常帧不会创建临时验证资源或触发同步读回。

水深显示目前是 Terrain Fragment Shader 中的诊断着色，不会抬高网格，也没有折射、反射、透明水面或岸线泡沫。它用于确认流向和蓄水位置，正式水体几何与材质属于后续渲染阶段。

### 当前验证

- Premake VS2026 工程重新生成成功，`Debug | x64` 解决方案及编辑器增量构建成功；
- 104 项无窗口回归全部 PASS，覆盖 CPU 水文方向、守恒、非负、重置、补帧上限、盆地蓄水和帧划分确定性；
- GTX 1050 / OpenGL 4.6 以 `GLIMMER_HYDROLOGY_VALIDATE=1` 运行真实 Compute 链路并 PASS：相对质量误差 `7.38228e-7`，中央盆地水深 `0.599998`、两侧最大水深 `6.28643e-7`，两种帧划分最大差值 `0`；
- HydrologyFlux、HydrologyUpdate、Terrain 和既有图形 Shader 均成功创建，编辑器无断言或提前退出；P13A 数值验收完成。

## Tone Mapping 跨驱动 Sampler 修复

一次跨电脑拉取后，GTX 1050 / NVIDIA 531.29 上的 Viewport 只显示黑色，而相同代码在另一台电脑上可以正常显示。RenderDoc 证明 Scene、Terrain、Bloom 和最终全屏命令都已提交；真正失败的是 Tone Mapping 的最后一次 `DrawElements(6)`：

```text
GL_INVALID_OPERATION: State(s) are invalid: program texture usage.
```

`ToneMapping.glsl` 同时声明 `sampler2D u_SceneTexture` 和 `samplerCube u_FogSkyLight`。旧实现只在 Fog Color Source 选择 Sky Light 时才把 Cube sampler 设置到 slot 2；默认 Manual 模式下它保持 GLSL 默认值 0，与 Scene Texture 的 slot 0 冲突。OpenGL 会验证整个已链接 Program 的 sampler 类型，即使本帧不执行 Cubemap 采样分支；严格驱动因此拒绝 Draw，Display FBO 只留下此前成功写入的黑色 Clear。较宽松驱动或已有 Uniform 状态可能掩盖问题，但不能作为合法行为依赖。

PostProcessRenderer 现在每帧显式声明完整绑定契约：

| Slot | Sampler | 类型 |
| ---: | --- | --- |
| 0 | `u_SceneTexture` | `sampler2D` |
| 1 | `u_SceneDepth` | `sampler2D` |
| 2 | `u_FogSkyLight` | `samplerCube` |
| 3 | `u_BloomTexture` | `sampler2D` |

Fog 或 Bloom 关闭时也保留不冲突的 Uniform 槽位；只有实际需要时才绑定对应可选纹理。这样 Shader 热重载、默认设置和不同驱动都不再依赖上一次 Program 状态或 sampler 默认值。

验证：VS2026 `Debug | x64` 编辑器目标增量构建成功，立即重复构建没有重新编译源文件；GTX 1050 默认 Manual Fog 下 Terrain/Skybox 画面恢复；修复后 RenderDoc API Validation 捕获包含最终 Tone Mapping Draw，且没有 High severity、`GL_INVALID_OPERATION` 或 `program texture usage`。`bin`、`bin-int` 均保留。

## 模型导入边界与 Assimp 子模块准备

这一阶段没有直接宣称“FBX 已支持”，而是先解决原模型系统最关键的耦合：旧 `Model.cpp` 同时解析 OBJ、计算切线、读取 MTL 并创建 GPU Mesh，后续若直接加入 Assimp，就会让 FBX 节点、骨骼、动画和材质处理全部进入 Renderer。

当前模型数据链调整为：

```text
OBJ 源文件
  → ModelImporter（按扩展名分发）
  → ObjModelImporter / tinyobjloader
  → MeshSource（纯 CPU 中间数据）
  → Model
  → Mesh / VAO / VBO / IBO
  → Renderer3D
```

`MeshSource` 保存源路径、Submesh、统一 `MeshVertex` 和源材质描述，不创建 Texture、Buffer 或任何 OpenGL 对象。`ObjModelImporter` 接管原有 OBJ 三角化、按材质拆分、顶点去重和稳定切线生成；`Model` 只负责把有效 Submesh 转换成运行时 Mesh。因此未来的 `AssimpModelImporter` 只需把 `aiScene` 转换到同一个 MeshSource，Renderer3D 不需要知道源文件来自 OBJ、FBX 还是 glTF。

Assimp 使用官方 Git 子模块并固定在 `v6.0.5`：

```text
Glimmer/vendor/assimp
commit 392a658f9c271be965271f45e7521a1b80ea4392
```

新设备初始化依赖：

```bat
git submodule update --init --recursive
```

Assimp 不加入 Glimmer Premake 项目逐文件编译，而是通过上游 CMake 独立生成静态库：

```bat
scripts\Win-BuildAssimp-vs2026.bat Debug
scripts\Win-BuildAssimp-vs2026.bat Release
```

脚本进入 VS2026 x64 Developer Environment，并使用 NMake 生成单配置构建目录。选择 NMake 是因为本机 CMake 4.3.3 配合 `Visual Studio 18 2026` Generator 时，两次停在 `CompilerIdC.vcxproj`；同一编译器在 Developer Environment 下可正常探测和编译。构建固定使用静态 CRT、关闭 Exporter/Tests/Tools/Samples/Docs，并将 importer 缩减为 OBJ、FBX、GLTF。生成结果位于忽略目录：

```text
Glimmer/vendor/assimp-build/vs2026-Debug/lib/assimp-vc145-mtd.lib
Glimmer/vendor/assimp-build/vs2026-Debug/contrib/zlib/zlibstaticd.lib
```

该准备阶段当时的能力边界（已由下一节继续推进）：

- `.obj`：继续支持，现已先转换成 MeshSource，再创建运行时 Mesh；
- `.fbx`、`.gltf`、`.glb`：Assimp 库本身已经按这些 importer 构建，但 Glimmer 尚未实现 AssimpModelImporter，也没有在 AssetManager 中注册这些扩展名，因此目前仍不能加载；
- `.glmesh`：尚未实现。MeshSource 目前只存在于内存中，关闭编辑器后仍会重新解析 OBJ；
- OBJ/MTL 的 BaseColor 纹理仍沿用直接 Texture2D 路径，尚未转换为 AssetHandle 与 `.glmat`，后续烘焙阶段再统一；
- Assimp 类型不得进入 Scene、Renderer3D、组件或公共 Model API，它只允许存在于 importer 私有实现中。

本阶段验证结果：Premake VS2026 工程生成成功；Assimp Debug 静态库完整构建并确认只启用 OBJ/FBX/GLTF，立即重复脚本约 5 秒完成且没有重新编译源文件；OBJ→MeshSource 新增 3 条无窗口回归，连同既有功能共 100 项断言全部 PASS；`GlimmerEditor-CyouBranch` 的 `Debug | x64` 目标构建成功，立即重复构建只检查并输出既有目标。`bin`、`bin-int` 和 Assimp 独立构建产物均保留。

下一阶段已在下节完成静态 FBX importer 与外部 PBR 贴图加载；版本化 `.glmesh`、自动 `.glmat` 烘焙、单位归一化、Skeleton、Animation、Morph Target 和 glTF 仍应分阶段建设。

## 静态 FBX 与 Cerberus PBR 材质加载

本阶段把 Assimp 从“可以独立编译”推进到实际引擎数据链。`.fbx` 现在会被 AssetManager 识别为 Model，并通过专用 `AssimpModelImporter` 转换为与 OBJ 共用的 MeshSource：

```text
FBX + 外部纹理
  → AssimpModelImporter（仅 importer 私有 Assimp 类型）
  → MeshSource / SubmeshSource / MeshMaterialSource
  → Model（按 MaterialIndex 共享纹理解码）
  → Mesh
  → Renderer3D / PBRModel
```

静态导入启用了 Triangulate、JoinIdenticalVertices、GenSmoothNormals、CalcTangentSpace、ImproveCacheLocality、SortByPType、ValidateDataStructure 和 PreTransformVertices。最后一项把 FBX 节点 Transform 烘焙进顶点，因此当前结果适合静态场景模型，但不会保留原节点层级、骨骼或动画。

材质读取支持 BaseColor、Normal、Metallic、Roughness、AO 和 Emissive。Cerberus FBX 本身只保存了 `Textures/Cerberus_A.tga` 的引用，其余贴图不在 FBX 材质连接中；importer 因此只在模型相邻目录、`Textures` 和 `Textures/Raw` 内按受限后缀查找 `_N`、`_M`、`_R`、`_AO`，不会递归扫描整个项目或按模糊名称随机匹配。Cerberus 最终解析到：

```text
Cerberus_A.tga       Base Color / sRGB
Cerberus_N.tga       Normal / Linear
Cerberus_M.tga       Metallic / Linear
Cerberus_R.tga       Roughness / Linear
```

当前版本化样本不包含 AO。IBL 示例实体不再用 `Cerberus_A.tga` 冒充 AO，也不再引用不存在的 `Textures/Raw/Cerberus_N.tga`；Normal、Metallic 与 Roughness 均由模型的导入贴图回退路径提供。

Renderer3D 的覆盖顺序是：实体 MaterialInstance 中显式存在的 BaseColor、Normal、AO、Emissive 贴图优先，缺失通道使用模型导入贴图；Metallic/Roughness 当前由模型导入贴图覆盖 `.glmat` 的标量。新采样器使用 unit 11/12，避开已有的 0～3 材质、4～7 CSM 与 8～10 IBL。一个 FBX 材质被多个 Submesh 使用时只解码和上传一套纹理。

### 使用流程

1. 新设备先初始化子模块，并构建对应配置的 Assimp 静态库：

   ```bat
   git submodule update --init --recursive
   scripts\Win-BuildAssimp-vs2026.bat Debug
   ```

2. 把 FBX 连同其相对纹理目录复制到 `GlimmerEditor-CyouBranch/assets` 内。例如应保留 `Cerberus_LP.FBX` 与同级 `Textures` 的关系。Content Browser 出于项目资产边界不会导入 `tmp` 外部路径。
3. 在实体上添加 Model Renderer 和 Material 组件，把 FBX 拖到 Model，把一个使用 PBRModel Shader 的 `.glmat` 拖到 Material。`.glmat` 提供 Shader 和可编辑覆盖值，FBX 提供缺失的导入纹理。
4. Content Browser 导入后会把 FBX Handle 写入 AssetRegistry；Scene YAML 仍只保存 ModelHandle/MaterialHandle，不保存 Assimp 对象或 GPU ID。

`assets/models/Cerberus` 当前包含版本化的 FBX 与 A/N/M/R 四张 TGA，并作为跨设备回归样本。该资源来源此前标记为仅限非商业教育用途，但仓库中尚未包含原许可说明文件；公开分发或商业使用前必须补齐可再分发许可，无法确认时应从发布资产中移除。

### 当前边界

- 仅开放 `.fbx` 静态网格；`.gltf/.glb` 虽已在 Assimp 构建中启用，但尚未注册；
- 不支持骨骼、动画、Morph Target、保留节点层级或嵌入纹理；
- 未额外进行厘米/米单位归一化，使用 Assimp 实际输出；不同 DCC 来源仍需建立明确导入设置；
- 导入纹理由 Model 运行时持有，尚不生成独立 AssetHandle、`.glmat` 或版本化 `.glmesh`；
- `.glmat` 目前没有 Metallic/Roughness Texture 字段，只有 FBX 导入回退路径能使用这两张独立贴图；ORM 打包也未实现；
- Tangent 仍是 vec3，没有保存镜像 UV 所需的 handedness。

### 验证

- Cerberus FBX 被解析为有效三角 Submesh，顶点与索引非空，全部切线有限且长度大于 0.9；
- 正式 assets 中的 Cerberus A/N/M/R 四张 TGA 路径全部解析成功；
- 104 项无窗口回归断言全部 PASS，测试不再因本机 `tmp` 缺失而静默跳过 FBX；
- GlimmerEditor-CyouBranch `Debug | x64` 成功链接 Assimp 和 zlib，最终 EXE 持续运行 15 秒，无 Shader 断言或提前退出；
- `bin`、`bin-int` 与 Assimp 独立构建产物均保留。

## Assimp 新设备构建自修复

新设备首次构建时报错：

```text
fatal error C1083: 无法打开包括文件: "assimp/config.h": No such file or directory
```

这不是 `vendor/assimp/include` 写错。Assimp 源码子模块只提交 `include/assimp/config.h.in`，上游 CMake 会根据平台与启用的 importer 生成真正的：

```text
Glimmer/vendor/assimp-build/vs2026-Debug/include/assimp/config.h
Glimmer/vendor/assimp-build/vs2026-Release/include/assimp/config.h
```

因此仅拉取子模块、生成 Premake 工程还不够；对应配置的生成目录必须存在。现在 Glimmer 的 VS 工程在 PreBuildEvent 中执行快速检查：

```text
构建 Glimmer Debug
  → Win-EnsureAssimp-vs2026.bat Debug
  → 校验 config.h、Assimp/zlib、CMake 配置和 Glimmer 构建指纹
  → 子模块提交、配置、ccache 状态全部匹配：立即继续
  → 任一缺失或过期：调用 Win-BuildAssimp-vs2026.bat Debug
  → 上游 CMake/NMake 构建完成
  → 编译 Glimmer
```

Release/Dist 同理使用 Release 产物。构建脚本通过 `vswhere` 限定查找 VS 18.x/2026，并验证激活的是 v145，不再假定安装在 C 盘；CMake 优先使用 VS 自带版本。脚本还显式关闭 Assimp 的 ccache，因为 PATH 中的 MinGW `ccache.exe` 可能错误包裹 MSVC `lib.exe`，出现 CMake 报告链接成功但 `.lib` 实际未生成的情况。

成功构建后会写入被忽略的 `glimmer-assimp-build.stamp`，记录 Schema、配置、Assimp 子模块提交、工具集与 ccache 状态。Ensure 同时核对该指纹和 `CMakeCache.txt`，所以旧电脑留下的 `ccache=ON` 缓存或更新后的 Assimp 子模块不会再仅凭三个旧文件被误判为可用。PreBuild 路径基于 `$(ProjectDir)`，既支持解决方案构建，也支持单独构建 `Glimmer.vcxproj` 或依赖它的测试工程。

一般操作只需初始化子模块、生成工程并正常构建：

```bat
git submodule update --init --recursive
scripts\Win-GenerateProject-vs2026.bat
```

也可以运行完整自动验证：

```bat
scripts\Verify-Windows.bat
```

需要主动重配 Assimp，而不是仅在缺失时补齐，可手动执行：

```bat
scripts\Win-BuildAssimp-vs2026.bat Debug
scripts\Win-BuildAssimp-vs2026.bat Release
```

本次在旧缓存仍为 `ASSIMP_BUILD_USE_CCACHE=ON` 的设备上验证：新版 Ensure 会触发一次原地重配并写入构建指纹，之后快速命中约 118 ms；重新生成 VS2026 工程后，测试工程可脱离解决方案单独构建。回归测试关闭 Debug 增量链接，避免损坏的 `.ilk` 生成无系统导入表的 EXE；定向 Rebuild 后 104 项断言全部通过，完整 `Verify-Windows.ps1` 通过，编辑器保持运行 10 秒无提前退出，`bin` 与其余构建产物均保留。

## CPU 无源泥沙输运

P13B 第一阶段在 `TerrainHydrologyRuntime` 中增加独立的 Sediment 状态。它表示每格单位地表面积上的悬浮泥沙质量，不是地形高度，也不是 Water 的颜色通道；初始化快照、Reset 和统计均与 Water 分离。

每个固定步在水流 Flux 已确定后执行有限体积输运：

```text
旧 Sediment × CellArea
  → 当前格悬浮泥沙质量
÷ ((旧 Water + RainfallDepth) × CellArea)
  → 水中泥沙浓度
× 四向 Water Flux
  → 四向泥沙质量流率
  → 按当前可用泥沙质量限制总外运
  → 汇总邻格入流 - 本格出流
  → 新 Sediment
```

雨水只稀释浓度，不产生泥沙。首版边界与水文一致，四周封闭，因此 `SedimentBoundaryLoss` 为 0；统计使用“当前质量 + 边界损失 - 初始质量”计算 `SedimentMassError`。逐格外运限幅避免一次固定步带走超过现有质量的泥沙，最终统计同时检查 Sediment 非负和有限。

当前阶段刻意不加入携沙能力、侵蚀或沉积：这些机制会形成 Sediment 与 Height 间的质量交换，需要单独定义源项和地形质量预算。现有 Transport Pass 只搬运既有悬浮泥沙，Height 保持逐值不变，也不会进入 Scene YAML 或污染有限次 Authoring Erosion。

本阶段是 CPU 数值基线，编辑器画面暂时不会显示泥沙。验证覆盖泥沙随水流向下游迁移、Water/Sediment Reset、封闭边界守恒、非负/有限、两种帧划分确定性和 Height 不变；新增 3 项断言后共 107 项无窗口回归全部 PASS，VS2026 `Debug | x64` 编辑器增量构建成功。下一步会按同一质量契约增加 GPU `R32F` Sediment Ping-Pong 和独立 Compute Transport Pass。

## GPU 泥沙输运与诊断显示

P13B 第二阶段把 CPU 的无源守恒契约迁移到 GPU。`TerrainHydrologyGPU` 新增独立 `R32F × 2` Sediment，不复用 Water 或 Terrain 派生图；每个固定步现在包含三个 Compute Pass：

```text
Height + 旧 Water + 旧 Flux
  → HydrologyFlux → 当前 Flux
旧 Water + 当前 Flux
  → HydrologyUpdate → 新 Water + 新 Velocity
旧 Water + 当前 Flux + 旧 Sediment
  → SedimentTransport → 新 Sediment
  → Barrier → Water / Velocity / Sediment Swap
```

Sediment Transport 对本格和四邻格使用同一个纯读取流率函数：把悬浮质量除以“旧 Water + 本步 Rainfall”得到浓度，再乘当前 Water Flux 得到质量流率；外运总量不能超过本格现有泥沙。这样无需原子加法，也不会在同一 Dispatch 中依赖其它 Workgroup 的写入结果。Pass 只写 Sediment，不能修改 Height、Water 或 Authoring Erosion。

### 如何查看

打开 `Debug → Overview → Runtime Hydrology`：

1. 将 `Visualization` 选择为 `Suspended Sediment`；
2. 调整 `Sediment Seed`，默认 `1.0 mass/area`；
3. 点击 `Apply Seed`，这只写入一次初始状态，不会每帧生成泥沙；
4. 保持 Rainfall 大于 0 并勾选 `Play`，棕橙色覆盖会随水流重新分布；
5. 暂停并点击 `Validate / Readback`，查看 Sediment Mass/Error 与 Min/Max；
6. 点击 `Run GPU Contract` 可检查标准盆地中的守恒、下游迁移和帧划分确定性。

Terrain Shader 固定使用 slot 23/24/25 读取 Water、Velocity 和 Sediment。诊断模式为互斥的 None/Water/Sediment，避免两种覆盖互相污染；棕橙色只是数值可视化，不代表最终泥水材质或地形颜色已经改变。

### 验证

- `SedimentTransport.comp`、HydrologyFlux、HydrologyUpdate 与 Terrain Shader 在 GTX 1050 / OpenGL 4.6 上全部编译成功；
- GPU Contract：泥沙相对质量误差 `0`，源格从 `1` 降至 `0`、下游低地从 `0` 增至 `1`，两种帧划分最大差值 `0`；既有水量相对误差仍为 `7.38228e-7`；
- VS2026 `Debug | x64` 编辑器增量构建成功，最终核心库重新链接后的 107 项无窗口回归全部 PASS；
- 三个水文 Compute Shader 现已实际接入 `ReloadShadersIfChanged` 轮询，修改成功时事务式替换，失败时保留上一有效程序。

该阶段仍没有侵蚀或沉积源项。后续只读 Capacity/Saturation 诊断已经完成 P13B；只有 P13C 才允许根据容量差异交换 Height 与 Sediment 质量。

## 泥沙携沙能力与饱和度诊断

P13B 最后阶段为 CPU/GPU 水文状态增加两个只读派生场：

```text
Capacity = CapacityScale × WaterDepth × Speed
Saturation = Sediment / Capacity
```

Capacity 表示当前水流每单位地表面积可承载的悬浮质量；Saturation 小于 1 表示欠饱和，接近 1 表示接近平衡，大于 1 表示过饱和。为了避免干格除零，Capacity 小于 `1e-6` 且仍有 Sediment 时使用上限 `1000` 表示强过饱和；没有 Sediment 时为 0。这个上限只是稳定的诊断编码，不是侵蚀率。

CPU `TerrainHydrologyRuntime` 在 Water、Velocity、Sediment 完成固定步更新后重新计算两个数组，并将 Min/Max 与有限性纳入统计。改变 Capacity Scale 会立即重算派生场，但不会改变 Water、Sediment 或 Height。

GPU 使用独立 `SedimentCapacity.comp`，在 Water/Velocity/Sediment 完成 Swap 后读取最终状态，写入单缓冲 `R32F` Capacity 与 Saturation。派生结果没有下一步历史依赖，因此不使用 Ping-Pong；它们也不会写回 Height 或参与 P13B 的泥沙质量预算。四个水文 Compute Shader 均进入原有事务式热重载轮询。

Terrain Shader 的诊断纹理槽位现为：

- 23：Water Depth；
- 24：Water Velocity；
- 25：Suspended Sediment；
- 26：Sediment Capacity；
- 27：Sediment Saturation。

Debug → Overview → Runtime Hydrology 新增 `Capacity Scale`，Visualization 增加 `Sediment Capacity` 和 `Sediment Saturation`。容量模式使用绿青色强调高承载区域；饱和度模式以蓝色表示欠饱和、浅色表示接近平衡、红色表示过饱和。点击 `Validate / Readback` 可查看 Capacity/Saturation Min/Max，`Run GPU Contract` 同时检查其有限性和帧划分确定性。

验证结果：

- 109 项无窗口断言全部 PASS，包括 CPU 派生场有限/有界、零 Capacity Scale 和两种帧划分一致性；
- VS2026 `Debug | x64` 编辑器增量构建成功；
- GTX 1050 / OpenGL 4.6 上四个 Compute Shader 和 Terrain Shader 编译成功；
- GPU Contract PASS：水量相对误差 `7.38228e-7`、泥沙质量误差 `0`、`capacityMax=0.0411786`、`saturationMax=1000`，Water、Sediment、Capacity、Saturation 的帧划分最大差值均为 `0`。

P13B 至此完成。下一阶段 P13C 会把 Capacity 与 Sediment 的差异作为侵蚀/沉积源项输入，并先在 CPU 参考模型定义 Height 与 Sediment 的质量交换、单步限幅和 Reset 契约。

## CPU 运行时侵蚀与沉积质量契约

P13C 第一阶段只扩展 CPU `TerrainHydrologyRuntime`，先验证地形与悬浮泥沙之间的质量交换，不立即修改 GPU Height Texture。侵蚀/沉积源项位于水与泥沙输运完成之后：

```text
Water / Sediment Transport 完成
  → 计算 Capacity 与 Saturation
  → 欠饱和：Height → Sediment
  → 过饱和：Sediment → Height
  → 重新计算 Capacity 与 Saturation
```

Height 本身是长度，Sediment 是单位地表面积上的悬浮质量，两者不能直接相加。因此新增 `TerrainDensity`，使用以下等效预算：

```text
TerrainMassPerArea = Height × TerrainDensity
CombinedMass = Σ((Height × TerrainDensity + Sediment) × CellArea)
```

欠饱和时的候选侵蚀量由 `(Capacity - Sediment) × ErosionRate × dt` 决定；过饱和时的候选沉积量由 `(Sediment - Capacity) × DepositionRate × dt` 决定。实际交换还会经过以下限制：

- 每个固定步的 Height 绝对变化不超过 `MaximumHeightChangePerStep`；
- 侵蚀后的 Height 不低于“初始 Height - MaximumErosionDepth”；
- 沉积量不超过当前格已有 Sediment；
- 所有参数、Height 和 Sediment 必须保持有限，Sediment 不得为负。

`ErosionRate` 和 `DepositionRate` 默认均为 0，意味着新增能力是显式启用的，不会改变旧场景和 P13B 的无源输运结果。Reset 会恢复初始化时的 Height、Water、Sediment，并清空累计 Eroded/Deposited Mass。

统计新增 Initial/Current Terrain Mass、Cumulative Eroded/Deposited Mass、Terrain+Sediment Mass Error、Height Min/Max 和当前固定步最大 Height 变化。原有 `SedimentMassError` 仍只描述悬浮泥沙相对初态的变化；启用侵蚀后它可以非零，应使用组合误差判断局部质量交换是否守恒。

验证结果：

- 欠饱和流动会降低 Height 并增加 Sediment；
- 过饱和静水会减少 Sediment 并抬高 Height；
- 高侵蚀率仍受单步变化和可侵蚀层下界约束；
- Reset 完整恢复侵蚀前状态；
- 两种帧划分执行相同固定步数后 Height/Sediment 一致；
- 新增 5 项后共 114 项无窗口断言全部 PASS，VS2026 `Debug | x64` 编辑器已重新链接成功，构建产物保留。

CPU 契约现已迁移到 GPU；下节说明编辑器中的运行时 Height 链路和验证方式。

## GPU 运行时侵蚀与沉积

P13C 的 GPU 阶段把地形 Height 从只读生成结果改为 `TerrainHydrologyGPU` 独占的运行时 Ping-Pong。生成器 Height 仍是不可变初始快照和侵蚀下界；模拟不会把结果写回生成器，也不会污染有限次 Authoring Erosion。

固定步执行顺序为：

```text
Flux → Water/Velocity + Sediment Transport
     → Capacity/Saturation
     → ErosionDeposition（同时写 Next Height 与 Next Sediment）
     → Barrier → 统一交换 Height/Sediment
     → 重算 Capacity/Saturation
```

`ErosionDeposition.comp` 不在同一纹理上原地读写。它读取 Initial Height、Current Height、Sediment 和 Capacity，同时输出 Next Height 与 Next Sediment；全局 Barrier 完成后才交换两组状态，因此 Color/Shadow 绕过不了同一份已完成的 Runtime Height，也不会观察到只更新一半的质量交换。

Debug 面板新增以下纯运行时参数：

- `Erosion Rate`、`Deposition Rate`：控制容量缺口或超额在每秒转换的比例，默认均为 0；
- `Terrain Density`：把 Height 长度换算为单位面积等效质量；
- `Max Erosion Depth`：相对初始化 Height 的最大可侵蚀层；
- `Max Height Step`：每个固定步允许的最大 Height 绝对变化。

启用水文后提高侵蚀/沉积速率，并使用 `Single Step` 或 `Play` 推进即可观察地形高度变化；`Readback` 可检查 Height Min/Max、Net Eroded/Deposited Mass 和 Terrain+Sediment Combined Error。这里的 Eroded/Deposited 是相对初始 Height 的净变化，不是跨帧累计吞吐量。`Reset` 会用初始化时缓存的数据恢复两张 Runtime Height，并清空 Water、Flux、Velocity、Sediment 与诊断场。

当前持久化边界保持保守：Runtime Height 不进入 Scene YAML，复制 Terrain 不携带模拟纹理，也没有自动 Bake。生成器版本变化会重建整个水文状态。初始化时为建立可恢复快照会同步读回一次生成器 Height，普通帧和 Reset 不再读回；只有显式 `Readback` 才同步获取 GPU 统计。

验证结果：

- GTX 1050 / OpenGL 4.6 上五个水文 Compute Shader 与 Terrain Shader 编译成功；
- 受控 GPU Contract 执行 100 个固定步：组合质量误差 `2.58287e-7`、侵蚀高度 `0.02`、沉积高度 `0.1`；
- `0.04 × 25` 与 `0.01 × 100` 两种帧划分的 Height/Sediment 最大差值均为 `0`，Reset 恢复检查通过；
- `Verify-Windows.ps1 -SkipGenerate` 增量构建成功，114 项无窗口断言全部 PASS，构建产物保留。

运行时派生图刷新现已完成。`TerrainGenerator::DeriveMapsFromHeight` 复用现有 `DeriveTerrainMaps.comp`，但允许输入 `TerrainHydrologyGPU` 的最终 Runtime Height。`TerrainRenderer` 会先完成本帧所有固定水文子步，再最多派生一次 Normal/Slope、Analysis 和 MaterialWeight；因此一个渲染帧即使追赶 4 个模拟步，也不会重复执行 4 次相同派生。Reset 会刷新一次以恢复初始表面；除此以外，未执行固定步或侵蚀/沉积源项关闭时没有额外派生开销。

这意味着侵蚀后的几何轮廓、阴影法线、坡度、曲率/流势和 Grass/Soil/Rock/Snow 分层现在读取同一版本的 Height。观察时可把 TerrainMaterial Sampling 保持为 `Full 4 Layers`，提高侵蚀率后从低角度查看沟槽：除轮廓下降外，陡坡岩石权重和低坡土壤/草地边界也应随新坡度移动；Reset 后两者一起恢复。受控 `GLIMMER_TERRAIN_VALIDATE=1` 验证会实际调用 Runtime Height 派生入口，并要求其有限性、权重归一化和输出哈希与相同 Height 的生成路径一致。

P13C 至此收口。运行时模拟仍是临时状态：不写入 Scene YAML，也没有隐式 Bake；若后续需要保存侵蚀结果，应单独设计显式 Terrain Asset Bake、失败回滚和 Undo/Redo，而不是改变当前 Reset/复制语义。

## CPU 简化气候与植被潜力基线

P14 第一阶段新增核心侧 `TerrainClimateRuntime`，先用纯 CPU 小网格固定气候场的单位、更新顺序和预算，再迁移 Compute Shader。该类位于 `Glimmer/Simulation`，不依赖编辑器、OpenGL 或具体植被模型，也不进入 Scene YAML。

场定义如下：

- `TerrainHeight`：米；
- `Temperature`：摄氏度；
- `AtmosphericMoisture`：每个二维空气柱包含的米水当量；
- `SurfaceWater` 与 `Rainfall`：米水深；
- `VegetationPotential`：`[0, 1]` 的环境适生度，不代表树木数量或实体实例。

使用统一水当量后，蒸发和降雨都是封闭系统内的显式转移：

```text
Temperature Relaxation
  → SurfaceWater --Evaporation--> AtmosphericMoisture
  → Conservative Upwind Moisture Advection
  → AtmosphericMoisture --Condensation/Orographic Rain--> SurfaceWater
  → Vegetation Potential Response
```

湿度输运按水平风向执行显式迎风通量，并按 CFL 比例限幅；封闭边界会保留原本越界的水汽，而不是悄悄丢失质量。饱和水汽随温度指数变化，超过饱和值的部分按凝结率降雨；同时使用 `dot(WindVelocity, TerrainGradient)` 估算迎风坡抬升，只在风沿坡向上时增加降雨。植被潜力由地表水适宜度和温度适宜度相乘，再按响应速率平滑靠近目标。

`Play/Pause/SingleStep/Reset`、固定时间步、最大追赶子步和 Dropped Time 与 P13 水文调度保持同类语义。统计同时给出大气水量、地表水量、累计蒸发/降雨、总水量误差以及各场范围；蒸发和降雨累计值用于解释内部通量，不能作为系统质量增减重复计入预算。

验证结果：

- 新增 9 项无窗口断言，覆盖暂停与单步、风向输运、封闭水量守恒、蒸发转移、迎风坡降雨、植被响应、Reset 和帧划分确定性；
- 重新运行 VS2026 Premake 生成，新增源文件已进入工程；
- `Verify-Windows.ps1 -SkipGenerate` 完成 `Debug | x64` 全解决方案增量构建，123 项无窗口断言全部 PASS；
- 构建产物和中间文件保留，没有删除 `bin`。

这些场现已迁移到 GPU；下一节说明运行方式与诊断入口。

## GPU 气候场与地形诊断

`TerrainClimateGPU` 由每个 `TerrainRuntime` 独立拥有，不进入 TerrainComponent 和 Scene YAML。它使用以下资源：

- Temperature：`R32F` Ping-Pong；
- AtmosphericMoisture：`R32F` Ping-Pong；
- VegetationPotential：`R32F` Ping-Pong；
- Rainfall：单张 `R32F` 派生纹理；
- SurfaceWater：只读引用 P13 Hydrology Water；没有水文 Runtime 时读取气候内部零纹理。

固定步拆成三个全局 Compute Pass：

```text
ClimateSource
  Temperature Relaxation + Evaporation
  → Barrier → swap Temperature/Moisture

ClimateAdvection
  Conservative Upwind Moisture Transport
  → Barrier → swap Moisture

ClimateResponse
  Condensation + Orographic Rain + Vegetation Response
  → Barrier → swap Moisture/Vegetation
```

三个 Pass 都只读 Current、只写 Next。湿度输运会根据 `abs(WindVelocity) × dt / CellSize` 计算 CFL 比例，总比例超过 1 时统一缩放；边界没有下风格时保留本格水汽，因此不会因 Clamp 采样隐式丢失。Response 通过世界高度梯度和 `dot(Wind, Gradient)` 判断迎风坡，只让正向抬升增加降雨。

TerrainRenderer 使用 GenerationVersion 管理气候资源重建，并以 ClimateFrameSerial 保证 Shadow Pass 和九个 Chunk 的重复 Prepare 不会让模拟在同一帧推进多次。三个 Compute Shader 都参与事务式热重载。

在 Debug → Overview → Runtime Climate 中可以：

- Play、Single Step、Reset；
- 修改二维 Wind Velocity；
- 修改 Initial Moisture，修改后需 Reset 才会作为新初值应用；
- 选择 Temperature、Atmospheric Moisture、Rainfall 或 Vegetation Potential 诊断；
- 点击 Readback 查看温度、湿度、降雨和植被范围；
- 点击 Run GPU Contract 执行受控 `3×1` 数值验证。

Terrain Shader 使用 slot 28～31 读取四个气候场。若水文与气候诊断同时启用，水文诊断优先；诊断关闭时不改变正常 Terrain PBR 颜色。当前 Temperature 用蓝—绿—红表示，Moisture 用褐—青表示，Rainfall 用暗紫—亮青表示，VegetationPotential 用褐—绿表示。

验证结果：

- VS2026 `Debug | x64` 全解决方案增量构建成功，123 项无窗口断言全部 PASS；
- GTX 1050 / OpenGL 4.6 上 `ClimateSource`、`ClimateAdvection`、`ClimateResponse` 和修改后的 Terrain Shader 编译成功；
- `GLIMMER_CLIMATE_VALIDATE=1` 实际执行 GPU Contract 并 PASS：Downwind Moisture `1`，Rising/Flat Rain `0.2/0`，Frame Partition Delta `0`；
- 构建产物和中间文件保留，没有删除 `bin`。

气候与水文的双向守恒耦合已经落地，具体执行顺序、所有权和验证方式见下一节。

已导入的 Quaternius Ultimate Nature Pack 同时包含 OBJ、FBX 和 Blend。当前引擎可使用 OBJ/FBX；Blend 作为源文件保留。整包尚未自动写入 AssetRegistry，也没有被气候 Runtime 硬编码引用。植被实例化阶段应先从 CommonTree/Birch/Willow、Bush 和 Grass 中各选少量代表模型，再建立物种参数、LOD 和实例批次。

## GPU 气候—水文守恒耦合

本阶段没有让 Climate 和 Hydrology 同时修改 Water。`TerrainClimateGPU` 新增 Evaporation 与有符号 WaterSource 两张 `R32F` 输出，最后一个独立 Compute Pass 执行：

```text
WaterSource = Rainfall - Evaporation
```

Climate 只描述本步每个格点应增加或移除的水深。Hydrology 的 Flux、Water Update 和 Sediment Transport 读取同一 Source/Sink；负值最多移除当前可用水深，最终只有 HydrologyUpdate 写 Water。Update 同时把实际应用值累加到独立 WaterSourceBudget Ping-Pong，显式 Readback 因而能从 GPU 的真实限幅结果重建 Expected Water，而不是在 CPU 上假设请求量全部成功。

`TerrainEnvironmentGPU` 负责统一调度。原 Hydrology 和 Climate 的 Play/Single Step 按钮仍保留，但都会进入同一个固定步累加器：

```text
ClimateSource → ClimateAdvection → ClimateResponse
  → ClimateWaterSource → Barrier
  → HydrologyFlux → HydrologyUpdate → Sediment/Erosion
```

这保证当前水面先参与蒸发和降雨计算，完整 WaterSource 对后续水流可见，并避免两个 Runtime 使用不同 Accumulator 时出现重复推进或少推进。任一 Reset 会共同恢复气候、水文和预算；Runtime 仍不序列化，也不会污染 TerrainComponent 或 Scene YAML。

Temperature 诊断的高度梯度由 `Temperature Lapse` 控制，单位是摄氏度/世界单位，温差近似为 `Lapse × HeightScale`。默认 `0.0065` 接近常见大气递减率，但当 Terrain 的 HeightScale 只有几十时色差会很弱；可在 Debug → Runtime Climate 中先设为 `0.05～0.10`。该值实时影响后续温度松弛，不改变地形几何；若要立即观察完整梯度，可修改后执行 Reset 再 Play 若干步。

在 Debug → Overview 中点击 Hydrology 的 `Validate / Readback` 或 Climate 的 `Readback`，会同步更新两侧统计。Climate 区域额外显示 Atmospheric + Surface 的 Total Water、Expected Total 和 Coupled Water Error。Hydrology 的标量 Rainfall 仍作为明确的外部水源保留并计入 Expected Total；若只观察封闭自然水循环，可把它设为 `0`。

验证结果：

- VS2026 `Debug | x64` 全解决方案增量构建成功，114 项无窗口回归全部 PASS；
- GTX 1050 / OpenGL 4.6 上新增 `ClimateWaterSource` 与修改后的三个水文 Shader 全部编译成功；
- 原 GPU 水文 Contract 保持 PASS，相对水量误差 `9.83321e-7`；
- 新增空间 Source/Sink Contract 先施加 `+0.10`、再施加 `-0.04`，最终水深 `0.06`，预算误差 `0`；
- GPU 气候 Contract 保持 PASS：Downwind Moisture `1`、Rising/Flat Rain `0.2/0`、Frame Partition Delta `0`；
- 构建产物和中间文件保留，没有删除 `bin`。

下一阶段不直接创建植被实体，而是先把 Humidity、Temperature 与 VegetationPotential 接入 Terrain Material Weight，定义动态生态权重与既有 Height/Slope/Curvature 权重的组合和归一化规则。

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

### **为什么我们要在引擎初始化阶段编译 Shader，而不是在每一帧渲染时编译？**

- **标准答案**：

  **开销极大**：Shader 编译涉及字符串解析、驱动程序的底层代码优化（JIT），这在 CPU 上非常耗时。

  **管线停顿 (Pipeline Stall)**：如果在 Run 循环里编译，每一帧都会产生巨大的卡顿，帧率会掉到个位数。

  **状态对象**：在 OpenGL 中，编译后的程序是一个数字 ID（Handle），它存在显存中。我们只需要在渲染前通过 glUseProgram(ID) 切换状态，这个动作几乎是瞬时完成的。

### **为什么在类里要把程序句柄起名叫 m_RendererID 而不是 m_ShaderID？**

- **标准答案**：
  这是一种**架构习惯**。在 OpenGL 中，最终起作用的是 **Program（程序对象）**，它是由 Vertex Shader 和 Fragment Shader 链接而成的。对于渲染器来说，它只需要知道这个“渲染程序的 ID”。未来我们在封装 Texture、Buffer 时，也会用 m_RendererID 来代表 GPU 端的资源句柄，保持命名的一致性。

### **glDetachShader 的作用是什么？不写会怎么样？**

- **标准答案**：
  glDetachShader 是将着色器对象从程序对象中“解绑”。
  **原因**：一旦 glLinkProgram 成功，程序对象就已经包含了所需的二进制指令。此时如果不 Detach 就直接 glDeleteShader，着色器对象并不会被真正删除，而是被标记为“待删除”，直到程序对象被销毁。Detach 之后再 Delete，可以更早地释放显存空间，是良好的资源管理习惯。

### **glGetUniformLocation 这个函数有什么性能问题吗？你会如何优化它？**

- **标准答案**：

  **性能损耗**：glGetUniformLocation 是一个相对“昂贵”的操作，因为它涉及到字符串匹配。如果在每一帧中对大量的 Uniform 调用这个函数，会显著降低 CPU 性能。

  **优化方案（Uniform 缓存）**：在 Shader 类中建立一个 **std::unordered_map<std::string, int>**。当第一次上传某个 Uniform 时，查询 Location 并存入 Map。下次上传时，直接从内存里的 Map 读取，避免调用 OpenGL 底层查询指令。

  **高级方案**：在现代 OpenGL（4.3+）中，可以使用 layout(location = x) 直接在 Shader 里给 Uniform 指定位置，彻底省去查询过程。

### **为什么要把 VertexBuffer::Create 定义为静态工厂方法，而不是直接 new OpenGLVertexBuffer？**

- **标准答案**：
  这是**依赖倒置原则（DIP）**的体现。Application 属于高级逻辑层，它应该依赖于抽象接口 VertexBuffer，而不是具体的 OpenGL 实现。通过这种方式，我们可以实现“编译时隔离”：如果你正在开发手机端的 Vulkan 版本，只需让 Create 返回 VulkanVertexBuffer 即可，**业务层的代码一行都不用改**。

### **为什么我们要把 View 和 Projection 矩阵乘在一起传给 Shader，而不是分开传？**

- **标准答案**：

  **减少计算量**：对于一个模型的所有顶点（可能有几万个），它们使用的 `VV` 和 `PP` 矩阵都是一样的。在 CPU 算好乘积 `VPVP` 只需一次 4x4 矩阵乘法；如果传给 Shader，GPU 就要对每个顶点都算一遍乘法。这极大地节省了 GPU 的计算资源。

  **管线优化**：这是标准做法。`P×V×MP×V×M` 构成了物体的最终屏幕位置。将 `PVPV` 视为“场景状态”，`MM` 视为“物体状态”，符合逻辑上的分层。

### **正交投影 (Orthographic) 和 透视投影 (Perspective) 的区别？**

- **标准答案**：

  **正交投影**：物体无论远近，大小看起来都一样。适合 2D 游戏、UI、CAD 软件。

  **透视投影**：近大远小。适合 3D 游戏，因为它模拟了人眼的成像规律。

### **在顶点着色器里，为什么矩阵相乘的顺序是** $P×V×M×pos$

- **标准答案**：

  **数学约定**：OpenGL 和 GLM 默认使用**列优先 (Column-major)** 存储，数学计算上遵循从右向左的变换顺序。

  **物理含义**：

  - `MM` (Model)：将顶点从**局部空间**转到**世界空间**（决定物体在哪）。
  - `VV` (View)：将顶点从**世界空间**转到**观察空间**（决定相机在哪看）。
  - `PP` (Projection)：将顶点从**观察空间**转到**裁剪空间**（决定哪些东西在屏幕内）。
  - **结论**：顶点必须先被物体变换，再被相机变换，最后被投影变换。顺序反了，渲染结果就会彻底错误。

### **你把 Shader 改成了虚基类，每一帧调用 Bind() 都会经过虚函数表（V-Table），这会产生严重的性能损耗吗？**

- **标准答案**：
  “虚函数确实存在一次间接寻址的开销，但在 **Shader 绑定**这种级别的操作中，这种损耗是**微不足道**的。

  **调用频率**：通常我们每一帧只绑定几次或几十次 Shader（取决于材质数量）。相比于 GPU 每秒处理的数百万个顶点，CPU 端这几十次虚函数调用完全不是瓶颈。

  **架构收益**：这种设计换取了极强的**跨平台能力**。在没有引入这个重构前，我们的 Application 被迫了解 OpenGL 的细节。现在，整个 Renderer 子系统完全由接口驱动，我们可以无缝接入 Vulkan 或 Metal，这种架构的健壮性远比节省那几纳秒的性能更重要。”

### **在渲染器抽象中，你如何处理特定 API（如 OpenGL）才有的 Uniform 上传功能？**

“起初我尝试在 Renderer 中使用 dynamic_pointer_cast 将通用的 Shader 指针转为 OpenGLShader。但我意识到这会导致 **‘编译时依赖耦合’**，使得通用的渲染层感知到了具体的图形后端，违背了开闭原则（OCP）。

因此，我采用了 **接口多态化** 的方案。我将常用的 Uniform 上传操作抽象到了 Shader 基类接口中。
对于 OpenGL 后端，它会实现这些虚函数并调用 glUniform。
对于未来可能的其他后端（如 Vulkan），它可以通过推送常量（Push Constants）或描述符集（Descriptor Sets）来实现这些接口。
这样 Renderer 类就实现了完全的 **后端无关性（Backend-Agnostic）**，提升了引擎的可扩展性。”

### **纹理 Slot (或者叫 Texture Unit) 是干什么的？**

- **你的回答**：
  “它是 GPU 上的‘插槽’。现代显卡通常有 16 到 32 个插槽。通过这个机制，我们可以在单次绘制（Draw Call）中同时使用多张贴图（比如：一张反射贴图，一张法线贴图）。我们在 C++ 中通过 glActiveTexture 选择插槽，并在 Shader 中通过 Uniform 变量告诉采样器它该读取哪个插槽。”

### **为什么在引擎里要提供 ShaderLibrary 这种管理器？**

**你的回答：**
“这主要涉及 **资源生命周期管理 (Resource Lifecycle Management)** 和 **降低运行时开销** 两个方面。
第一，**避免重复加载**。通过 unordered_map 的映射机制，我们可以确保同一个 Shader 文件在整个应用程序生命周期内只被编译和链接一次，节省了宝贵的初始化时间和显存。
第二，**解耦逻辑与资源引用**。在复杂的场景中，不同的图层（Layer）可能需要共享同一个 Shader。通过库，我们不再需要在图层之间互相传递脆弱的原始指针，而是通过统一的‘键值（Key）’来获取资源，这极大地增强了代码的模块化和健壮性。
第三，**集中式优化**。有了 Library 这一层，未来我们可以轻松实现‘热重载（Hot Reloading）’。即当开发者在外部修改了 .glsl 文件后，Library 可以自动重新编译对应 Shader，而无需重启游戏，从而提升开发效率。”

### **我看你在纹理上传时使用了 glTextureSubImage2D，为什么不使用传统的 glTexImage2D？**

**你的回答：**
“我选择了 **DSA (Direct State Access)** 模式。
传统的 OpenGL API 强依赖于‘绑定-编辑（Bind-to-Edit）’模型，这在大型引擎开发中会导致两个严重问题：

1. **状态污染**：频繁的 Bind/Unbind 容易导致不可预见的渲染错误。
2. **性能开销**：为了确保操作正确，开发者往往需要不断查询或重置全局绑定状态，增加了驱动程序的开销。

通过使用以 glTexture... 开头的 DSA 函数，我可以绕过上下文绑定点，直接通过 **Object Handle（资源句柄）** 操作 GPU 资源。这不仅使代码更加**线程安全**且逻辑清晰，还减少了驱动层的状态验证次数。这在我的 Glimmer 引擎中是迈向高性能、现代化渲染管线的重要一步。”

### **为什么我们要通过 Framebuffer 进行间接渲染，而不是直接画在窗口上？**

**你的回答：**
“这是为了实现 **‘渲染管线的虚拟化’**。
首先，它解决了**编辑器集成**的问题。通过将渲染结果输出为纹理，我们可以利用 ImGui 等 UI 库在同一个 OS 窗口内组织多个视口（Viewport），实现类似 Unity 的工作流。
其次，它为 **渲染后期（Post-Processing）** 提供了底座。一旦画面存在于纹理中，我们就可以对这块显存执行模糊、调色、抗锯齿（FXAA/MSAA）等计算，而不会影响原始的几何体渲染。
最后，它允许我们实现 **‘分辨率独立渲染’**。游戏逻辑可以运行在 4K 画布上，但最终通过缩放显示在 1080p 的窗口中，这种灵活性是现代高性能引擎的基石。”
