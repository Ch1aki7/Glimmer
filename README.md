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

### 为什么 EnTT ID 不够用

`entt::entity` 很适合做 Registry 内部索引，但它只在当前 Scene 实例里有意义。删除实体、重新加载场景或复制到 Runtime Scene 后，同一个数值可能已经指向别的对象。编辑器命令、场景保存和跨 Scene 查找需要一份更稳定的身份，于是实体多了一层 64 位 UUID。

`UUID` 默认用 `mt19937_64` 生成非零值，随机引擎由 Mutex 保护。这里追求的是项目内稳定引用，并没有把它包装成分布式 ID 服务。`0` 被保留为无效值，`CreateEntityWithUUID()` 也会拒绝零值和当前 Scene 内的重复 UUID。

### Scene 怎样维护身份

每个通过 `Scene` 创建的实体都会获得 `IDComponent`。Scene 同时维护 `UUID -> entt::entity` 索引，因此编辑器命令可以记住 UUID，在 Undo 或 Redo 时重新找到目标，而不用长期保存一个可能失效的 EnTT Handle。

```text
CreateEntity()
    -> 生成 UUID
    -> 添加 IDComponent
    -> 写入 Scene UUID 索引

DestroyEntity()
    -> 先移除 UUID 索引
    -> 再销毁 Registry 实体
```

`FindEntityByUUID()` 还会检查 Registry 中的 Handle 是否仍然有效，发现陈旧记录时顺手清掉。这个防线不复杂，但能让查找失败安静地返回空实体，而不是把旧 ID 当成新实体继续使用。

### 复制与序列化的区别

这部分最初很容易混淆。复制单个实体时，目标是新对象，所以 `DuplicateEntity()` 会生成新的 UUID；`Scene::Copy()` 用于 Edit/Play 隔离，两个 Scene 中的逻辑实体需要对应起来，因此保留原 UUID。组件值会复制过去，脚本实例和 Terrain Runtime 等运行期对象则重新建立。

当前 Scene YAML 是 Version 6，每个实体都保存 UUID。加载时通过 `CreateEntityWithUUID()` 恢复索引；没有 Version 的旧场景按 Version 1 读取，因为文件里没有稳定身份，加载器会为实体生成新 UUID。无窗口回归测试使用固定 UUID 做保存、加载和 `FindEntityByUUID()` 往返，避免只检查实体数量这种过于宽松的结果。

UUID 解决的是实体身份，资源身份交给下一章的 `AssetHandle`。把两者分开后，Scene 可以稳定找到某个实体，组件也能稳定引用某份项目资源。

## AssetHandle 与基础资产管理系统

### 组件只记住自己引用了什么

早期 `SpriteRendererComponent` 直接保存 `Ref<Texture2D>`。这样画图很方便，保存场景时却立刻遇到麻烦：智能指针不能写进 YAML，同一路径也可能被加载多次，组件还被迫参与 GPU 资源生命周期。`AssetHandle` 就是在这个阶段加进来的，它是资源的 64 位稳定身份，底层复用 `UUID`。

Scene 组件现在只保存 Handle。路径、资源类型和运行期对象由 `AssetManager` 负责：

```text
Scene / Material 中的 AssetHandle
    -> AssetRegistry.yaml 查询 Metadata
    -> 组合项目 assets 根目录与相对路径
    -> 按类型加载资源
    -> 放入对应运行期缓存
```

这个分层后来覆盖到 Sprite、Model、Shader、Material、TerrainMaterial 和 Cubemap。场景文件不再夹带绝对路径，Renderer 也无需从组件里接管资源所有权。

### 导入与注册表

`ImportAsset()` 只接受项目 `assets` 目录内的现有文件。路径会转为规范化的项目相对路径，Windows 下的查找键还会统一大小写。同一路径再次导入时复用已有 Handle；新资源按扩展名判断类型，随后写入 `AssetRegistry.yaml`。

```yaml
AssetRegistry:
  - Handle: 9195328290163695800
    Type: Texture2D
    FilePath: textures/balatro.png
    ColorSpace: SRGB
    Semantic: Color
```

注册表按 Handle 排序输出，重复启动不会因为 `unordered_map` 的遍历顺序制造一整页 Diff。Texture Metadata 还记录 `SRGB/Linear` 与 `Color/Normal/Data/Height` 语义。首次导入会按文件名做推断，材质纹理拖放则写入明确语义；元数据变化时 Texture Cache 会失效，下次解析才按新颜色空间重新加载。

### 延迟加载和失败路径

`AssetManager` 为每种已支持资源维护独立缓存。`GetTexture2D()`、`GetModel()` 或 `GetMaterial()` 先检查 Handle 与类型，再复用缓存；未命中才访问磁盘。无效 Handle、类型不匹配或文件丢失都会返回空引用，调用方决定使用纯色、跳过 Draw，或在 Inspector 显示加载失败。

我更愿意让缺失资源在一个明确入口失败，而不是让各个 Renderer 猜路径。代价也很清楚：当前加载是同步的，首次命中仍可能卡住主线程；注册表没有文件移动监视、依赖图和自动修复。资源卸载与后台 GPU 上传队列也还没建立。现在这套系统适合中小型编辑器项目，还不是流式资产管线。

### 数据各自落在哪里

| 位置 | 保存内容 |
| --- | --- |
| Scene 或 `.glmat` | 被引用资源的 `AssetHandle` |
| `AssetRegistry.yaml` | Handle、类型、相对路径与 Texture Metadata |
| `AssetManager` 缓存 | 当前进程已经加载的资源对象 |

这个边界给后续材质系统省了很多事。材质可以引用 Shader 与纹理，实体再引用材质，整个链路只有 Handle 进入持久化文件。

## Material 资产与实体材质组件

### 共享参数和实体差异分开保存

材质最初只是散落在绘制代码旁边的一组颜色和数值。一旦两个实体需要共享外观，继续复制参数就会出现很现实的问题：改了一个，另外几个要不要跟着改？当前结构把答案写进数据模型里。

`.glmat` 是共享 Material Asset，`MaterialComponent` 只保存它的 Handle 和该实体的 `MaterialOverrides`。多个实体引用同一材质时，编辑共享资产会一起变化；只想改其中一个，就在 Override Mask 中启用对应字段。

```text
MaterialHandle
    -> AssetManager::GetMaterial()
    -> 读取共享 .glmat
    -> MaterialInstance 合并已启用的 Overrides
    -> Renderer2D / Renderer3D / Shadow Pass 使用最终属性
```

Override 会保存 Mask 和 Values。关闭某个字段只让它停止参与合并，原来的编辑值仍留着，重新打开时不用从头输入。Scene 序列化也只保存组件引用与局部覆盖，共享参数继续留在 `.glmat`。

### `.glmat` 里有什么

当前 Material 保存 Shader Handle、Base Color 与纹理，还包括 Metallic、Roughness、Normal、AO、Emissive 参数，以及 `Opaque / Mask / Blend` 和 Alpha Cutoff。纹理槽保存的仍是 AssetHandle，颜色空间与语义由 Asset Metadata 约束。

加载时会收紧容易出错的范围，例如 Roughness 最低为 `0.04`，Normal Scale 限制在 `[0, 2]`，Alpha Cutoff 限制在 `[0, 1]`。解析失败的 Material 不进入缓存。Renderer2D 读取其中适合无光照批处理的颜色、纹理和平铺参数；Renderer3D 再消费 PBR、法线、AO、Emissive 与 AlphaMode。Metallic/Roughness 目前仍是标量，项目还没有单独贴图或 ORM 通道。

### 编辑时有两条路径

实体 Inspector 修改 `MaterialComponent::Overrides`。这些操作进入 Scene 的 CommandHistory，不会写共享 `.glmat`；Play 模式下改到的是 Runtime Scene 副本，停止后丢弃。

在 Content Browser 选中 `.glmat` 后，Asset Inspector 编辑的是共享 `MaterialState`。每次命令都会保存到磁盘，Undo/Redo 也会写回对应状态。Play 模式把共享 Material 设为只读，防止运行时调试意外改掉项目资产。

保存失败不能只弹一句日志。`Material::Save()` 先写 `.tmp`，已有文件会临时改名为 `.bak`，替换失败时尝试恢复原文件。Inspector 也会把内存状态退回操作前，并且失败的命令不会进入 Undo 栈。这里多写了几步文件操作，但至少不会出现界面已经更新、磁盘仍是旧值的假象。

### 运行时解析

Renderer 收到 Material Handle 与可选 Overrides 后构造最终属性。Material 和 Override 都提供 Version，Renderer3D 还会比较缓存中的 `MaterialState` 与 Overrides；共享材质或局部覆盖变化后，最终属性会重新合并。纹理仍由 AssetManager 延迟解析，Material 本身不保存 OpenGL Texture ID。

无效材质不会让场景崩溃。2D 路径可退回 Sprite 自带的 Color、TextureHandle 与 TilingFactor；3D 路径根据提交契约跳过无法形成有效渲染项的对象。这个回退让旧场景可以逐步迁移，也避免材质文件暂时损坏时把整个编辑器带走。

### 当前留下的缺口

材质的共享资产事务已经接入 Undo/Redo，Normal、AO 和 Emissive 也进入 3D 渲染。接下来仍需补齐 Metallic/Roughness Texture 或 ORM 约定，以及 Shader 参数布局反射。Terrain 使用独立 `.glterrainmat`，它有自己的四层数据与保存入口，不能和普通 `.glmat` 当成同一种资源解析。

## 统一光源组件与 Light UBO

### 把灯光从 EditorLayer 还给 Scene

早期测试灯光只是 `EditorLayer` 里的一个位置变量。它能照亮模型，却不能随场景保存，也不会自然地复制到 Runtime Scene。现在方向光、点光源和 SkyLight 都是 ECS 组件，位置与朝向继续使用实体的 `TransformComponent`。这样移动 Gizmo 时，改到的就是场景数据，不再有第二套编辑器灯光状态。

方向光取 Transform 的局部 `-Z` 轴作为照射方向，点光源位置来自 Translation。Scale 不参与强度或范围计算。`DirectionalLightComponent` 还保存 CSM 的分辨率、级联数量和 Bias 等参数；这些阴影设置交给 `ShadowRenderer`，不会挤进 Light UBO。

### 每帧怎样收集

Edit 与 Play 使用同一个 `Scene::UploadLightEnvironment()`：

```text
Scene 遍历 Enabled Light Components
    -> 取第一个 Directional Light
    -> 收集最多 16 个 Point Lights
    -> 取第一个有效 SkyLight
    -> Renderer::UploadLightEnvironment()
```

`LightEnvironment` 是跨图形 API 的 CPU 描述。方向光和点光源会被打包进 binding 1 的 Uniform Buffer，SkyLight Handle 与强度则交给 `EnvironmentLighting` 解析环境贴图。Scene 只负责收集组件，不创建 OpenGL Buffer。

当前选择规则很朴素：第一个启用的方向光生效，点光源按 Registry 遍历顺序截取前 16 个。这里还没有按距离筛选，也没有 Tiled 或 Clustered Lighting。灯多了以后，简单增加数组长度只会把问题推迟，并不会让光源选择更合理。

### std140 布局

GPU 数据使用 `vec4` 打包，避免 CPU 结构里的 `vec3` 与 GLSL std140 填充对不上：

```text
DirectionIntensity    xyz = direction, w = intensity
DirectionalColor     rgb = color
AmbientColorIntensity rgb = color, w = ambient intensity
LightCounts          x = point light count
PointLights[16]      position/range + color/intensity
```

`GPUPointLight` 固定为 32 字节，完整 `GPULightEnvironment` 是 576 字节，编译期 `static_assert` 会检查这两个尺寸。没有有效方向光时，结构体保留 `0.03` 的低环境光，旧场景不会直接黑成一片，但也不会凭空得到方向光。

### 谁消费这份光照

`PBRModel.glsl` 与 `Terrain.glsl` 都声明 binding 1 的相同布局。Renderer 每帧只上传一次，Shader 各自计算方向光和点光源贡献。CSM 阴影贴图、Diffuse Irradiance、Specular Prefilter 与 BRDF LUT 通过各自 Renderer 绑定，Light UBO 不承担纹理资源所有权。

当前 Scene YAML 是 Version 6，光源参数、Transform 和阴影设置都能保存并复制到 Play Scene。Inspector 中的连续灯光编辑也进入 CommandHistory。仍未支持 Spot Light；点光源上限和缺少空间剔除，是这套 UBO 接下来真正需要处理的地方。

## 3D Material Pass 与基础 PBR

### 从组件到 RenderItem

3D 模型最初由编辑器保存 Model 数组和全局 Shader 选择，场景里的实体并没有完整描述自己怎样被画出来。现在这条链路从 ECS 开始：`ModelRendererComponent` 保存 Model Handle，`MaterialComponent` 保存 Material Handle 与局部 Overrides，`Renderer3D` 负责把它们解析成 RenderItem。

```text
Scene Entity + Transform
    -> Model / Material AssetHandle
    -> AssetManager 解析资源
    -> MaterialInstance 合并 Overrides
    -> Renderer3D RenderQueue
    -> PBR Shader + Light / Shadow / IBL
```

Model 只持有导入后的 Mesh 与局部材质纹理，不再自己上传 Uniform 或发 Draw。缺少 Model、Material 或 Shader 时，提交会被计入 `SkippedModels` 并跳过。这个失败点放在 Renderer3D 入口，资源类就不必偷偷决定渲染策略。

### 队列取代逐对象立即绘制

提交阶段先解析材质和纹理，再按 AlphaMode 分流。Opaque 与 Mask RenderItem 根据 Shader、Material、纹理和 Mesh 状态排序；状态相同且 Shader 支持 Instancing 时，Renderer 会把 Transform 与 EntityID 写入实例 Buffer，每次最多提交 1024 个实例。单个对象或不支持 Instancing 的 Shader 仍走普通 Draw。

Blend 项留到 `EndScene()`，按相机距离从远到近稳定排序，开启 SrcAlpha 混合并关闭深度写入。透明对象目前逐项绘制，不参加 Opaque Instancing。Mask 仍在不透明队列中写深度，片元 Alpha 低于 Cutoff 时由 Shader discard。

这套队列让我第一次能把场景提交数量和 GPU 绘制次数分开看清。DebugPanel 展示 Shader Bind、Texture Bind、Draw Call、Instance Count 与跳过数量，用来检查排序和批处理有没有真的省下状态切换。

### 当前 PBR 数据

`PBRModel.glsl` 使用 GGX、Schlick-GGX 与 Fresnel-Schlick 组成 Cook-Torrance 直接光照。Base Color 和 Emissive 按 sRGB 纹理加载，Normal、AO 及 Metallic/Roughness 数据按线性空间使用。普通 `.glmat` 提供 Metallic/Roughness 标量与 BaseColor、Normal、AO、Emissive 纹理；FBX 等导入模型还可以从 Mesh 材质取得独立 Metallic/Roughness 贴图作为补充。

直接光来自 Light UBO。方向光可采样 1 到 4 级 CSM，环境部分使用 Diffuse Irradiance、Specular Prefilter 和共享 BRDF LUT。材质若没有有效纹理，Renderer 会绑定白色回退纹理，再由 `u_Has*Texture` 告诉 Shader 对应纹理槽是否真的有资源。

Shader 同时向 `RGBA16F` 场景颜色附件和整数 EntityID 附件输出，模型因此能参与鼠标拾取。HDR 值在这里保持线性，材质 Shader 不做 Tone Mapping 或 Gamma 编码。

### 还没收口的部分

普通 Material Asset 还没有 Metallic/Roughness Texture 字段或统一 ORM 槽，当前相关贴图主要来自模型导入数据。局部反射探针、蒙皮动画和更复杂的透明排序也未进入这条路径。自定义 3D Shader 若要参与 AlphaMode 或 Instancing，必须满足对应 Uniform 与顶点输入契约，单纯能链接成功还不够。

## 线性 HDR 场景缓冲与独立 Tone Mapping

### 先保住高光，再决定怎样显示

当场景颜色还写在 `RGBA8` 时，PBR 高光和强点光源一旦超过 `1.0` 就会被截断。后面再调曝光，只是在放大一张已经丢失细节的图片。现在场景主颜色附件使用 `RGBA16F`，所有场景 Shader 输出线性 HDR；显示转换集中到帧末尾完成。

Scene Framebuffer 当前包含三份数据：

| 附件 | 格式 | 用途 |
| --- | --- | --- |
| Color 0 | `RGBA16F` | 线性场景颜色 |
| Color 1 | `RED_INTEGER` | EntityID 拾取 |
| Depth | `Depth24Stencil8` | 深度测试与世界位置重建 |

EntityID 不经过后处理，鼠标仍直接读取 Scene Framebuffer 的整数附件。Viewport 显示的是 `PostProcessRenderer` 生成的 RGBA8 纹理。

### PostProcessRenderer 的执行顺序

后处理资源已经从 `EditorLayer` 收进引擎侧 `PostProcessRenderer`。它持有一个全分辨率 RGBA8 Display Framebuffer、两张半分辨率 RGBA16F Bloom Ping-Pong，以及 Tone Mapping、Bloom Extract 和 Blur Shader。

```text
RGBA16F Scene Color
    -> 可选 Bloom 亮部提取与半分辨率双向模糊
    -> 合并 Bloom
    -> 用 Scene Depth 重建世界位置并计算距离/高度雾
    -> Exposure EV
    -> ACES Filmic + White Point
    -> 可选 Grayscale
    -> Gamma 2.2 编码
    -> RGBA8 Display Texture
```

Bloom 阈值会考虑当前 Exposure EV，提取出的颜色仍保持线性；模糊结果在 Tone Mapping 前加回 Scene Color。雾也在线性颜色上混合，可以使用手动颜色、方向光颜色，或从 SkyLight 的较粗 Mip 取环境色。没有相机时，依赖深度重建的雾会自动关闭。

### Sampler 和尺寸管理

Display Framebuffer 跟随 Viewport 尺寸，Bloom 纹理保持一半宽高，最小为 `1×1`。`PostProcessRenderer::Resize()` 只在尺寸变化时重建附件。

Tone Mapping Shader 同时声明 `sampler2D` 和 `samplerCube`。OpenGL 会检查整个 Program 的 Sampler 类型，即便当前雾分支没有执行，所以 Cube Sampler 固定使用纹理单元 2，Bloom 使用单元 3，不能依赖默认值都落在 0。这个问题在部分驱动上会直接让 Draw 失败，属于看起来像 Shader 逻辑、实际是绑定契约的典型坑。

后处理无法完全绕过。即使 Bloom、Fog 和 Grayscale 都关闭，场景仍会经过 Exposure、ACES 与 Gamma，保证 Viewport 始终收到显示空间颜色。Settings 目前是编辑器会话内的运行时状态，没有写进 Scene YAML；TAA 也尚未实现，当前没有 Jitter、Velocity 或 HDR History。

## 天空盒与 SkyLight 资产化

这一章把天空从编辑器里的测试背景整理成场景资产，并继续补齐 PBR 所需的环境光。现在同一份 SkyLight 会同时参与可见背景、模型反射、地形环境光和后处理雾色，场景中只保存资源句柄与实体参数。

### 资源入口与场景数据

Cubemap 有两种入口：六面 LDR 图片使用 `.glsky` 描述，Radiance `.hdr` 可以直接导入，也可以由 `.glsky` 的 `Source` 字段引用。相对路径始终以描述文件所在目录为基准，资源注册表统一把它们登记为 `AssetType::Cubemap`。

```yaml
Cubemap:
  Source: ../textures/studio.hdr
  Resolution: 512
```

六面格式仍支持 `Right / Left / Top / Bottom / Front / Back`、`ColorSpace` 和 `MissingFaceColor`。这种格式适合已经切好的 JPG/PNG；等距柱状 HDR 则会转换为线性 `RGBA16F TextureCube`，并生成直到 `1×1` 的普通 Mip Chain。

场景侧保持很轻：

```cpp
struct SkyLightComponent
{
    AssetHandle CubemapHandle{ 0 };
    float Intensity = 1.0f;
    bool Enabled = true;
};
```

`Scene` 取第一个启用且句柄有效的 SkyLight。`.glimmer` 保存 Handle、Intensity 和 Enabled；进入 Play 后组件随场景副本复制，Cubemap Runtime 仍由 `AssetManager` 缓存。Inspector 可以替换或重新加载环境资源，成功 Reload 会递增 Runtime Version。

### 从背景图到 IBL

可见背景由 `SkyboxRenderer` 绘制。View 矩阵会去掉平移，顶点深度落在远平面；绘制期间深度函数临时切到 `LessEqual`，结束后恢复 `Less`。结果先写入 HDR Scene Framebuffer，再与场景一起完成 Bloom、雾和 Tone Mapping。

PBR 使用的是同一环境源派生出的三张资源：

```text
Cubemap Source
  -> Diffuse Irradiance      32×32 RGBA16F，64 samples
  -> Specular Prefilter      64×64 RGBA16F，64 samples，7 mips
  -> Split-Sum BRDF LUT      64×64 RG16F，128 samples
```

Diffuse Irradiance 负责低频漫反射；Specular Prefilter 用 GGX 重要性采样，把 Roughness 映射到不同 Mip；BRDF LUT 与环境内容无关，在 Renderer 初始化时生成并由进程共享。模型固定使用纹理槽 8/9/10，地形使用 20/21/22，避开材质、阴影和地形纹理已有的槽位。

### 开发时的取舍

环境卷积放在 `EnvironmentMapLoader` 与 `EnvironmentLighting`，没有塞进 OpenGL 纹理类。这样方向约定、采样算法和缓存规则留在 Renderer 核心层，后端对象只处理存储与传输。

派生缓存使用 `源 Handle + Cubemap Runtime Version + 类型 + 分辨率 + 样本数` 作为键。正常帧命中活动键时不会读回源纹理，也不会重复卷积；资源 Reload 后，旧版本对应的条目会失效。Mip 0 保留源环境，较粗层才逐步扩散，这一点能避免低粗糙度反射一开始就发糊。

目前缓存只存在于进程内存，没有磁盘派生文件和 LRU 预算。场景也还没有环境旋转、多 SkyLight 混合、局部 Reflection Probe 或动态场景反射；IBL 看到的是 SkyLight，不会反射场景实体。

### 验证记录

P10 验收时，GTX 1050 / OpenGL 4.6 日志确认 BRDF LUT 只生成一次，Diffuse 与 Specular 各生成一次；PBR Lab 6/6 通过，78 项无窗口回归全部通过，并覆盖 LUT 的有限值、范围与 Roughness/掠射角响应。

## 编辑器基础收口

组件逐渐增多后，早期那种由 `EditorLayer` 直接保存选择、面板顺手修改 Scene 的写法已经很难维护。这一轮收口的重点，是把选择、展示、命令和场景生命周期分开，让新增属性沿着同一条编辑路径接入。

### 面板与选择边界

`EditorLayer` 负责编辑场景、运行场景、相机、Framebuffer、后处理输入和各面板的生命周期编排；Hierarchy 负责实体列表与创建、复制、删除；Inspector 负责显示实体组件或资产内容。面板通过注入的 Scene、`SelectionContext` 和 `EditorCommandHistory` 工作，不拥有场景生命周期。

`SelectionContext` 只保留 Entity 或 Asset 中的一种选择。Hierarchy 选中实体时会清掉 AssetHandle，Content Browser 选中资产时会清掉 Entity。Inspector 因而只需按选择类型分流，也会在对象失效时停止访问旧组件。

### 一次操作对应一条命令

命令接口的 `Execute()` 与 `Undo()` 都返回成功状态。只有操作成功，`EditorCommandHistory` 才会在 Undo/Redo 双栈间移动命令；新命令成功执行后才清空 Redo。这项约束来自材质保存可能失败的实际情况：UI 已经改了内存值，并不代表磁盘操作也成功。

轻量且不会失败的操作使用 `LambdaEditorCommand`，需要保存前后完整状态的操作使用 `ValueEditorCommand<T>`。ImGui Slider 和 Gizmo 会连续产生值，`EditorValueTransaction<T>` 在控件激活时保存 Before，释放时提交 After，整次拖动只占一条历史记录。

当前命令范围包括：

- 实体创建、复制、删除，以及组件添加、移除和重置；
- Transform 与 Gizmo 连续编辑；
- Terrain、Camera、Directional/Point/Sky Light 的连续参数；
- HeightMap、SkyLight Cubemap、TerrainMaterial、MaterialHandle 等离散替换；
- 实体 Material Overrides 的开关、数值、纹理和 Reset；
- 共享 `.glmat` 的完整状态、保存与失败回滚。

快捷键为 `Ctrl+Z` 撤销，`Ctrl+Y` 或 `Ctrl+Shift+Z` 重做。

### 实体恢复为何使用 UUID

销毁实体后，旧 `entt::entity` 已经失效，也可能被 registry 复用。`EntitySnapshot` 因此保存稳定 UUID、Transform 和当前支持的可选组件，命令执行时再通过 `FindEntityByUUID()` 查找，必要时用原 UUID 重建实体。

Terrain 快照只复制 `TerrainSpecification`，GPU Runtime 交给 Renderer 延迟重建；Native Script 只复制工厂回调，不复制正在运行的脚本实例。这两个限制防止 Undo 后的新实体继续引用已销毁的运行时对象。

### Edit、Play 与调试场景

进入 Play 前会清空选择，并由 Editor Scene 复制出 Runtime Scene；Hierarchy 和 Inspector 随后绑定运行时副本，CommandHistory 暂时置空。Stop 会丢弃运行时副本并重新绑定编辑场景，所以播放期间的试改不会写回原场景。

DebugPanel 的临时 Lab 也沿用这条边界：Lab 激活时禁用命令历史和场景保存，Hierarchy 不枚举临时实体，退出后恢复原 Editor Scene。这样诊断工具可以创建真实 ECS 场景，又不会混进项目文件。

目前 Tag、SpriteRenderer、ModelRenderer 等少数属性仍有直接修改路径。共享 Material 已有保存、撤销和失败回滚；TerrainMaterial 只有显式 Save/Reload，尚未进入 Asset Command，也没有统一的退出 Dirty 提示。这些是后续编辑器事务继续收口的位置。

## MaterialInstance 与实体材质 Override

多个实体引用同一份 `.glmat` 时，直接修改 `Material` 会让所有引用者一起变化。`MaterialInstance` 解决的是这层共享边界：资产保存基础值，实体只保存自己明确覆盖的字段，渲染提交时再合并成最终材质。

```text
.glmat MaterialProperties
        + MaterialComponent::Overrides
        -> MaterialInstance
        -> Renderer2D / Renderer3D / ShadowRenderer
```

### 覆盖数据怎么组织

`MaterialComponent` 保存 `MaterialHandle` 与 `MaterialOverrides`。Override 由位掩码和一份候选 `MaterialProperties` 组成，现已覆盖 14 个字段：BaseColor、四类纹理、TilingFactor、Metallic、Roughness、AlphaMode、AlphaCutoff、NormalScale、AOStrength、EmissiveColor 和 EmissiveStrength。

关闭某个开关只移除对应 Mask 位，不抹掉它的候选值；重新启用时可以接着使用上次的输入。`Clear()` 才会同时重置 Mask 和全部 Values。ShaderHandle 仍来自基础 `.glmat`，实体不能覆盖 Shader，因为它会改变管线契约，不能当成普通表面参数处理。

`MaterialInstance` 先复制基础属性，再替换 Mask 中启用的字段，并在合并点约束参数范围，例如 Roughness 最低为 `0.04`、TilingFactor 最低为 `0.01`。它只是一次轻量的 CPU 解析，不持有新的纹理或 GPU Material。

### 编辑、保存与运行时结果

Entity Inspector 改的是 `MaterialComponent::Overrides`，不会保存 `.glmat`；Asset Inspector 改的是共享 Material，成功保存后所有未覆盖字段都会继承新值。实体的开关、连续参数、纹理拖放/清除与 Reset 都使用完整组件快照进入 Undo/Redo。Play 模式编辑的是 Runtime Scene 副本，共享 Material Asset 保持只读。

场景 YAML 只在 Override 非空时写出 `Overrides` 节点，并保存 Mask 与完整 Values。旧场景没有该节点时按纯共享材质加载；禁用字段的值仍会往返保存，为以后重新启用保留输入。实体复制、Scene Copy 和 `EntitySnapshot` 都按值复制这份局部状态。

### 渲染侧的处理

Renderer3D 使用 `(EntityID, MaterialHandle)` 缓存合并结果，同时比较基础材质完整状态、Overrides 与版本。即使 Undo 恢复了旧版本号，或某条写入路径漏掉 Dirty，只要内容不同就会重新解析；超过 120 帧未使用的条目会在 `EndScene()` 回收。

最终属性会进入 RenderQueue 的排序与兼容判断。Shader、Mesh、纹理或任何最终材质位模式不同都会拆开 Batch；相同组合才能走 Instanced Draw。ShadowRenderer 也读取同一个 MaterialInstance：Opaque 和 Mask 投射阴影，Mask 使用最终 BaseColor Alpha、纹理、Tiling 与 Cutoff 裁剪，Blend 当前跳过阴影提交。

Renderer2D 继续沿用 Sprite 批处理，材质无效时回退到 Sprite 自身的颜色、纹理和 Tiling；它仍固定使用兼容的 2D Shader，不按 `.glmat` ShaderHandle 切换管线。

这套模型目前没有实体级 Shader Override。MaterialInstance 缓存已经落地，但 Metallic/Roughness 的独立贴图或 ORM 仍属于后续材质通道工作；共享资产方面，只有 Material 具备完整的 Undo、磁盘保存和失败回滚协议。

### 验证记录

无窗口回归覆盖完整 Material 保存/重载、14 项 Override 合并、数值 Clamp 与 Scene YAML 往返。真实 OpenGL Lab 还验证了材质差异会正确拆批：三个相同实例为一次 Draw，单个 Roughness Override 后变为两次 Draw；Alpha 场景则验证了 Opaque、Mask、Blend 的队列与回退行为。

## 项目品牌与 Windows 应用图标

项目早期的 Logo 一直放在 `tmp/logo/`。这在试稿阶段没问题，但临时目录随时可能清理，也不适合作为构建输入。后来把源图、标准 PNG 和 Windows 图标统一迁到 `resources/`，三个可执行程序从此共用一份品牌资源。

### 资源怎么保存

```text
resources/
├─ branding/
│  ├─ GlimmerAppIcon-Source.png
│  ├─ GlimmerAppIcon.png
│  ├─ GlimmerLogo-Crystal.png
│  └─ GlimmerLogo-Minimal.png
└─ windows/
   ├─ Glimmer.ico
   └─ Glimmer.rc
```

高分辨率源图留作后续导出，`GlimmerAppIcon.png` 是文档和界面可直接使用的透明版本。`Glimmer.ico` 内含 16、24、32、48、64、128 和 256 px 图像，小尺寸标题栏与高 DPI 资源管理器都能取得合适层级。

### Windows 构建接入

GLFW 的 Win32 后端会查找名为 `GLFW_ICON` 的资源。共享 RC 文件只有一条声明：

```rc
GLFW_ICON ICON "../resources/windows/Glimmer.ico"
```

`Sandbox`、`GlimmerEditor` 和 `GlimmerEditor-CyouBranch` 的 Premake 配置都把这份 RC 加入 `files`。Visual Studio 生成 `ResourceCompile` 项后，图标会直接写进 EXE；运行时不读取 PNG，也不受启动目录影响。

这里选择共享 RC，而没有给每个应用复制一份图标文件。原因很实际：资源 ID、尺寸集合或 Logo 更新只需维护一个入口。改动 PNG、ICO、RC 或 Premake 后，应重新运行 `scripts\Win-GenerateProject-vs2026.bat`，不要手改随后会被覆盖的 `.vcxproj`。

这条链路目前只覆盖 Windows。Linux 桌面图标和 macOS App Bundle 需要各自的打包方案，不能沿用 Win32 RC。正式品牌文件继续放在 `resources/branding/`，`tmp/` 只留可丢弃的中间产物。

### 验证记录

当时重新生成了三个应用工程并完成 VS2026 `Debug | x64` 全量构建；从生成的 EXE 中成功提取到图标，确认资源已经嵌入，而非碰巧从工作目录加载。对应提交为 `3b14bd8`。

## 材质编辑事务与 Undo/Redo

MaterialInstance 分清了共享 `.glmat` 和实体局部 Override，但最初的 Inspector 仍会直接改内存。拖动一个滑块可能产生几十个中间值，共享材质还会频繁落盘；文件被占用时，画面、磁盘和撤销栈甚至可能处在三个不同状态。材质事务就是为了解决这类不一致。

### 两种编辑对象，两套快照

实体材质使用完整 `MaterialComponent` 快照，里面有 MaterialHandle 和全部 Overrides。共享材质使用 `MaterialState`，一次保存 ShaderHandle 与完整 `MaterialProperties`。两者都走 `ValueEditorCommand<T>`，但 Apply 的落点不同：实体命令只改 Scene 组件，共享资产命令还要保存 `.glmat`。

```text
Entity Inspector                 Asset Inspector
MaterialComponent Before/After  MaterialState Before/After
        -> Scene Component              -> Material Cache
                                           -> .glmat
```

连续控件由 `EditorValueTransaction<T>` 管理。控件激活时截取 Before，拖动期间直接预览，释放时再提交 After，所以一次拖动只生成一条命令。纹理拖放、清除、Override 开关、MaterialHandle 替换和 Reset 属于离散操作，会直接提交完整快照。

当前实体事务已覆盖 14 项 Material Override，包括 BaseColor、四类纹理、Metallic/Roughness、AlphaMode、AlphaCutoff、Normal/AO 参数和 Emissive 参数。共享 Asset Inspector 使用相同的完整属性集合。进入 Play 后共享 Material 变为只读，实体编辑则发生在 Runtime Scene 副本里，Stop 后直接丢弃。

### 失败不会推进历史

`IEditorCommand::Execute()` 与 `Undo()` 返回 `bool`。Execute 失败时不压入 Undo Stack，也不清掉 Redo；Undo 或 Redo 失败时，命令仍留在原栈，处理完文件锁定或权限问题后可以重试。

共享材质的 Apply 会先设置目标状态，再调用 `Material::Save()`。保存失败时恢复先前内存状态，并把错误交给 Inspector 显示。这样撤销栈只记录真正完成的操作，画面也不会假装保存成功。

### `.glmat` 的安全替换

`Material::Save()` 先把完整 YAML 写到同目录 `.tmp` 并检查 Flush，再把旧文件改名为 `.bak`，最后用临时文件替换正式文件。替换失败会尝试恢复备份；如果连恢复也失败，日志会保留备份路径供人工处理。成功后才清理 `.bak`。

这个流程比直接截断原文件多几步，但能保住上一份有效材质。它目前是 Material 专用协议；TerrainMaterial 虽然支持 Save/Reload，还没有接入 Asset Command、统一 Dirty 状态和退出保存提示。

### 验证记录

原生冒烟测试跑过 Execute、文件重载、Undo、再次重载和 Redo。测试还用不共享删除权限的 Windows 文件句柄锁住 `.glmat`：Redo 按预期失败，内存与磁盘保持旧值，命令留在 Redo Stack；解锁后同一条 Redo 可以成功。VS2026 `Debug | x64` 全解决方案构建通过，编辑器在 Intel Iris Xe / OpenGL 4.6 下稳定运行 8 秒。

## 3D Opaque RenderQueue 与状态排序

早期 `Renderer3D::DrawModel()` 边遍历 Scene 边绘制。共享同一个 Shader 和纹理的模型仍会反复绑定，渲染顺序也被 ECS 遍历顺序牵着走。引入 RenderQueue 后，提交与执行被拆开，后来的 Instancing、AlphaMode 和 Transparent Queue 都建立在这次改造上。

### 一帧分成提交和执行

```text
BeginScene
  -> 保存 ViewProjection / CameraPosition，清空本帧队列
SubmitModel
  -> 解析 Model、MaterialInstance、Shader 和纹理
  -> 每个有效 Mesh 生成一个 RenderItem
FlushOpaqueAndMask
  -> 排序、缓存状态、兼容批次 Instancing
EndScene
  -> 透明项远到近绘制，清理过期材质缓存
```

RenderItem 持有 Mesh、Shader 和纹理的强引用，也保存最终 MaterialProperties、Transform、EntityID、纹理存在标记与相机距离。队列执行前资源不会悬空，EntityID 仍能写入整数附件供 Viewport 拾取。

### 排序要比较最终状态

当前 Opaque/Mask 排序键依次包含 ShaderHandle、MaterialHandle、六组纹理 GPU ID、Mesh 地址、完整最终材质位模式、纹理存在标记和 EntityID。最初只比较 BaseColor 纹理已经不够了；Normal、AO、Emissive、导入 Metallic/Roughness 和实体 Override 都会改变实际绑定或 Shader 输入。

相邻 RenderItem 只有在 Shader、Mesh、全部纹理、最终材质值和纹理存在状态一致时才组成兼容 Batch。Shader 支持实例化且 Batch 大于一项时，Transform 与 EntityID 写入动态 Instance Buffer，单次最多提交 1024 个实例；不支持实例化的 Shader 仍走普通 Draw。

执行阶段会缓存已绑定 Shader 和每个纹理槽的 RendererID。Shader 变化时才上传场景级参数并绑定阴影、IBL；材质参数按兼容批次上传。底层 `DrawIndexed` 不再擅自解绑 Texture2D，下一位状态所有者负责覆盖它。这让上层缓存真正有效，也把 OpenGL 状态责任放回 Renderer3D。

无效 Model、Material、Shader、空 Mesh 和零索引 Mesh 会被跳过，不会带着半套资源进入队列。统计中保留 Submitted/Skipped、Draw、Batch、Shader/Texture Bind、Instanced/Individual Draw 与 Material Cache Hit/Miss，DebugPanel 可以直接观察排序是否省下了状态切换。

### 后续演进后的执行边界

章节名保留了最初的 Opaque Queue，但当前 Renderer3D 已把 Opaque 与 Mask 放进同一状态排序队列，Blend 放进独立 Transparent Queue。完整编辑器的顺序是 Opaque/Mask、Terrain、Skybox、Sprite、Transparent；透明项按实体原点到相机的距离由远到近稳定排序，启用标准 Alpha 混合并关闭深度写入，绘制结束后恢复默认状态。

透明队列仍逐项 Draw，不做 Instancing，也没有 OIT；距离使用实体原点，尚未改成 Mesh Bounds 中心。Opaque/Mask 可以实例化，Mask 的材质差异会拆批。Renderer3D 的 MaterialInstance 合并结果也已经按 `(EntityID, MaterialHandle)` 缓存，120 帧未使用的条目会被回收。

### 验证记录

最初的真实 OpenGL 宿主用两种 EntityID 顺序提交三个相同模型，两次都得到 3 个 RenderItem，Shader Bind 从 3 降到 1，Texture Bind 从 3 降到 1，并安全跳过一个无效 Handle。后续 Instancing Lab 把三个完全兼容的模型收成一次 Draw；加入 Roughness Override 后拆为两次 Draw，说明优化没有跨过材质兼容边界。

## 项目工作文档同步约定

项目功能多起来以后，单靠 README 很快就会遇到一个问题：开发过程、当前架构和下一步计划混在一起，隔几天再看，很难判断某段话到底是现状还是旧设想。为此，仓库把长期信息拆到三份文档中，并在根目录 `AGENTS.md` 固定了每次任务的读取与收尾规则。

| 文档 | 保存什么 |
| --- | --- |
| `Documents/PROJECT_STATUS.md` | 唯一当前主线、验收条件、完成记录、优先级和技术债 |
| `ARCHITECTURE.md` | 已经落地的模块职责、所有权、依赖、数据流和边界 |
| `README.md` | 功能用途、开发思路、操作方式、验证记录和踩坑笔记 |

这三个文件解决的问题不同。排期变化只改 PROJECT_STATUS；代码改变模块关系时才改 ARCHITECTURE；用户或开发者能感知到的行为变化写进 README。计划中的结构不能提前写成架构事实，已经完成的工作也不能继续挂在当前主线里。

每项任务结束前都要复核三份文档。没有事实变化的文件保持原样，但要确认它与源码以及另外两份文档没有冲突。真有冲突时以源码为准，当次就修正，避免把过期上下文带到下一台设备或下一次会话。

这套约定看起来像额外步骤，实际省掉了大量重新考古的时间。对应制度建立时，三份文档的职责和触发条件做过交叉检查，根 `AGENTS.md` 也同步加入了执行规则。

## 生态系统路线整合

程序化地形与环境模拟最初有一份独立路线图。随着 HeightMap、材质、渲染队列和水文模拟陆续完成，那份文件开始同时承担计划、实现说明和历史记录，已经完成的 M0/M1 也容易被误读成待办。后来把路线拆回三文档体系，不再维护第二份平行计划。

### 路线放在哪里

可执行阶段、依赖顺序和验收条件统一进入 `Documents/PROJECT_STATUS.md`；模块已经怎样连接写入 `ARCHITECTURE.md`；README 继续保留每项能力的开发过程和使用边界。当前主线以 PROJECT_STATUS 中唯一的 `当前主线` 为准，README 章节顺序只反映建设历史，不承担排期。

截至当前状态，地形链已经走过程序化生成、派生图、Authoring Erosion、四层 TerrainMaterial、Chunk LOD、阴影、运行时水文侵蚀和 CPU/GPU 气候场。P14 正在把气候结果接到 Terrain Material Weight 与植被闭环。这里不再复制完整任务表，因为复制一份很快就会产生两种答案。

### 沿用至今的工程约束

- 同一 Seed 与参数必须得到可复现的程序化结果，生成只由 Dirty 或显式请求触发；
- Compute Pass 使用明确的 Ping-Pong 所有权，不在同一资源上无保护读写；
- 水文和气候按固定时间步推进，状态保持非负，并通过显式 Readback 检查质量预算；
- GPU Runtime、派生纹理和模拟缓存不进入 Scene YAML，场景只保存重建所需参数与 AssetHandle；
- 调试场景放进隔离 Lab，不把测试实体写入默认 Editor Scene；
- 验证按改动范围覆盖无窗口回归、真实 GPU Lab、保存往返、Edit/Play 隔离与差异检查。

有些约束是踩过坑以后留下的。比如每帧 GPU Readback 会把异步 Compute 重新变成同步流程，Runtime Height 若偷偷写回场景又会混淆 Authoring 与 Play。把这些限制放进共同路线，比在各个模块里重复解释更稳妥。

## 3D Instancing 与 MaterialInstance 缓存

Opaque RenderQueue 先解决了排序和重复绑定，DrawCall 数量却没有下降：三个完全相同的模型仍要画三次。Instancing 的目标很具体，只有 Mesh、Shader、纹理和最终材质状态完全一致的项才共享 Draw，其余情况宁可逐项执行。

### 实例输入契约

`BufferLayout` 为每个元素记录 `PerVertex` 或 `PerInstance`。OpenGL VertexArray 连续分配 Attribute Location，矩阵拆成列属性；实例数据通过 `glVertexAttribDivisor(..., 1)` 每个实例推进一次。

```cpp
struct InstanceData
{
    glm::mat4 Transform;
    glm::ivec4 EntityData; // x = EntityID
};
```

PBRModel 使用 location 4 到 7 接收 Transform，location 8 接收 EntityID。`u_UseInstancing` 在实例路径和普通路径之间切换，所以 Viewport 拾取仍能得到每个实例自己的 EntityID。Shader 在链接和热重载后检查 `a_InstanceTransform`、`a_InstanceEntityData` 与 `u_UseInstancing`；缺少任一符号就逐项 `DrawIndexed`，不会强行套用错误布局。

公共 RendererAPI 提供 `DrawIndexedInstanced`，OpenGL 后端落到 `glDrawElementsInstanced`。Renderer3D 的动态 Instance Buffer 单次容纳 1024 项，更大的兼容批次会按 1024 自动分块。

### 合批边界

当前兼容判断比较 Shader、Mesh、六组实际纹理、完整最终 `MaterialProperties` 和纹理存在标记。六组纹理包括 BaseColor、Normal、AO、Emissive，以及模型导入的 Metallic 与 Roughness。实体即使引用同一份 `.glmat`，只要 Override 合并后的结果不同就会拆批；两种 Override 写法得到同一最终状态时则可以合并。

透明项不进入这条实例化路径。Opaque 与 Mask 可以合批，Mask 的 Base Alpha、纹理、Tiling 或 Cutoff 不同都会拆开。自定义 Shader 若没有实例输入契约，也会保留正确的普通 Draw，只是拿不到 DrawCall 优化。

### MaterialInstance 缓存

重复模型每帧重新合并材质也有一笔 CPU 成本。Renderer3D 因此用 `(EntityID, MaterialHandle)` 缓存 ShaderHandle 与最终属性，并保留基础 `MaterialState`、完整 Overrides、版本和最后使用帧。

命中判断会比较完整状态，version 只作为运行期变更信息。这样 Undo/Redo 恢复旧版本号，或某条写入路径漏掉 Dirty 时，只要内容不同仍会重新解析。缓存项超过 120 帧未使用后在 `EndScene()` 回收，不让临时实体永久占用表。

DebugPanel 的 Renderer3D 统计会显示 BatchCount、InstanceCount、Instanced/Individual Draw、SavedDrawCalls 和 Material Cache Hit/Miss。它们比单看 FPS 更容易判断合批是否真的发生。

### 验证记录

真实 OpenGL 临时场景中，三个相同 Cube 从 3 Draw 收到 1 个 Instanced Draw；给其中一个实体增加 Roughness Override 后变成 2 Draw。再加入两个不支持实例契约的 Phong 实体，最终是 5 Items / 4 Draws，两个 Phong 项逐个回退。实例 Transform 与 EntityID 经过真实驱动编译和运行验证，VS2026 `Debug | x64` 全解决方案构建通过；对应提交为 `9053c6a`。

## Transparent RenderQueue 与材质 AlphaMode

RGBA 纹理接入模型后，所有材质继续走不透明路径会留下很直观的错误：透明区域照样写深度和 EntityID，后面的物体也被挡住。为此，Material 明确区分 `Opaque`、`Mask` 和 `Blend`，Renderer3D 则把需要混合的项留到场景后段执行。

### 三种 Alpha 行为

```yaml
Material:
  AlphaMode: Mask
  AlphaCutoff: 0.5
```

- `Opaque` 进入状态排序与 Instancing，保持深度写入；
- `Mask` 计算有效 Alpha，低于 Cutoff 的片元直接丢弃，其余部分仍按不透明物体处理；
- `Blend` 进入透明队列，开启 `SrcAlpha / OneMinusSrcAlpha` 混合，保留深度测试并关闭深度写入。

有效 Alpha 是 `BaseColor.a × BaseColorTexture.a`。Mask 被丢弃的位置不会写颜色、深度或 EntityID；Blend 在 Alpha 小于等于 `1/255` 时也会丢弃，避免肉眼不可见的片元抢走 Viewport 拾取结果。旧 `.glmat` 没有新字段时按 `Opaque / 0.5` 加载。

AlphaMode 和 AlphaCutoff 同时存在于共享 Material 与实体 Overrides，Inspector 编辑、Undo/Redo、MaterialInstance 合并和 Scene YAML 往返使用同一字段。自定义 3D Shader 若要声明支持这套行为，需要消费 `u_AlphaMode` 与 `u_AlphaCutoff`；Renderer 不会替 Shader 自动补上裁剪代码。

### 为什么要拆队列

`SubmitModel()` 先解析最终 MaterialInstance。Opaque 与 Mask 进入同一个状态排序队列，可以按严格材质条件实例化；Blend 进入 TransparentQueue，按实体原点到相机的平方距离由远到近稳定排序，并逐项 Draw。

```text
Opaque / Mask Models
  -> Terrain
  -> Skybox
  -> Sprite Batch
  -> Transparent Models
```

Sprite Pass 曾经在 Skybox 前实际 Flush，透明像素先和 Clear Color 混合，随后天空盒又覆盖背景。RenderDoc 抓帧定位到顺序问题后，完整编辑器改为让 Scene 暂存 Sprite Pass，等 Skybox 绘制完成再调用 `FlushSpritePass()`。不负责 Skybox 编排的旧宿主仍使用立即执行的默认路径。

透明 Pass 结束会恢复 Blend 禁用、DepthWrite 启用和 `DepthFunc::Less`。Renderer2D、Skybox 和 Renderer3D 各自声明并恢复所需状态，不再依赖上一段渲染碰巧留下正确的 OpenGL 配置。

### 当前边界与验证

透明排序仍使用实体原点，没有按 Mesh Bounds 中心计算；Transparent 不参与 Instancing，也没有双面材质或 OIT。Sprite 与 3D Blend 属于两套队列，当前没有跨队列距离排序。阴影侧只接收 Opaque 与 Mask，Blend 默认跳过，避免半透明表面投出整块实心阴影。

真实 OpenGL 烟测使用带 0 到 255 Alpha 的 `balatro.png`：2 个相同 Opaque、1 个 Mask、2 个不同距离 Blend 得到 `5 Items / 4 Draws`，其中 Opaque 合为一次 Instanced Draw，两个 Blend 各画一次。旧材质兼容、新字段保存重载和实体 Override YAML 往返也在同一轮通过。

## 可扩展 Debug 面板与 GPU Instancing Lab

渲染优化只看一帧日志很难判断对错，往默认场景塞几千个测试实体又会污染项目。`Window -> Debug` 因此成为编辑器里的诊断入口：面板展示统计，独立 Tool 创建可控的测试场景，正式 Renderer 仍走正常代码路径。

### 临时场景的所有权

Instancing Lab 会创建真实 ECS Scene，再通过 EditorLayer 回调临时替换 `m_ActiveScene`。`m_EditorScene` 始终保留，退出 Lab、切换场景、进入 Play 或关闭编辑器时都会恢复。Lab 激活期间禁用 CommandHistory 和场景保存，Hierarchy 只显示提示，不枚举成千上万个实体干扰 CPU 统计。

```text
Editor Scene 保留
  -> Tool 创建临时 Scene
  -> EditorLayer 切换 Active Scene 与面板上下文
  -> Scene / Renderer3D 正常提交和绘制
  -> Exit 时释放临时 Scene，恢复 Editor Scene
```

关闭 Debug 窗口只隐藏面板，不会偷偷销毁正在观察的场景。Instancing Lab 与 PBR Material Lab 互斥，同一时刻只有一个 Tool 可以占用临时场景边界。

### Instancing Lab 怎么用

默认网格是 `50×1×50`，共 2500 个 Cube，实体上限为 100000。Count、Spacing 和 Origin 可以调整，Model 与 Material 也能从 Content Browser 拖入。常用预设有三种：

- Maximum Instancing 让所有实体共享最终状态，用来观察 1024 实例分块；
- Material Split 交替设置两档 Roughness，预期拆成两个材质组；
- Transparent Comparison 把全部实体设为 Blend，预期 Instanced Draw 为 0。

2500 个单 Submesh 实体在 Maximum Instancing 下会拆成 `1024 + 1024 + 452`，理论值是 3 个 Instanced Draw，节省 2497 次 Draw。Tool 会按实体数、Submesh 数、预设和分块上限计算预期 Items、Draws 与 InstanceCount，再逐帧对比 Renderer3D Statistics。数据尚未对应新场景时显示 Pending，一致且没有 Skipped Model 时才显示 PASS。

首个、中间和末尾代表实体可以直接选中，用来检查同一次 Instanced Draw 写出的不同 EntityID。这个小功能很有用，它能抓到画面看似正确、拾取却全部落到同一实体的实例数据错误。

### 后续接入的诊断工具

DebugPanel 现在还包含 PBR Material Lab、Terrain Overview 和 Terrain Sampling Benchmark。InstancingLabTool 后来又承载 CSM 性能与视觉验证：自动基准使用固定 2500 实体，在 9 组 Cascade/Resolution 配置间轮换，每组预热 15 帧并收集 30 个新的 GPU Timer 样本；视觉预设则生成 Opaque、Mask、Blend 投影对照和级联着色场景。

这些扩展继续遵守原来的边界。Tool 拥有参数、临时 Scene 和预期统计；DebugPanel 只负责分类与窗口；EditorLayer 只处理场景切换和生命周期。首版 Instancing Lab 验收时重新生成了 VS2026 工程，全解决方案构建通过，完整编辑器稳定运行 8 秒。

## PBR 材质纹理通道扩展与 Material Lab

基础 Cook-Torrance PBR 最初只有 BaseColor 纹理，金属度和粗糙度则是两个标量。Normal、AO 和 Emissive 加入后，改动贯穿 `.glmat`、实体 Overrides、Inspector、Scene YAML、MaterialInstance、RenderQueue 与 Shader；少改一处，保存往返或合批就会悄悄丢字段。

### 字段与颜色空间

MaterialProperties 当前提供 BaseColor、Normal、AO、Emissive 四个资产纹理 Handle，并配有 NormalScale、AOStrength、EmissiveColor 和 EmissiveStrength。旧文件缺少这些字段时使用无纹理、Normal/AO 强度 1、Emissive 强度 0，因此旧材质加载后不会自行发光或改变表面方向。

纹理元数据按内容解释：

| 通道 | 颜色空间 | 语义 |
| --- | --- | --- |
| BaseColor | sRGB | Color |
| Normal | Linear | Normal |
| AO | Linear | Data/Height |
| Emissive | sRGB | Color |

BaseColor 与 Emissive 是颜色输入，采样后进入线性 HDR 计算；Normal 和 AO 保存数值，不能执行 Gamma 解码。Inspector 拖放时会写入相应元数据，元数据变化会清除该 Texture Handle 的 GPU 缓存。Renderer 只读取语义兼容的纹理，不在 Draw 中改写 AssetRegistry。

### Shader 里的处理

四个材质纹理固定占用纹理单元 0 到 3。缺图时绑定白纹理保证槽位有效，同时使用独立的 `u_Has*Texture` 阻止错误采样。Renderer3D 的排序和兼容判断包含纹理 GPU ID、存在状态与完整最终材质参数，所以贴图或强度 Override 不同的实体会拆批。

Normal Map 通过 World Normal 与 World Tangent 构造 TBN。切线先做 Gram-Schmidt 正交化，模型导入器对退化 UV 和零切线提供稳定正交基回退，避免出现 NaN。当前 Tangent 仍是 `vec3`，镜像 UV 所需的 Handedness 还没有保存。

AO 只调制环境光和 IBL，不重复压暗方向光、点光等直接照明。Emissive 不进入 BRDF，它把 sRGB 解码后的纹理乘以颜色与强度，直接加到线性 HDR Radiance，最后再经过 Exposure、Bloom 和 Tone Mapping。

共享 `.glmat` 目前仍没有 MetallicTexture、RoughnessTexture 或 ORM 字段。模型导入路径已经可以携带独立 Metallic/Roughness 运行时纹理，并在材质缺失对应来源时通过纹理单元 11/12 参与 PBR；这些纹理尚未资产化为 Material Handle，也不会自动生成 `.glmat`。

### PBR Material Lab

`Window -> Debug -> Rendering` 中的 PBR Lab 会生成六个并排的 UV Sphere，分别检查 Normal、AO、Emissive、Dielectric Smooth、Metal Smooth 和 Metal Rough。球体使用真实 MaterialOverrides 与 Renderer3D 链路，面板按 Model 的 Submesh 数计算预期 RenderItem，实际数量一致且没有跳过项时显示 PASS。

设置 `GLIMMER_PBR_LAB_AUTORUN=1` 可在启动时自动生成场景，并在系统临时目录执行旧 `.glmat` 兼容、完整 Material 保存重载和 Scene Override YAML 往返。验证文件完成后删除，不写入项目资产。验收日志记录 `PBR Material Lab PASS: rendered 6/6 items`，VS2026 `Debug | x64` 全解决方案构建与真实 OpenGL 运行均通过。

## 无窗口回归测试与 Windows 一键验证

DebugPanel Lab 能验证真实 Shader、Framebuffer 和 DrawCall，但它依赖窗口与 GPU，新设备上的第一轮排错不该从这里开始。`GlimmerRegressionTests` 是独立 ConsoleApp，只链接可在 CPU 侧验证的引擎与编辑命令代码，不创建 Application、Window 或 OpenGL Context；断言失败直接返回非零退出码。

### 这套测试负责什么

测试文件通常写进系统临时目录，进程结束时清理，不修改项目资产或默认 Scene。当前覆盖的范围已经包括：

- Material、TerrainMaterial 和 Scene YAML 往返，以及旧字段的兼容默认值；
- MaterialInstance Override 合并、参数 Clamp 和稳定 UUID 查找；
- OBJ/FBX 到 MeshSource 的 CPU 导入，Cerberus FBX 使用仓库内版本化样本；
- Terrain 规格复制、Runtime 隔离、Preset、Chunk Layout 和 Camera Frustum Culling；
- CommandHistory 的 Execute、Undo、Redo 状态迁移；
- Shadow Frustum Culling 与 Environment Map 的纯数据基础；
- Terrain 水文和气候 Runtime 的守恒、Reset、固定步与帧划分确定性。

GPU Shader 编译、纹理采样、Framebuffer、Instancing 和实际 Draw 仍交给完整编辑器与 Debug Lab。把两类测试分开后，数据层失败不会被显卡环境掩盖，渲染问题也不用硬塞进一个伪造 Context 的单元测试里。

### Windows 一键入口

新设备先初始化递归子模块，然后在仓库根目录运行：

```powershell
git submodule update --init --recursive
.\scripts\Verify-Windows.bat
```

脚本会检查子模块和 Assimp Debug 产物，缺失时补建依赖；随后运行 Premake VS2026、构建 `GlimmerEngine.slnx` 的 `Debug | x64`，最后执行测试程序。`.bat` 没有 `pause`，并原样传播 PowerShell、MSBuild 或测试进程的退出码，终端和后续 CI 都能直接判断结果。

已经生成并构建过工程时，可以只跑测试：

```powershell
.\scripts\Verify-Windows.bat -SkipGenerate -SkipBuild
```

`-ForceTestFailure` 会把 `--force-failure` 传给测试程序，用来确认失败码没有在脚本层丢失。测试目标禁用了 Debug 增量链接，原因是一次损坏的 `.ilk` 曾生成缺失系统导入表的 EXE；数据测试本身没错，进程却以 `0xC0000005` 提前退出，这类构建缓存问题必须和断言失败分开。

初版一键入口有 23 项断言，Terrain 阶段扩到 46 项；P13C 验收时已达到 114 项并全部通过，当前套件还加入了气候 Runtime。历史数字保留作里程碑证据，不拿旧计数冒充今天固定不变的测试规模。

## Terrain 生命周期与 Inspector 编辑事务收口

Terrain 同时有可保存配置和一大组 GPU Runtime。若复制实体时顺手共享 Runtime，两个 Scene 会指向同一套 Height、Mesh、水文或气候状态，Play 模式改一下就可能污染编辑场景。这里的规则很干脆：复制规格，丢弃 Runtime，由目标 Scene 自己重建。

### Specification 与 Runtime 的界线

`TerrainComponent` 保存 `TerrainSpecification` 和 `Ref<TerrainRuntime>`。复制构造与复制赋值只复制 Specification，并将 Runtime 置空。Duplicate Entity、`Scene::Copy()`、`EntitySnapshot` 和 Undo/Redo 都会经过这条语义。

```text
TerrainSpecification
  -> 可复制、可撤销、写入 Scene YAML

TerrainRuntime
  -> Mesh / LOD / Chunk 状态
  -> Height 与派生纹理
  -> Generator / Hydrology / Climate GPU 状态
  -> 只属于当前 Scene，不序列化
```

`TerrainRenderer::Prepare()` 在下一次绘制时创建空 Runtime，并检查 MeshResolution、HeightMapResolution、资源 Handle 和生成版本。程序化地形按需创建 Generator；外部 HeightMap 则解析纹理并重新建立派生图。水文与气候 Runtime 也跟随 `GenerationVersion` 重建，不会继续使用旧高度对应的模拟状态。

场景文件只保存重建所需的 TerrainSpecification，包括 Preset、Noise、Authoring 参数、HeightMap 与 Shader Handle、TerrainMaterialHandle。OpenGL ID、生成器指针、运行时 Height、Water、Sediment 和 Climate 纹理都不进入 YAML。

### Inspector 的一次拖动

Terrain 连续控件使用 `EditorValueTransaction<TerrainComponent>`：按下时保存完整 Before，拖动中实时预览，释放时把 Before/After 作为一条 `ValueEditorCommand` 压入历史。Apply、Undo 和 Redo 通过组件复制赋值恢复规格，同时让旧 Runtime 失效。

Preset、分辨率、高度、Noise、Geology 和 Authoring Erosion 参数都沿用这条事务边界。HeightMap、Compute Shader 与 TerrainMaterial 的拖放或清除属于离散命令。Light、SkyLight 和 Camera 的连续属性也复用了同一套做法。

`Regenerate` 只把运行时标记为 Dirty，用当前规格重新生成，不改可序列化数据，所以不会制造一条空洞的 Undo。旧 `TerrainPanel` 曾自己持有 Generator 指针与第二份 Noise 状态，但它没有接入当前 EditorLayer，后来已经删除；正式参数只由组件 Inspector 拥有。

### 验证记录

无窗口测试验证了实体复制、Scene YAML、Edit 到 Play 的 Scene Copy，以及 Terrain 命令 Apply、Undo、Redo：规格保持一致，副本 Runtime 始终为空，运行场景修改不会回写编辑场景。P7 时该组回归随 Preset 扩到 46 项；后续水文与气候测试继续沿用同一 Runtime 隔离约束。

## 山脉生成、派生图与 Authoring Erosion

最早的程序化高度更像均匀噪声起伏，能画出地表，却很难得到有方向的山链、台地或沟谷。山脉生成这一轮把地貌参数、有限次热侵蚀和派生图串成一条 Authoring 管线，用户点击 Regenerate 后能得到可复现的基础地形。

### Preset 与可调参数

Inspector 提供 Alpine、Plateau、Rolling Hills、Volcanic 和 Eroded Valley 五个 Preset。选择 Preset 会一次性写入 Seed、Noise、HeightScale 和 Thermal Erosion，并作为一条命令进入 Undo/Redo；之后手动修改任一地貌参数，Preset 会回到 Custom。

方向性山链由 MountainDirection 与 MountainWidth 控制，PlateauStrength 负责台地过渡，Channel Erosion 在基础噪声里刻出沟谷。后续地质细化又加入 GeologyBlend、GeologyScale、RiftStrength 和 TrendStrength，用多套结构场混合断层、裂谷与大尺度走向。它们都属于生成参数，不是逐帧模拟状态。

Thermal Erosion 有 Enable、Iterations、Talus 和 Strength。Iterations 在运行时限制为最多 128，Strength 限制到 0.5。它处理局部坡差，和 GenerateFBM 中的 Channel Erosion 是两件事：前者多轮搬运高度，后者直接参与基础形状函数。

### 三段 Authoring 管线

```text
GenerateFBM.comp
  -> 大陆、山链、台地、火山、沟谷和地质结构
ThermalErosion.comp × N
  -> Read Height / Write Height / Barrier / Swap
DeriveTerrainMaps.comp
  -> Normal+Slope / Curvature+Flow+Height / 四层 Material Weights
```

高度使用 `R32F SimulationGrid`。每轮热侵蚀只读当前纹理、只写另一张纹理，Barrier 后再交换索引，没有同纹理的无保护读写。默认 Alpine 是 1 次生成、28 次热侵蚀和 1 次派生，共 30 次 Dispatch。

三张派生图都是运行时 `RGBA16F`：Normal/Slope 保存编码法线与坡度，Analysis 保存曲率、局部 Flow Potential 和高度，MaterialWeight 保存 Grass、Soil、Rock、Snow 四层归一化权重。P14 正在为这份静态地貌权重加入气候与植被反馈，但动态生态输入不会反过来重跑整条 Authoring 管线。

### 与运行时侵蚀的分工

Authoring Erosion 只在 Terrain Dirty、用户 Regenerate 或 Compute Shader 成功热重载后运行，普通渲染帧不会继续改变基础高度。P13 的水文侵蚀使用另一套固定步 Runtime Height、Water 与 Sediment 状态；它不改生成初态，也没有隐式 Bake。运行时高度真的变化时，每个 Color Frame 最多刷新一次派生图，Shadow 和九个 Chunk 的重复 Prepare 不会重复推进模拟。

三个 Authoring Compute Shader 都支持热重载。成功编译后 Terrain 变为 Dirty 并完整重建；失败时继续保留上一份有效 Program 和地形结果。

### 验证记录

五类 Preset 的重复应用与参数范围通过无窗口回归。真实 OpenGL 验证覆盖 Generate、Thermal Erosion 和 Derive Maps：同一 GPU 上相同 Seed/参数连续生成的组合哈希一致，全部 Height 与派生通道有限且在合法范围内，四层材质权重和保持为 1。同步 Readback 只在 `GLIMMER_TERRAIN_VALIDATE=1` 验证模式执行，普通编辑流程没有这笔开销。

## TerrainMaterial 四层 Triplanar PBR

这一轮给地形补上了独立的 `TerrainMaterial` 资产。它负责把地形生成阶段得到的高度、坡度、曲率和湿润度，转换成草地、泥土、岩石、积雪四层材质。地形网格只提供形状，地表看起来像山坡、裸岩还是雪线，由这层资产决定。

### 为什么单独做一种材质资产

普通模型材质围绕 Mesh Slot 工作，地形材质却要处理固定的四层混合和一组派生图。硬塞进 `MaterialInstance` 会让模型材质背上很多用不到的字段，所以这里使用 `.glterrainmat`，并让 `TerrainComponent` 只保存它的 Asset Handle。场景 YAML 不保存运行时生成的纹理，也不会复制一份共享材质参数。

四层结构保持固定，编辑时更容易理解，也让 Shader 的绑定布局稳定：

| 层 | 常见用途 | 可调内容 |
| --- | --- | --- |
| Grass | 平缓、湿润区域 | Base Color、Albedo、Normal、AO、Tiling、Metallic、Roughness |
| Soil | 草地与岩石之间的过渡 | 同上 |
| Rock | 陡坡和高曲率区域 | 同上 |
| Snow | 高海拔区域 | 同上 |

全局参数负责控制 Triplanar 锐度和权重对比度，也能分别调节高度、坡度、曲率、湿润度对混合结果的影响。贴图按用途注册：Albedo 使用 sRGB/Color，Normal 和 AO 保持 Linear。缺图时会回退到层的 Base Color、几何法线和 `AO = 1`，因此材质仍能正常显示。

### 从派生图到最终地表

`TerrainRenderer` 先绑定 Height、Normal/Slope、Analysis 和 MaterialWeight 四张地形纹理，再绑定四层材质贴图。Shader 根据最终权重混合各层 PBR 参数，并以世界坐标在 X、Y、Z 三个方向投影纹理。投影权重来自世界法线，所以近乎垂直的山壁不会像普通 UV 那样被拉成长条，网格 LOD 改变时纹理密度也能保持一致。

四层是材质表达模型，实际采样量由质量档位决定。当前默认的 Full 4 Layers 会完整计算四层；Top-2 会保留权重最高的两层，Dominant 档进一步只让主层提供 Normal/AO，Auto Distance 则在近处 Top-2 和远处主层细节之间平滑切换。编辑器因此保有稳定的完整质量基线，低端设备也能按距离控制 Terrain、CSM、IBL 和诊断纹理共同带来的采样开销。

开发时我刻意保留了两条边界：

- Asset Inspector 的 Save/Reload 仍是显式操作，材质字段还没有接入统一的 Command History、Dirty 和退出保存提示；
- 默认内存场景的 `TerrainMaterialHandle` 为 `0`，此时使用内建回退材质。只有场景明确引用 `.glterrainmat` 时，才会加载资产中的贴图。

当前 MaterialWeight 仍由静态地貌派生。P14 已经产出 Temperature、Humidity 和 VegetationPotential，但这些动态场尚未回写材质权重；这项工作留在气候与植被闭环里处理，避免每个气候步都重复整套地形派生。

验证覆盖 `.glterrainmat` 的保存与加载、场景 Handle 往返、缺失贴图回退、四层权重归一化，以及 Debug/Release 构建。默认材质资产可直接在 Terrain Inspector 中拖放和重载。

## 方向光 Shadow Map 与 CSM

单张方向光阴影图在近处容易糊，分辨率拉高后又会把大量像素花在远景。这一轮把方向光阴影改成 Cascaded Shadow Maps，按相机深度把可见范围切成 1 到 4 段，让近景得到更密的阴影采样，远景继续保留轮廓。

### 一帧阴影是怎样生成的

场景取第一个启用且打开 `Cast Shadows` 的方向光。它的 Resolution、Cascade Count、Shadow Distance、Bias、Split Lambda 和 Blend Width 随场景序列化；深度纹理、Framebuffer 和 Light ViewProjection 只属于运行时。

每帧的处理顺序如下：

1. 使用 Practical Split 计算各级联距离，并为相邻级联留出混合区；
2. 用包围球稳定光空间范围，再把投影中心吸附到 Shadow Texel，减轻相机移动时的阴影抖动；
3. 按级联视锥剔除模型和 Terrain Chunk，只把可能投影到该范围的物体放进 Shadow Queue；
4. 把 Opaque 与 Mask 模型按 Mesh 和最终 Mask 状态排序，相容项合并成最多 1024 个实例的深度批次；Terrain 走自己的 Chunk/LOD 深度路径；
5. PBRModel 和 Terrain 在颜色阶段按视空间深度选择级联，用 3x3 PCF 采样，并在重叠区平滑混合。

Shadow Pass 使用独立的 `Depth32F` Framebuffer，不写颜色附件。模型阴影也有自己的缓存 VAO，实例矩阵占用 location 4 到 7；这样深度批次不会改坏 Renderer3D 正在使用的实例缓冲。

### 材质透明度与光照边界

Mask 材质会沿用最终 `MaterialInstance` 的 BaseColor Alpha、贴图 Alpha、Tiling 和 Cutoff。材质没有覆盖贴图时，再回退到 Mesh 导入的 BaseColor 贴图。Blend 物体暂时不写阴影，因为半透明投影需要抖动、透射或排序策略，直接写实心深度会得到错误结果。

接收端只用阴影衰减方向光的直接光照，Ambient、Emissive、Point Light 和 IBL 保持不变。当前也只支持方向光 CSM，点光和聚光阴影还没有进入这条管线。纹理槽按渲染器分区：模型使用 4 到 7，Terrain 使用 16 到 19，避免和材质、IBL 资源互相覆盖。

### 调试与性能记录

Debug Panel 可以显示级联着色，红、绿、蓝、黄分别对应四段；这个开关只影响当前运行，不写入场景。GPU 计时采用非阻塞查询，基准工具只在拿到新样本后推进，防止把旧结果重复计入。

固定 `2500` 个实例的 Maximum Instancing 场景跑过 `1/2/4` 级联与 `1024/2048/4096` 分辨率的九组组合。RTX 4060 上从 `1024 x 1` 的平均 `0.748 ms` 增长到 `4096 x 4` 的 `6.313 ms`；同一最高档在 Iris Xe 上为 `11.224 ms`。这组数据确认成本主要跟级联数量和分辨率增长，也说明默认值需要给画质和显存留余地。

视觉验收另外覆盖了级联接缝、Mask 轮廓、Acne、Peter Panning，以及 Blend 不投实心阴影。P9 完成时通过 VS2026 `Debug | x64` 构建和 64 项无窗口回归。

## Renderer2D 空批次残留修复

移除实体的 `SpriteRendererComponent` 后，画面偶尔还会留下上一帧的白色方块。问题出在两个接口对数字 `0` 的理解不同：Renderer2D 用 `QuadIndexCount = 0` 表示本帧没有 Sprite，底层 `DrawIndexed(0)` 却把它解释成绘制 VertexArray 的完整 IndexBuffer。动态 VBO 里的旧顶点因此又被预生成索引提交了一次，并落到 0 号白纹理。

修复放在批次所有者 `Renderer2D::Flush()` 中：

```cpp
void Renderer2D::Flush()
{
    if (s_Data.QuadIndexCount == 0)
        return;

    // Bind textures and issue the actual batch draw...
}
```

我没有修改 `DrawIndexed(0)` 的全局约定，因为模型等调用方可能还在依赖绘制完整索引缓冲的语义。让 Renderer2D 在空批次处提前返回，范围更小，也能保证添加、移除及 Undo/Redo Sprite 组件后，空帧不会绑定纹理、增加 Draw Call 或重画旧数据。

修复通过 VS2026 `Debug | x64` 编辑器构建、55 项无窗口断言和 Intel Iris Xe/OpenGL 4.6 默认 Alpine 场景启动检查。

## tmpTerrain 地质地貌迁移实验

这次实验的目标很直接：看看 `tmp/tmpTerrain` 原型里哪些地貌思路值得留下，又能否接进 Glimmer 已有的 Terrain 管线。最后我只迁移了 Worley 地质块、陡峭区遮罩、裂谷和大尺度趋势，并把它们重写进 `GenerateFBM.comp`。原型代码没有整段照搬，Terrain Entity、热重载、Dirty 重建、热侵蚀、派生图和场景序列化仍走原来的生命周期。

### 接进现有生成器

Inspector 的 `Geological Features` 区域提供四个参数：

| 参数 | 用途 |
| --- | --- |
| Geology Blend | 在原高度与地质塑形结果之间混合，设为 0 可精确回到旧生成公式 |
| Geology Scale | 调整 Worley 地质块和裂谷的尺度 |
| Rift Strength | 控制狭长低地对高度的削减量 |
| Trend Strength | 沿 Mountain Direction 加入大尺度高低趋势 |

修改参数会把 Preset 切到 Custom，并通过 Inspector 事务压成一次 Undo/Redo。Terrain Runtime 随后标记为 Dirty，下一次 `Prepare()` 才重建 Height、Normal/Slope、Analysis 和 MaterialWeight。四项参数会写入 Scene YAML，各个内置 Preset 也有自己的保守默认值。

生成顺序没有因此分叉：

```text
原有 Domain Warp / Continental / Ridge / Channel
  -> Worley Relief / Steep Mask / Rift / Directional Trend
  -> Geology Blend
  -> Preset 修正
  -> Thermal Erosion Ping-Pong
  -> Normal / Analysis / MaterialWeight 派生
```

### 原型里没有迁移的部分

我没有接入原型中的水流、泥沙、蒸发和气象代码。它在同一次 Dispatch 中写入 WaterFlow，做完 Workgroup Barrier 后便读取相邻 Workgroup 的结果；这个同步只能约束组内线程，跨组数据仍可能不可见。把这种代码塞进正式 Terrain 路径，画面也许能动，结果却无法稳定复现。

后来的 P13 水文实现沿用了这次实验确定的边界：Rain/Source、Flux、Water Update、Sediment、Erosion/Deposition 和 Derive Maps 分成独立 Dispatch，状态使用 Ping-Pong 纹理，阶段间设置全局 Memory Barrier。P14 气候也通过统一的固定步环境时钟与水文耦合，没有回头复用原型的单 Pass 写法。

这部分在 GTX 1050/OpenGL 4.6 上完成真实 Shader 验证。默认 Alpine 一次生成包含 30 次 Dispatch，两轮输出的有限值、范围和四层权重归一化均通过，确定性 Hash 为 `16881604791310884879`；当时的 VS2026 `Debug | x64` 构建和 64 项无窗口回归也全部通过。

## TerrainMaterial Top-2 采样与 GPU 基准

四层 Triplanar PBR 的画质不错，代价也很实在。Grass、Soil、Rock、Snow 都读取 Albedo、Normal 和 AO，每张贴图又要做三个方向的投影，最重路径接近每像素 40 次纹理采样。这个阶段的工作是给它增加可控的质量档位，并用真实 GPU 数据判断省下来的采样是否值得。

### 四种采样方式

权重筛选发生在 Height、Slope、Curvature 和 Flow/Moisture 修正完成之后，因此不会改变地形生成、权重派生或 PBR 光照公式。

| 模式 | 实际处理 |
| --- | --- |
| Full 4 Layers | 四层都贡献 Albedo、Normal、AO、Metallic 和 Roughness |
| Top 2 Layers | 只保留权重最高的两层，重新归一化后采样完整 PBR 数据 |
| Top 2 + Dominant Normal/AO | 两层混合 Albedo，Normal 和 AO 只取主层 |
| Auto Distance | 近处保留 Top-2 细节，远处淡出次层 Normal/AO |

Auto Distance 的 Detail Distance 默认为 80 个世界单位，阈值前后各 15% 组成 `smoothstep` 过渡带。这里不能硬切。次层法线如果在某个距离突然消失，相机前后移动时会看到整片山坡闪一下，比省下几次采样更显眼。

当前编辑器默认使用 Full 4 Layers。Top-2 会收窄原本由三层或四层共同形成的过渡，Dominant 和 Auto 还可能让表面细节随观察距离变化；完整四层更适合作为编辑画质基线。性能紧张时再从 Debug Panel 切换模式。`TerrainMaterialHandle = 0` 的默认内存场景仍使用内建颜色，它不会加载 DefaultTerrain 的 11 张材质贴图，也可以作为轻量对照。

### 计时方式与结果

`TerrainRenderer` 有自己的非阻塞 GPU Timer。统计只有在新的 Query 结果可用时才更新，面板不会为了显示耗时卡住 CPU。`TerrainSamplingBenchmarkTool` 只切换运行时采样模式并读取 Statistics，不拥有 Scene；每档先收集 15 个新样本预热，再记录 30 个新样本。

GTX 1050/OpenGL 4.6 的固定 Alpine 场景得到以下结果：

| Sampling Mode | Average | Minimum | Maximum | 相对 Full-4 |
| --- | ---: | ---: | ---: | ---: |
| Full 4 Layers | 10.681 ms | 10.449 ms | 11.510 ms | 基线 |
| Top 2 Layers | 6.026 ms | 5.790 ms | 6.661 ms | 降低 43.6% |
| Top 2 + Dominant Normal/AO | 4.226 ms | 4.006 ms | 5.122 ms | 降低 60.4% |

自动验证入口 `GLIMMER_TERRAIN_SAMPLING_BENCHMARK_AUTORUN=1` 会临时给默认 Terrain 分配 DefaultTerrain、固定相机，完成三档测试后正常退出，不保存场景。`GLIMMER_TERRAIN_SAMPLING_VISUAL_MODE=0..3` 用于逐档截图。固定视口检查没有发现新的轮廓变化、Triplanar 方向错误或条带接缝；默认档调整也有回归断言锁定为 Full-4。

## HDR 环境 Cubemap 与 Mip Chain 基础

IBL 开工前先碰到一个更基础的问题：Renderer 只认识六张 LDR 天空盒图片，常见的 Radiance `.hdr` 等距柱状环境图进不来。这个阶段先统一环境源，把六面 `.glsky` 和单张 `.hdr` 都转换成相同方向约定、带完整 Mip Chain 的 `TextureCube`。天空盒和后续环境光从这里开始共享同一份输入。

### 资产入口

项目 `assets` 下的 `.hdr` 会直接注册为 `AssetType::Cubemap`，可以拖到 Viewport 或 SkyLight 的 Cubemap 属性。如果需要固定名称和输出面尺寸，则使用 `.glsky`：

```yaml
Cubemap:
  Source: "../textures/environment.hdr"
  Resolution: 512
```

`Source` 相对 `.glsky` 所在目录解析。留空时继续读取 `Right/Left/Top/Bottom/Front/Back` 六面字段，旧资产无需转换。Cubemap Inspector 会显示源类型、实际路径、格式、面尺寸、Mip 数和 Runtime Version，也可以显式 Reload。

### 从经纬图到 GPU 纹理

```text
.hdr 或 .glsky Source
  -> AssetManager Cubemap Handle
  -> Cubemap::Reload()
  -> EnvironmentMapLoader 解码线性 RGBA float
  -> 双线性 Equirectangular-to-Cubemap
  -> RGBA16F TextureCube
  -> GenerateMipmaps() 直到 1x1
```

`EnvironmentMapLoader` 位于 Renderer 核心层，负责经度循环、纬度钳制和 `+X/-X/+Y/-Y/+Z/-Z` 的方向转换。OpenGL 层只创建不可变存储、上传指定面与 Mip，并调用 Mip 生成。HDR 数值全程保持线性，不会先压进 0 到 1，也不会走 sRGB 解码。

`TextureCubeSpecification::MipLevels` 显式记录级数，完整链为 `floor(log2(faceSize)) + 1`。传统六面 LDR Cubemap 同样生成完整链并使用 Trilinear Min Filter。这里生成的是普通颜色下采样，适合稳定天空盒采样；粗糙度反射使用后续单独生成的 Specular Prefilter，不能拿普通 Mip 冒充。

### Reload 与派生资源

Cubemap 每次成功加载都会更新源路径、HDR 标记和 Runtime Version。Environment Lighting 用下面这组信息管理派生缓存：

```text
Source Handle + Runtime Version + 派生类型 + 生成参数
```

因此 HDR Reload 只会让相关的 Diffuse Irradiance 和 Specular Prefilter 失效，普通帧不会重复卷积。BRDF LUT 与具体环境无关，作为进程级共享资源管理。后续章节记录了这三部分 IBL 的接入过程；当前代码已经完整使用这套缓存边界。

验证曾抓到一个很隐蔽的极点问题：纬度行号虽然被钳制，双线性插值权重仍来自钳制前的坐标。修复为先钳制连续纬度，再计算权重。71 项无窗口回归覆盖完整 Mip 数、六面中心方向、经纬采样方向、HDR 高亮值和 Radiance 解码；GTX 1050/OpenGL 4.6 启动检查也通过，没有出现 Shader、Framebuffer 或 OpenGL 断言。

## Diffuse Irradiance 环境漫反射

HDR Cubemap 接通后，SkyLight 仍只是背景图，关掉方向光，模型和地形很快就暗成一片。P10 的第二步是从同一张环境图生成低频漫反射，让朝向不同的表面接收到对应方向的天空颜色。

### 环境光怎样进入材质

Scene 只提交第一个启用的 `SkyLightComponent`，内容是 Cubemap Handle、Intensity 和 Enabled 状态。`EnvironmentLighting` 解析资产并生成 Irradiance，Renderer3D 与 TerrainRenderer 再把它绑定到各自预留的纹理槽：

```text
SkyLight Cubemap Handle + Intensity
  -> AssetManager::GetCubemap()
  -> EnvironmentLighting 派生缓存
  -> 读取线性浮点六面数据
  -> EnvironmentMapLoader 余弦卷积
  -> 32x32 RGBA16F Irradiance Cubemap
  -> PBRModel slot 8 / Terrain slot 20
```

卷积使用确定性的 Hammersley 序列，默认每个像素取 64 个样本。Irradiance 保存法线半球上的入射光积分：

```text
E(N) = integral L(w) * max(dot(N, w), 0) dw
```

Shader 随后计算：

```text
Diffuse IBL = (1 - F) * (1 - metallic) * albedo * irradiance / PI
              * AO * SkyLightIntensity
```

这里保留 Fresnel 项，是为了让漫反射和镜面反射共享同一份能量分配。金属表面的漫反射自然趋近于零。没有有效 Irradiance 时，Shader 继续使用原有 Ambient Color 回退，场景不会因为环境资产失效而全黑。

### 缓存为何带 Runtime Version

卷积放在 CPU 上做，逐帧生成肯定不可接受。缓存键由 Cubemap Handle、Runtime Version、派生类型、Resolution 和 Sample Count 组成。同一键直接复用 GPU 纹理；Reload Cubemap 会推进 Version，只清理该源的旧版本。Diffuse 与 Specular 使用不同派生类型，改一边的参数不会误伤另一边。

```text
Handle + Runtime Version + DiffuseIrradiance + Resolution + Sample Count
```

这份缓存目前只在进程内存中，重启编辑器后仍需生成一次，也没有 LRU 预算。Scene YAML 只保留源 Handle 和光照强度，Irradiance 像素、GPU 对象及缓存统计都不序列化。EditorLayer 只负责把场景环境交给 Renderer，没有卷积算法或 OpenGL 调用。

常量环境回归确认积分结果为 `PI * Radiance`。这一阶段完成时，74 项无窗口测试通过；GTX 1050/OpenGL 4.6 上默认 `32x32 / 64 samples` 只生成一次，PBR Material Lab 的 6 个模型和 Terrain 均正常接收环境漫反射。后续 Specular Prefilter 与 BRDF LUT 已经接入同一条 Environment Lighting 管线。

## Specular Prefilter 粗糙度环境反射

漫反射解决了暗面，金属和光滑表面仍缺少环境反射。直接采样 Skybox 的普通 Mip 看似省事，但普通下采样没有遵守 GGX 分布，Roughness 增大时高亮的形状和能量都会失真。于是 P10 的下一步生成专用 Specular Prefilter Mip Chain，并配套 Split-Sum BRDF LUT。

### Prefilter 和 Roughness

`EnvironmentMapLoader` 使用 Hammersley 序列做 GGX 重要性采样。默认输出 `64x64 RGBA16F` Cubemap，共 7 层 Mip，每像素 64 个样本。Mip 0 直接保留源环境，后续 Mip 逐级提高 Roughness；材质越粗糙，Shader 选择的 LOD 越高，太阳一类的集中高亮也就扩散得更宽。

```text
SkyLight Cubemap
  -> EnvironmentLighting Specular 缓存
  -> GGX Prefilter，64x64，7 Mips
  -> PBRModel slot 9 / Terrain slot 21
  -> textureLod(reflection, roughness * maxLod)
```

Diffuse 和 Specular 只有在缓存缺失时才读回源 Cubemap。两者同时缺失时共用一次 CPU 浮点数据，然后分别生成；正常渲染帧只绑定已有纹理。Reload 或参数变化通过统一缓存键准确失效，旧 Runtime Version 的条目会被移除。

### BRDF LUT 的取舍

Prefilter 处理环境方向，二维 BRDF LUT 则预积分 `N dot V` 与 Roughness 对 GGX 镜面项的 scale/bias：

```text
Specular IBL = PrefilteredEnvironment(R, roughness)
             * (F0 * BRDF.x + BRDF.y)
             * AO * SkyLightIntensity
```

LUT 与具体 HDR 无关，所以 `EnvironmentLighting::Init()` 只生成一次线性 `RG16F Texture2D`，模型绑定 slot 10，Terrain 绑定 slot 22。切换 SkyLight 或 Reload Cubemap 都不会重算它；只有 LUT 设置变化时才重新生成。

初版使用 `128x128 / 256 samples`，GTX 1050 的 Debug 启动因此多等了约 5 秒。二维结果本身很平滑，又有双线性过滤，我最后把默认值降到 `64x64 / 128 samples`。肉眼没有发现明显差异，启动生成则缩短到相邻日志秒内。高质量参数入口仍然保留，之后若做离线缓存可以继续使用。

78 项无窗口回归覆盖 Prefilter 粗糙度响应、BRDF LUT 有限范围和掠射角 Fresnel。GTX 1050/OpenGL 4.6 实测中，Prefilter、Diffuse 和 BRDF LUT 都只记录一次生成；PBRModel、Terrain 与 PBR Material Lab 的 6 个模型正常渲染。

## Terrain 固定 3×3 Chunk、LOD 与剔除

Terrain 最初始终以整张网格提交。只要山地有一小角进入视野，全部三角形都会参与 Color 和 Shadow Pass；想加 LOD 时，也找不到比整块地形更细的切换单位。P11 先把地形固定拆成 `3x3` 九块。这个规模谈不上动态地形系统，但足够把共享网格、剔除、LOD 稳定性和接缝处理验证清楚。

### 九个区域，共用三份网格

`TerrainChunkLayout` 是纯 CPU 布局工具。它把整体世界尺寸和 Height UV 各分成三份：

```text
SharedMeshResolution = ceil(MeshResolution / 3)
ChunkWorldSize       = TerrainWorldSize / 3
ChunkUVScale         = (1/3, 1/3)
```

九个 Chunk 不复制 HeightMap、派生图或 TerrainMaterial，也不各建一份 Mesh。`TerrainRuntime` 只持有 LOD0、LOD1、LOD2 三个共享模板，分辨率约为完整 Chunk 的 `1`、`1/2`、`1/4`。绘制时上传每块的 UV Offset、UV Scale、Local Offset 和 Local Scale，同一份高度纹理便能覆盖正确区域。

Color Pass 为每块建立局部 AABB，再乘实体 Transform 与相机视锥测试。Shadow Pass 使用相同的 Chunk Bounds 对当前 Cascade 做剔除。判定很保守：只有八个包围盒角点全部落在同一 Clip Plane 外才丢弃，横跨视锥边缘的地块会继续提交。阴影目前固定使用 LOD0，避免投影轮廓随着相机距离改变。

### 让 LOD 切换少露破绽

Color Pass 按 Chunk 世界中心到相机的距离选择三级 LOD，默认阈值是 `90 / 180`。每块保存上一帧级别，跨过阈值外侧 5 个世界单位后才切换；这段迟滞能压住相机停在阈值附近时的来回跳动。初选结束后还会约束上下左右邻块，级别最多相差一级。

不同分辨率的边缘会产生 T-Junction。每份 `TerrainMesh` 因此复制四条边界顶点，并用 `a_Skirt` 标记让 Vertex Shader 向下拉出裙边。Skirt 只盖住侧面缝隙，表面仍采样原来的 Height 和 Normal；Color 与 Shadow 都使用它。

Debug Panel 会显示 Candidate、Submitted、Culled、三级 LOD 数量和提交三角形数。完整可见时 Candidate 固定为 9，Submitted 与 Culled 的和也应为 9。`Visualize Terrain LODs` 用红、绿、蓝标出 LOD0/1/2，环境变量 `GLIMMER_TERRAIN_LOD_VISUALIZE=1` 可用于固定相机检查，设置不会写入场景。

88 项无窗口回归覆盖九块完整覆盖、共享网格向上取整、视锥边界、距离阈值、迟滞和相邻级差。Intel Iris Xe/OpenGL 4.6 的固定截图确认三档区域连续，颜色交界处没有露出 Skybox 或 Clear Color；默认场景也没有出现 Chunk 误剔除。当前边界仍是固定 `3x3`、离散三级 LOD 和 Skirt，尚未实现动态 Chunk 层级或连续几何 Morph。

## Scene Depth 距离雾

P12 开始时，远近山体的颜色和对比度几乎一样，画面很难读出尺度。我先在 HDR Scene Pass 与显示映射之间加入距离雾。它直接使用 Scene Depth 重建世界位置，所以雾效跟真实世界距离走，不依赖 Terrain Shader，也能覆盖模型和其他写入深度的几何体。

### 从深度还原距离

Scene Framebuffer 保留 `RGBA16F` Color、整数 EntityID 和可采样的 `Depth24Stencil8`。后处理使用屏幕 UV、Depth 与当前相机的逆 ViewProjection 还原片元位置：

```glsl
vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
vec4 world = u_InverseViewProjection * clip;
world /= world.w;

float distanceToCamera = length(world.xyz - u_CameraPosition);
float rangeWeight = smoothstep(startDistance, endDistance, distanceToCamera);
float opticalWeight = 1.0 - exp(-density
    * max(distanceToCamera - startDistance, 0.0));
```

Depth 接近 1 的像素没有场景几何，Shader 会保留 Skybox 原色。这个判断很小，却避免了整片天空被当成远平面染成统一雾色。雾在曝光和 ACES 前混合，Scene Color 与 Fog Color 都处在线性 HDR 空间，最后只做一次 Gamma。

Settings 中可以调整 Density、Start、End 和 Color，默认值为 `0.012 / 60 / 260`。Fog Color 有 Manual、Sky Light 和 Directional Light 三种来源：Sky Light 按视线读取 Cubemap 的模糊 Mip，Directional Light 使用当前主光的 `Color * Intensity`；来源无效时回退 Manual。

### 高度雾为什么沿射线积分

只看片元终点高度会漏掉相机与物体之间经过的低空雾层。当前实现沿整条视线积分指数密度：

```glsl
float cameraDensity = exp(-falloff * (cameraY - baseHeight));
float denominator = falloff * (fragmentY - cameraY);
float heightIntegral = abs(denominator) > epsilon
    ? cameraDensity * (1.0 - exp(-denominator)) / denominator
    : cameraDensity;
```

俯视山谷时，穿过低空的路径会积累更多雾；高处山脊和相机附近仍能保留细节。相机越过 Base Height 时公式保持连续，指数输入与积分结果也有钳制，调试参数拉得很极端时不会制造 Inf 或 NaN。

这些参数目前属于 `PostProcessSettings` 的运行时状态，不写 Scene YAML。`GLIMMER_DISTANCE_FOG_VISUALIZE=1` 可以用固定设置启动检查。P12 验证时，Intel Iris Xe/OpenGL 4.6 的固定相机画面确认近景对比度、远景衰减和 SkyLight 雾色都符合预期；88 项无窗口回归通过。当前效果仍是解析距离雾和高度雾，没有体积阴影、光柱或完整大气散射。

## HDR Bloom 后处理

Tone Mapping 接入 HDR 后，太阳和 Emissive 高亮虽然有足够亮度，缩到显示范围时却显得很硬。Bloom 用周围像素的光晕把这种亮度重新表现出来。首版直接复用 Scene HDR Color，没有给材质或光源增加额外的 Bloom 标记。

### 一条尽量简单的 Bloom 链

```text
Scene RGBA16F
  -> 按 EV 判断 Threshold / Soft Knee
  -> Half-resolution RGBA16F
  -> 水平与垂直 Gaussian Blur
  -> 按 Intensity 加回 Scene HDR
  -> Distance / Height Fog
  -> 2^EV -> ACES White Point -> Gamma
```

提取时先用当前 EV 计算显示亮度，再判断 Threshold。用户调曝光后，肉眼看到的 Bloom 起点因此不会乱跑；写入模糊缓冲的仍是未曝光 Radiance，合成结果最后统一乘一次 `2^EV`。Soft Knee 在阈值附近提供渐进权重，省掉高光边缘那圈明显的硬切。

模糊缓冲使用视口一半的宽高和 `RGBA16F`，两张 Framebuffer 交替执行横向、纵向高斯采样。默认 Threshold `1.0`、Soft Knee `0.5`、Intensity `0.08`、Blur Passes `6`。关闭 Bloom 会跳过提取与模糊 Pass；Resize 时两张缓冲跟随 Display Framebuffer 调整，最小尺寸保持为 `1x1`。

我当时刻意把强度压得很低。默认画面只让太阳一类 HDR 高光轻微扩散，Terrain 中间调不会整体泛白。Bloom 在雾之前加回线性颜色，远处光晕随后和场景一起受 Fog 衰减，不会像贴在屏幕上一样穿过雾层。

Intel Iris Xe/OpenGL 4.6 验证覆盖 BloomExtract、BloomBlur 与 ToneMapping Shader，固定 Terrain 画面没有出现整屏泛白或雾层穿透；88 项无窗口回归和 VS2026 `Debug | x64` 构建通过。当前仍是经典半分辨率双缓冲高斯 Bloom，尚无 Mip Pyramid、Kawase、Lens Dirt 或自动曝光。

## TAA 接入评估

P12 期间我评估过 Temporal Anti-Aliasing，最后决定先不接。原因并不玄乎：当时管线只有当前帧 HDR、Depth 和 EntityID，缺少可靠的像素运动数据。若直接把上一帧颜色按固定 Alpha 混回来，静止截图可能更平滑，编辑器一动就会留下残影。

当前源码仍缺少这些前置条件：

- 相机投影没有 Halton 一类的亚像素 Jitter，也不保存 Previous ViewProjection；
- Scene FBO 没有 RG16F Velocity Attachment；
- Model、GPU Instancing 和 Sprite 没有 Previous Transform 数据；
- 后处理没有 HDR History Ping-Pong，也没有 Resize、Play/Stop、场景切换和相机跳变时的失效规则；
- Shader 没有 Depth Disocclusion、Neighborhood Clamp 和 Transparent/Emissive Reactive Mask。

仅靠当前 Depth 重建世界位置，再投影到上一帧，只能照顾静态几何与相机运动。移动模型、实例、Sprite、Blend 物体和编辑器 Gizmo 都缺少自身速度，历史颜色会拖在后面。这样的结果不适合成为编辑器默认抗锯齿。

以后正式接入时，最低顺序应是：

```text
Projection Jitter
  -> Current / Previous Clip Position
  -> RG16F Velocity Attachment
  -> HDR History Ping-Pong
  -> Depth Disocclusion + Neighborhood Clamp
  -> Transparent / Emissive Reactive Mask
  -> Resize、场景和相机状态变化时清空 History
```

TAA 只应处理 HDR Scene Color，并放在 Bloom 提取之前。EntityID 继续读取当前帧整数附件，不能参与历史混合，否则鼠标拾取会和画面产生一帧或多帧错位。到目前为止这项功能仍未实现，这一章记录的是接入条件和暂缓理由。

## 后处理渲染器职责收拢

雾和 Bloom 最初直接写在 `EditorLayer` 里。功能能跑，但编辑器已经同时管着 Scene、相机、资产和面板，再把 Framebuffer 分配、Shader 参数和 Pass 顺序留在那里，后面每加一种效果都要继续膨胀。P12.1 因此把整条显示链迁到引擎侧 `PostProcessRenderer`。

```text
EditorLayer
  -> 渲染 Scene HDR / EntityID / Depth
  -> 收集 Camera、SkyLight、DirectionalLight 输入
  -> PostProcessRenderer::Execute(input)
       -> Bloom Extract
       -> Half-resolution Ping-Pong Blur
       -> Scene + Bloom
       -> Fog -> EV -> ACES -> Gamma
       -> Display Texture
```

`EditorLayer` 现在只准备 `PostProcessInput`，其中包含 Scene Color/Depth、Inverse ViewProjection、Camera Position 和雾色所需的光源引用。`PostProcessRenderer` 自己持有 Display Framebuffer、两张 Bloom Framebuffer 和三张后处理 Shader，并在 Viewport Resize 时调整这些资源。

`PostProcessSettings` 集中保存 Bloom、距离/高度雾、曝光、ACES White Point 和灰度开关。Settings 面板仍直接编辑这份运行时状态，操作方式和画面公式都没有改变；参数不会进入 Scene YAML。Shader 继续注册到编辑器共用的 `ShaderLibrary`，原来的自动与手动热重载也还有效。

这次重构的判断标准很朴素：编辑器决定何时执行后处理，引擎对象负责怎样执行。这样水文和气候调试继续增加时，`EditorLayer` 只需传输入，不会重新卷入画面算法。P12.1 完成时通过 VS2026 `Debug | x64` 构建、88 项无窗口回归和 15 秒编辑器启动检查，Viewport 输出与 EntityID 拾取没有变化。

## 固定步长水文 CPU 参考模型

P13A 没有一上来就写 Compute Shader。我先做了不需要窗口和 GPU 的 `TerrainHydrologyRuntime`，用小网格把水量、流向和时间步语义定死。CPU 版本更慢，却容易逐格断言，也能在出现质量误差时直接检查中间状态。

### 水流核心

每格保存 Height、Water、四向 Flux 和二维 Velocity。P13A 阶段 Height 是初始化快照，水文只更新其余字段：

```text
旧 Height + Water + Flux
  -> 比较四邻域水面高度
  -> 更新 Left / Right / Down / Up Flux
  -> 按当前可用水量限制总出流
  -> 汇总邻居入流和本格出流
  -> 新 Water + Velocity
```

首版使用封闭边界，水不会从网格四周流失。Rainfall 以每秒水深加入单元，并计入总水量预算。如果某格的计划出流超过当前步可提供的体积，四个方向会按同一比例缩小；这种限幅让 Water 保持非负，也保留原来的流向比例。

后来 P13B/P13C 在同一参考模型中加入 Sediment、Capacity/Saturation 和 Erosion/Deposition，Height 也因此成为可变运行时状态。基础水流顺序没有换掉，新增阶段在它后面执行；更完整的质量交换由后续章节单独记录。

### 固定步语义

`Advance(frameDelta)` 只在 Play 状态累积帧时间，并按 `FixedTimeStep` 执行零到多个子步。Pause 不消费时间；Single Step 只允许在暂停时推进一次。Reset 会恢复初始 Height、Water 和 Sediment，清空 Flux、Velocity、累加器与统计。

`MaxSubsteps` 限制单帧追赶次数。卡顿留下的完整步时间会计入 `DroppedTime`，只保留不足一个固定步的余数。宁可明确丢掉过期模拟时间，也不能在下一帧无上限补算，把编辑器拖进持续卡顿。

统计会记录 Step Count、Simulated Time、Accumulator、Dropped Time、水量与质量误差、深度范围、最大速度和有限性。当前扩展版还包含泥沙与地形质量数据。

CPU 基线用 `0.04 x 25` 和 `0.01 x 100` 两种帧划分运行同一秒，最终 Water 与 Velocity 一致。封闭边界无降雨时质量误差低于 `1e-5`，三格山峰和盆地用例也确认水会流向低处。P13A 当时新增 8 条水文测试，总计 96 项无窗口断言通过。

## GPU 水文与 Debug 可视化

CPU 契约稳定后，P13A 才把相同字段迁到 GPU。程序化 Terrain 提供 `R32F` Height，Water、Flux 和 Velocity 各自拥有读写纹理，单步的前半段算流量，后半段更新水深与速度：

| 字段 | 初始格式 | 用途 |
| --- | --- | --- |
| Water | `R32F x 2` | 当前与下一步水深 |
| Flux | `RGBA16F x 2` | 左、右、下、上四向流量 |
| Velocity | `RGBA16F x 2` | XY 平面速度，ZW 预留 |

```text
Height(Read) + Water(Read) + Flux(Read)
  -> HydrologyFlux.comp -> Flux(Write)
  -> Global Memory Barrier + Swap
  -> HydrologyUpdate.comp -> Water(Write) + Velocity(Write)
  -> Global Memory Barrier + Swap
```

两次 Dispatch 之间必须使用全局 Memory Barrier。Workgroup Barrier 只保证单个线程组内部同步，无法说明相邻组已经写完纹理，这正是早期地貌原型里最不可靠的部分。普通 PNG/JPG 高度图也不能直接作为这条路径要求的 `R32F` Storage Texture，所以 GPU Runtime 只为程序化 Terrain 创建。

### 当前运行方式

`TerrainRuntime` 按 GenerationVersion 创建水文资源，地形重新生成后旧模拟会被替换。P13A 最初由 Hydrology 自己推进固定步；P14 接入气候后，`TerrainEnvironmentGPU` 统一拥有累加器，每个子步按 Climate、Barrier、Hydrology 的顺序执行。Hydrology 和 Climate 的 Play/Single Step 都进入这一个时钟。

`TerrainRenderer` 使用 FrameSerial 保证同一 Color Frame 只推进一次。Shadow Prepare 和九个 Chunk 的重复访问只读取结果，不会悄悄多跑模拟。普通帧也不做 GPU Readback；统计读取与 Contract 验证都需要显式请求。

Debug Panel 可以 Play、Pause、Single Step、Reset，也能调整 Rainfall 并查看 Water 诊断。当前面板还扩展到 Sediment、Capacity、Saturation、运行时侵蚀和气候 Source/Sink。水深着色只是 Terrain Fragment Shader 的诊断覆盖，不会抬高网格，也没有折射、反射、透明水面或岸线泡沫。

P13A 的受控 GPU Contract 使用 `3x1` 高低高盆地，分别以 `0.04 x 25` 和 `0.01 x 100` 运行 100 个固定步。GTX 1050/OpenGL 4.6 得到相对质量误差 `7.38228e-7`、中央水深 `0.599998`、两侧最大值 `6.28643e-7`，两种帧划分最大差值为 0。`GLIMMER_HYDROLOGY_VALIDATE=1` 可在启动时运行同一验证；正常帧不会创建这些临时资源。

## Tone Mapping 跨驱动 Sampler 修复

这次问题很典型：同一份代码在一台电脑上正常，换到 GTX 1050 / NVIDIA 531.29 后，Viewport 只剩黑屏。我起初沿着 Scene FBO、Terrain 和 Bloom 的输出往后查，RenderDoc 却显示这些阶段都已经提交，真正失败的是 Tone Mapping 最后一次 `DrawElements(6)`：

```text
GL_INVALID_OPERATION: State(s) are invalid: program texture usage.
```

问题出在 sampler 槽位。`ToneMapping.glsl` 同时声明了 `sampler2D u_SceneTexture` 和 `samplerCube u_FogSkyLight`。旧代码只在 Fog Color Source 选择 Sky Light 时把 Cubemap 设置到 slot 2；使用默认 Manual 模式时，`u_FogSkyLight` 会保留 GLSL 默认值 0，恰好与 Scene Texture 冲突。

这里容易被分支条件带偏。OpenGL 校验的是整个已链接 Program 的纹理类型，不会因为本帧没有执行 Cubemap 采样分支就忽略这个 sampler。宽松驱动可能暂时放过，严格驱动会直接拒绝 Draw。

现在 `PostProcessRenderer` 每帧都会声明完整的绑定契约：

| Slot | Sampler | 类型 |
| ---: | --- | --- |
| 0 | `u_SceneTexture` | `sampler2D` |
| 1 | `u_SceneDepth` | `sampler2D` |
| 2 | `u_FogSkyLight` | `samplerCube` |
| 3 | `u_BloomTexture` | `sampler2D` |

Fog 或 Bloom 关闭时，可以不绑定对应的可选纹理，但 uniform 仍要落在互不冲突的槽位。修复后的资源布局不再依赖 sampler 默认值、上一帧状态或具体驱动的容错行为。这次排查也提醒我，Shader 资源契约要按 Program 看，不能只看当前会走到哪条分支。

验证时，VS2026 `Debug | x64` 编辑器目标增量构建通过；GTX 1050 在默认 Manual Fog 下恢复 Terrain 和 Skybox。修复后的 RenderDoc API Validation 能捕获最终 Tone Mapping Draw，且没有 High severity、`GL_INVALID_OPERATION` 或 `program texture usage` 报错。

## 模型导入边界与 Assimp 子模块准备

准备接入 FBX 时，我先处理了旧模型系统的职责混杂。原来的 `Model.cpp` 同时负责 OBJ 解析、切线计算、MTL 读取和 GPU Mesh 创建。继续把 Assimp 塞进这里，节点、材质乃至后续骨骼数据都会顺着 `Model` 渗入 Renderer，后面很难拆开。

因此这一阶段先建立统一的 CPU 中间层：

```text
模型源文件
  → ModelImporter（按扩展名分发）
  → ObjModelImporter / AssimpModelImporter
  → MeshSource（纯 CPU 数据）
  → Model
  → Mesh / VAO / VBO / IBO
  → Renderer3D
```

`MeshSource`、`SubmeshSource`、`MeshVertex` 和 `MeshMaterialSource` 只描述导入结果，不创建 Texture、Buffer 或 OpenGL 对象。OBJ 解析被移入 `ObjModelImporter`；Assimp 的 `aiScene`、`aiMesh` 等类型则留在 `AssimpModelImporter` 的私有实现里。这样，导入器只需要把不同文件格式翻译成同一种数据，渲染端不必知道源文件是 OBJ 还是 FBX。

Assimp 采用官方 Git 子模块，固定在 `v6.0.5` 的提交 `392a658f9c271be965271f45e7521a1b80ea4392`。它没有被拆成源文件塞进 Premake 工程，而是由上游 CMake 生成静态库。构建使用 VS2026 x64 Developer Environment、NMake 和静态 CRT，并关闭 Exporter、Tests、Tools、Samples、Docs，只保留 OBJ、FBX、GLTF importer。选择 NMake 是一次实际的兼容性取舍：本机 CMake 4.3.3 使用 `Visual Studio 18 2026` Generator 时会卡在 `CompilerIdC.vcxproj`，同一套编译器在 Developer Environment 中可以稳定完成探测和构建。

新设备只需先初始化子模块：

```bat
git submodule update --init --recursive
```

当前 Premake PreBuild 会调用 Ensure 脚本，检查头文件、Assimp/zlib 静态库、CMake Cache、构建配置、子模块提交和 Schema 2 stamp；缺失或过期时会自动重建。`scripts\Win-BuildAssimp-vs2026.bat` 仍保留给强制重建和单独排障使用，产物按 Debug/Release 放在忽略的 `Glimmer/vendor/assimp-build/vs2026-<Config>` 目录。

这一步建立边界时，FBX importer 还没有落地；现在它已经由下一章实现。当前 AssetManager 对外注册 `.obj` 和 `.fbx`，构建进 Assimp 的 `.gltf/.glb` 尚未开放，版本化 `.glmesh` 也还没有。模型每次加载仍会解析源文件，导入纹理也还是 Model 持有的运行时对象，没有转换成 AssetHandle 或自动生成 `.glmat`。

阶段验证包括 Assimp Debug 静态库完整构建、重复执行脚本约 5 秒且没有重编源文件、3 条 OBJ 到 MeshSource 的无窗口回归，以及当时共 100 项断言通过。编辑器 `Debug | x64` 目标也完成构建。后续功能继续扩展，但 Assimp 私有类型没有越过 importer 边界。

## 静态 FBX 与 Cerberus PBR 材质加载

边界稳定后，FBX 才真正接入运行时。AssetManager 现在会把 `.fbx` 识别为 Model，交给 `AssimpModelImporter` 转换；OBJ 仍走 `ObjModelImporter`。两条路径从 `MeshSource` 开始合流：

```text
FBX + 外部纹理
  → AssimpModelImporter
  → MeshSource / SubmeshSource / MeshMaterialSource
  → Model（按 MaterialIndex 缓存纹理）
  → Mesh
  → Renderer3D / PBRModel
```

静态导入启用了 Triangulate、JoinIdenticalVertices、GenSmoothNormals、CalcTangentSpace、ImproveCacheLocality、SortByPType、ValidateDataStructure 和 PreTransformVertices。`PreTransformVertices` 会把节点变换烘焙进顶点，省去了当前渲染路径处理层级的负担，代价也很明确：节点层级、骨骼和动画不会保留下来。这条路径只面向静态模型。

材质导入会读取 BaseColor、Normal、Metallic、Roughness、AO 和 Emissive。Cerberus FBX 自身只引用 `Textures/Cerberus_A.tga`，其余通道需要从外部文件补齐。导入器只在模型目录、`Textures` 和 `Textures/Raw` 中检查 `_N/_Normal`、`_M/_Metallic`、`_R/_Roughness`、`_AO/_Occlusion` 等受控后缀，不会递归扫项目，也不会用模糊名称碰运气。

当前版本化样本最终使用四张贴图：

```text
Cerberus_A.tga       Base Color / sRGB
Cerberus_N.tga       Normal / Linear
Cerberus_M.tga       Metallic / Linear
Cerberus_R.tga       Roughness / Linear
```

样本没有 AO，因此示例场景不会再拿 Base Color 冒充 AO。`Model` 按 MaterialIndex 缓存解码后的纹理，同一材质被多个 Submesh 引用时只上传一套。Renderer3D 中，MaterialInstance 显式提供的 BaseColor、Normal、AO、Emissive 优先，缺失通道回退到模型导入结果；导入的 Metallic 和 Roughness 使用 unit 11/12，避开 0～3 的材质、4～7 的 CSM 与 8～10 的 IBL 槽位，并覆盖 `.glmat` 中对应的标量值。

实际使用时，把 FBX 和它的相对纹理目录一起放进 `GlimmerEditor-CyouBranch/assets`，再为实体设置 Model Renderer 和 Material。`.glmat` 提供 PBRModel Shader 与可编辑覆盖值，FBX 补足没有显式配置的导入纹理。Content Browser 写入的是 FBX AssetHandle，Scene YAML 仍只保存 ModelHandle 和 MaterialHandle，不会序列化 Assimp 对象或 GPU ID。

当前限制需要保留在文档里：只开放静态 `.fbx`；`.gltf/.glb` 尚未注册；节点层级、骨骼、动画、Morph Target 和嵌入纹理暂不支持；DCC 单位不会自动换算；Tangent 仍是 vec3，没有镜像 UV 所需的 handedness。导入纹理也没有独立 AssetHandle，`.glmat` 暂无 Metallic/Roughness Texture 字段，ORM 打包和版本化 `.glmesh` 都还未实现。

Cerberus 的 FBX 与 A/N/M/R 四张 TGA 作为跨设备回归样本保存在仓库中，但原许可说明文件尚未补齐。公开分发或商业使用前需要取得可再分发许可；无法确认时，应从发布资产中移除。

后续跨设备收口验证中，Cerberus 能生成有效三角 Submesh，顶点、索引和切线检查通过，四张 TGA 均能从正式 assets 解析；104 项无窗口断言通过，Windows 验证脚本通过，编辑器 `Debug | x64` 成功链接 Assimp 和 zlib，并持续运行 10 秒，没有 Shader 断言或提前退出。

## Assimp 新设备构建自修复

这次故障出现在一台刚拉完仓库的新设备上。子模块已经初始化，Premake 工程也能生成，但编译 Glimmer 时停在：

```text
fatal error C1083: 无法打开包括文件: "assimp/config.h": No such file or directory
```

起初看起来像 Include Path 写错了，实际缺的是 Assimp 的生成文件。源码子模块只有 `include/assimp/config.h.in`，真正的 `config.h` 要由上游 CMake 根据平台、配置和 importer 选项生成。仅把仓库和子模块拉下来，Assimp 仍不具备可链接状态。

我不想让每台设备都靠人工记住一串前置命令，于是把依赖检查放进 Glimmer 的 PreBuildEvent：

```text
构建 Glimmer
  → Win-EnsureAssimp-vs2026.bat <Configuration>
  → 检查 config.h、Assimp/zlib 静态库和 CMakeCache
  → 核对 Schema 2 stamp、构建配置、子模块提交和 ccache 状态
  → 状态有效：直接继续
  → 缺失或过期：调用 Win-BuildAssimp-vs2026.bat
  → 完成后再编译 Glimmer
```

Debug 使用 Debug 静态库，Release 和 Dist 使用 Release 产物。构建脚本通过 `vswhere` 查找 VS 18.x/2026，再确认 v145 工具集，路径不依赖 Visual Studio 安装在哪个盘。CMake 优先使用 VS 附带版本，并通过 NMake 生成静态 CRT 库。

这里还踩过一个不太显眼的坑：PATH 中的 MinGW `ccache.exe` 会包裹 MSVC `lib.exe`，CMake 日志看似成功，最终却没有生成可用的 `.lib`。脚本现在显式设置 `ASSIMP_BUILD_USE_CCACHE=OFF`，并把这个状态写入 `glimmer-assimp-build.stamp`。Ensure 会同时检查 stamp 和 `CMakeCache.txt`，旧缓存、配置切换或 Assimp 提交变化都会触发重配。

新设备的常规流程现在只有两步：

```bat
git submodule update --init --recursive
scripts\Win-GenerateProject-vs2026.bat
```

之后正常构建解决方案或单独构建 `Glimmer.vcxproj` 都会走同一条 Ensure 路径。需要完整回归时运行 `scripts\Verify-Windows.bat`；需要强制重配依赖时，再单独执行 `scripts\Win-BuildAssimp-vs2026.bat Debug` 或 `Release`。

验证使用了一台仍保留 `ASSIMP_BUILD_USE_CCACHE=ON` 旧缓存的设备。首次构建能识别过期状态并原地重配，之后 Ensure 快速命中约 118 ms。重新生成 VS2026 工程后，测试工程可脱离解决方案单独构建；定向 Rebuild 的 104 项断言和完整 Windows 验证均通过，编辑器持续运行 10 秒，没有提前退出。

## CPU 无源泥沙输运

P13A 已经让 Water 和 Flux 在固定时间步内稳定运行，P13B 的第一步是给水流增加悬浮泥沙。我先把 Sediment 做成独立数组，单位是每格单位地表面积上的悬浮质量。它不借用 Water 的颜色通道，也不等同于 Terrain Height；初始化快照、Reset 和统计各自维护。

这一阶段只研究输运。每个固定步在水流 Flux 求解完成后执行有限体积更新：

```text
旧 Sediment × CellArea
  → 本格悬浮泥沙质量
÷ ((旧 Water + RainfallDepth) × CellArea)
  → 泥沙浓度
× 四向 Water Flux
  → 四向泥沙质量流率
  → 按本格现有质量限制总外运
  → 邻格入流 - 本格出流
  → 新 Sediment
```

这里有两个容易混在一起的量。雨水会增加水深，所以浓度会被稀释，但雨水本身不会凭空生成泥沙；Flux 表示水的体积流率，乘上浓度后才得到泥沙质量流率。每格的总外运还要按当前可用质量限幅，否则一个较大的固定步就可能把 Sediment 算成负数。

边界沿用水文模型的封闭条件，因此 `SedimentBoundaryLoss` 在这个阶段为 0。统计使用 `当前质量 + 边界损失 - 初始质量` 计算 `SedimentMassError`，并逐格检查非负和有限。

我当时有意没有加入携沙能力、侵蚀和沉积。那些过程会在 Height 与 Sediment 之间交换质量，必须先定义单位换算、源项限幅和组合质量预算。先把无源输运单独验证，出了误差时就只需要检查通量与边界。当前代码已经在后续 P13C 接上侵蚀和沉积；两项速率保持默认 0 时，仍会回到这一章定义的无源行为。

这轮验证覆盖下游迁移、Water/Sediment Reset、封闭边界守恒、非负与有限检查、两种帧划分确定性，以及 Height 逐值不变。新增 3 项后，当时共 107 项无窗口断言通过，VS2026 `Debug | x64` 编辑器增量构建成功。

## GPU 泥沙输运与诊断显示

CPU 基线通过后，我把同一套输运规则迁到 `TerrainHydrologyGPU`。Sediment 使用两张独立的 `R32F` 纹理做 Ping-Pong，没有塞进 Water，也没有复用 Terrain 的 Normal、Slope 等派生图。在 P13B 这个阶段，一个固定步由三段 Compute 组成：

```text
Height + 旧 Water + 旧 Flux
  → HydrologyFlux → 当前 Flux
旧 Water + 当前 Flux
  → HydrologyUpdate → 新 Water + 新 Velocity
旧 Water + 当前 Flux + 旧 Sediment
  → SedimentTransport → 新 Sediment
  → Barrier → Water / Velocity / Sediment Swap
```

`SedimentTransport.comp` 对本格和四个邻格调用同一套只读流率计算：用旧 Sediment 除以旧 Water 与本步 Rainfall 的总水量得到浓度，再乘当前 Water Flux。总外运仍受本格现有泥沙限制。每个 Invocation 只写自己的 Next Sediment，不需要原子加法，也不会读取另一个 Workgroup 尚未完成的结果。

这段设计最看重的是资源所有权。Current 只读，Next 只写，Dispatch 结束后统一 Barrier 和 Swap。输运 Pass 不能碰 Height、Water 或有限次 Authoring Erosion。后续 P13B 加入 Capacity/Saturation，P13C 又加入 Height/Sediment 交换，但无源输运仍保留在管线前半段，没有另写一套算法。

调试入口位于 `Debug → Overview → Runtime Hydrology`。把 `Visualization` 切到 `Suspended Sediment`，设置 `Sediment Seed` 后点击 `Apply Seed`，再用 `Play` 或 `Single Step` 推进。Seed 只写一次初始状态，不会每帧补充泥沙。暂停后可用 `Validate / Readback` 检查质量误差和数值范围，`Run GPU Contract` 会运行受控盆地验证。

Terrain Shader 通过 slot 25 读取 Sediment；同一组诊断中的 Water 和 Velocity 使用 slot 23/24，后续 Capacity/Saturation 使用 26/27。棕橙色覆盖只是数值显示，不会修改地形材质。水文诊断模式互斥，避免多层颜色混在一起。`SedimentTransport.comp` 也加入事务式热重载，编译失败时继续保留上一份有效程序。

GTX 1050 / OpenGL 4.6 上，P13B 当时的三个水文 Compute Shader 和 Terrain Shader 均编译通过。受控 GPU Contract 得到泥沙相对质量误差 `0`，源格从 `1` 降至 `0`，下游格从 `0` 增至 `1`，两种帧划分最大差值为 `0`；已有水量相对误差仍为 `7.38228e-7`。编辑器增量构建和当时的 107 项无窗口断言也全部通过。

## 泥沙携沙能力与饱和度诊断

无源输运能回答泥沙去了哪里，却无法判断水流还带得动多少泥沙。P13B 收尾时，我给 CPU 和 GPU 水文状态补了两个只读派生场：

```text
Capacity = CapacityScale × WaterDepth × Speed
Saturation = Sediment / Capacity
```

Capacity 是每单位地表面积的当前携沙能力。Saturation 小于 1 时，水流仍有余量；接近 1 时处在平衡附近；大于 1 时，现有泥沙超过容量。这个模型很简化，但它先给后续侵蚀和沉积提供了一条能验证的分界线。

干格需要单独处理。Capacity 小于 `1e-6` 且格内仍有 Sediment 时，Saturation 记为 `1000`；若连 Sediment 也没有，则为 0。`1000` 只是一个有限的诊断上限，方便 Shader 着色和统计，不能拿它当侵蚀速率。

CPU `TerrainHydrologyRuntime` 会在 Water、Velocity 和 Sediment 更新后重算两个数组，并把范围与有限性写入统计。调整 Capacity Scale 会立即刷新派生场，不会推进固定步，也不会修改 Water、Sediment 或 Height。

GPU 侧由 `SedimentCapacity.comp` 读取完成 Swap 后的 Water、Velocity 和 Sediment，写入单缓冲 `R32F` Capacity 与 Saturation。它们没有下一步历史依赖，用 Ping-Pong 只会增加所有权负担，所以这里保留单张纹理。P13B 时这两个场只用于观察；下一章的 P13C 才开始读取 Capacity，执行 Height 与 Sediment 的质量交换。

调试纹理槽位按一组连续编号保留：

| Slot | 诊断场 |
| ---: | --- |
| 23 | Water Depth |
| 24 | Water Velocity |
| 25 | Suspended Sediment |
| 26 | Sediment Capacity |
| 27 | Sediment Saturation |

在 `Debug → Overview → Runtime Hydrology` 中可以调整 `Capacity Scale`，再选择 `Sediment Capacity` 或 `Sediment Saturation`。Capacity 使用绿青色显示高承载区；Saturation 用蓝色、浅色和红色区分欠饱和、接近平衡与过饱和。`Validate / Readback` 会同步读取 Min/Max，`Run GPU Contract` 还会检查有限性和帧划分一致性。

这一阶段新增测试后共有 109 项无窗口断言通过。GTX 1050 / OpenGL 4.6 上，四个水文 Compute Shader 和 Terrain Shader 编译成功；受控 GPU Contract 得到水量相对误差 `7.38228e-7`、泥沙质量误差 `0`、`capacityMax=0.0411786`、`saturationMax=1000`，Water、Sediment、Capacity 和 Saturation 的两种帧划分差值均为 `0`。

## CPU 运行时侵蚀与沉积质量契约

P13C 先在 CPU 参考模型里接通 Height 与 Sediment 的交换。我没有马上改 GPU Height Texture，因为这里最容易出错的地方是单位。Height 是长度，Sediment 是单位面积悬浮质量，直接做加减会让数值看似变化，质量预算却没有意义。

为此，规格中加入 `TerrainDensity`，把地形高度换成单位面积等效质量：

```text
TerrainMassPerArea = Height × TerrainDensity
CombinedMass = Σ((Height × TerrainDensity + Sediment) × CellArea)
```

每个固定步先完成水和泥沙输运，再计算 Capacity/Saturation，然后处理源项：

```text
欠饱和：Height → Sediment
过饱和：Sediment → Height
交换完成后重新计算 Capacity / Saturation
```

欠饱和时，候选侵蚀量是 `(Capacity - Sediment) × ErosionRate × dt`；过饱和时，候选沉积量是 `(Sediment - Capacity) × DepositionRate × dt`。公式只是起点，实际交换还要经过几道硬限制：

- 单步 Height 绝对变化不超过 `MaximumHeightChangePerStep`；
- Height 不低于 `初始 Height - MaximumErosionDepth`；
- 沉积不能消耗超过本格现有的 Sediment；
- 输入参数和结果必须有限，Sediment 不能变成负数。

`ErosionRate` 和 `DepositionRate` 默认都是 0。旧场景不会因为代码升级就开始改地形，P13B 的无源输运也能原样复现。`Reset` 会恢复初始化时的 Height、Water 和 Sediment，并清空侵蚀、沉积统计。

这里保留了两套误差指标。`SedimentMassError` 只看悬浮泥沙相对初态的变化，启用侵蚀后出现非零值很正常；判断 Height 与 Sediment 的局部交换是否守恒，要看 `TerrainSedimentMassError`。统计还记录地形初始/当前质量、侵蚀与沉积量、Height 范围和本步最大高度变化，排查限幅时不用猜。

CPU 回归覆盖欠饱和侵蚀、过饱和沉积、可侵蚀层下界、单步限幅、Reset 和帧划分确定性。新增 5 项后，当时共 114 项无窗口断言通过，VS2026 `Debug | x64` 编辑器重新链接成功。这套预算随后原样迁到 GPU。

## GPU 运行时侵蚀与沉积

GPU 阶段给 `TerrainHydrologyGPU` 增加了独占的 Runtime Height Ping-Pong。TerrainGenerator 产出的 Height 保持为不可变初始快照，同时充当最大侵蚀深度的参照。模拟只改运行时纹理，不会回写生成器，也不会触碰有限次 Authoring Erosion。

一个固定步的主体顺序是：

```text
Flux → Water/Velocity + Sediment Transport
     → Capacity/Saturation
     → ErosionDeposition（写 Next Height 与 Next Sediment）
     → Barrier → Height/Sediment Swap
     → 重算 Capacity/Saturation
```

`ErosionDeposition.comp` 同时读取 Initial Height、Current Height、Sediment 和 Capacity，输出 Next Height 与 Next Sediment。它不会在同一张纹理上边读边写。Dispatch 完成后统一 Barrier，再交换两组资源，Color 和 Shadow Pass 因而只能看到一份完整的 Runtime Height。后来 P14 把气候 Water Source/Sink 接入水文前段，没有改变这条 Height/Sediment 所有权。

Debug 面板提供 `Erosion Rate`、`Deposition Rate`、`Terrain Density`、`Max Erosion Depth` 和 `Max Height Step`。前两项默认关闭。需要观察时，先启用水文并设置速率，再用 `Single Step` 或 `Play` 推进；`Readback` 会显示 Height Min/Max、Net Eroded/Deposited Mass 和 Terrain+Sediment Combined Error。

这里的 Eroded/Deposited 是当前 Height 相对初始快照的净变化，不是历次交换量的累计吞吐。`Reset` 会用缓存的初始数据恢复两张 Height，并清空 Water、Flux、Velocity、Sediment 和诊断场。

持久化边界保持得很严。Runtime Height 不进入 Scene YAML，复制 Terrain 不携带 GPU 模拟纹理，生成器版本变化会重建整套水文 Runtime，也没有自动 Bake。初始化时会同步读回一次生成 Height，用来建立可恢复快照；普通帧和 Reset 不做 GPU Readback，只有用户显式点击 `Readback` 才同步取统计。

地形表面也要跟着 Runtime Height 更新，否则几何已经出现沟槽，法线和材质分层却还停在旧地形上。`TerrainGenerator::DeriveMapsFromHeight` 复用 `DeriveTerrainMaps.comp`，允许输入最终 Runtime Height。`TerrainRenderer` 会先跑完本帧全部固定子步，再至多刷新一次 Normal/Slope、Analysis 和 MaterialWeight。即使某帧追赶 4 个子步，也不会重复派生 4 次；没有步进或侵蚀/沉积关闭时，不增加这项开销。

现在 Color、Shadow、坡度、曲率/流势和 Grass/Soil/Rock/Snow 权重使用同一版本的 Height。低角度观察沟槽时，轮廓、岩石坡面和土壤/草地边界会一起变化，Reset 后也会一起恢复。`GLIMMER_TERRAIN_VALIDATE=1` 会实际调用 Runtime Height 派生入口，检查有限性、权重归一化，并比较它与同一 Height 生成路径的输出哈希。

GTX 1050 / OpenGL 4.6 上，五个水文 Compute Shader 和 Terrain Shader 编译通过。受控 GPU Contract 运行 100 个固定步，组合质量误差为 `2.58287e-7`，侵蚀高度 `0.02`，沉积高度 `0.1`；`0.04 × 25` 与 `0.01 × 100` 两种帧划分的 Height/Sediment 最大差值均为 `0`，Reset 检查通过。Windows 增量验证和 114 项无窗口断言也全部通过。

这套模拟仍是临时运行时状态。若要保存侵蚀结果，需要另行设计显式 Terrain Asset Bake、写入失败回滚和 Undo/Redo，不能悄悄改变现有 Reset、复制与场景保存语义。

## CPU 简化气候与植被潜力基线

P14 要做的是一个能和地形、水文继续衔接的简化气候层。我没有直接从 Compute Shader 开始，因为气候场一多，单位和水量来源很容易混乱。第一步先在 `Glimmer/Simulation` 中实现纯 CPU 的 `TerrainClimateRuntime`，用小网格把更新顺序和预算跑通。

首版只保留六个场：

| 场 | 单位与含义 |
| --- | --- |
| `TerrainHeight` | 米，气候步内只读 |
| `Temperature` | 摄氏度 |
| `AtmosphericMoisture` | 二维空气柱中的米水当量 |
| `SurfaceWater` | 地表米水深 |
| `Rainfall` | 当前固定步凝结出的米水深 |
| `VegetationPotential` | `[0, 1]` 环境适生度 |

`VegetationPotential` 只是环境条件的响应值。它不等于树木数量，也不会创建实体或引用具体植被模型。先把这层语义留干净，后面做材质反馈和实例分布时才有调整空间。

每个固定步按下面的顺序执行：

```text
Temperature Relaxation
  → SurfaceWater 经 Evaporation 转入 AtmosphericMoisture
  → Conservative Upwind Moisture Advection
  → AtmosphericMoisture 经 Condensation / Orographic Rain 转回 SurfaceWater
  → Vegetation Potential Response
```

温度会向 `SeaLevelTemperature - LapseRate × TerrainHeight` 缓慢靠近。湿度沿二维风向做显式迎风输运，`abs(WindVelocity) × dt / CellSize` 超过稳定范围时按 CFL 比例统一缩放。封闭边界会把原本要离开网格的水汽留在当前格，不会偷偷损失质量。

饱和水汽量随温度指数变化，超过饱和值的部分按凝结率形成降雨。迎风坡降雨使用 `dot(WindVelocity, TerrainGradient)`，只接受风沿坡面上升时的正值。植被潜力则由地表水适宜度和温度适宜度相乘，再按响应速率逐步靠近目标，不会一帧跳到最终结果。

这套 CPU Runtime 沿用固定时间步、最大追赶子步和 Dropped Time。`Play`、`Pause`、`SingleStep`、`Reset` 的行为与 P13 水文一致。统计把大气水量与地表水量放在同一预算里；累计 Evaporation 和 Rainfall 只用于解释内部转移，不能再次当作系统增减量相加。

无窗口回归新增 9 项，覆盖暂停/单步、风向输运、封闭水量守恒、蒸发、迎风坡降雨、植被响应、Reset 和帧划分确定性。重新生成 VS2026 工程后，`Debug | x64` 全解决方案增量构建通过，当时共 123 项断言全部通过。

## GPU 气候场与地形诊断

CPU 数值稳定后，同一套规则被迁入 `TerrainClimateGPU`。每个 `TerrainRuntime` 独占一份气候状态；TerrainComponent 只保存可持久化的地形规格，不持有这些纹理，Scene YAML 也不会记录它们。

GPU 资源按是否需要历史状态拆开：

| 场 | GPU 资源 |
| --- | --- |
| Temperature | `R32F` Ping-Pong |
| AtmosphericMoisture | `R32F` Ping-Pong |
| VegetationPotential | `R32F` Ping-Pong |
| Rainfall | 单张 `R32F` 派生纹理 |
| SurfaceWater | 只读 Hydrology Water；无水文 Runtime 时使用零纹理 |

Temperature、Moisture 和 Vegetation 会参与下一步计算，所以必须保留 Current/Next。Rainfall 只描述最近一个固定步的输出，没有历史反馈，单张纹理已经够用。这样拆资源比把所有值塞进一张 RGBA 纹理更啰嗦一点，却能把读写关系看得很清楚。

一个气候固定步最初由三段 Compute 组成：

```text
ClimateSource
  Temperature Relaxation + Evaporation
  → Barrier → Temperature/Moisture Swap

ClimateAdvection
  Conservative Upwind Moisture Transport
  → Barrier → Moisture Swap

ClimateResponse
  Condensation + Orographic Rain + Vegetation Response
  → Barrier → Moisture/Vegetation Swap
```

三个 Pass 都只读 Current、只写 Next。湿度输运沿风向计算出流，CFL 总比例大于 1 时统一缩放；网格边缘没有下风邻格，就把那部分水汽留在本格。Response 使用世界高度梯度判断迎风坡，负向或平坦地形不会得到额外抬升降雨。

`TerrainRenderer` 按 GenerationVersion 创建或重建气候资源，并用 FrameSerial 挡住同一渲染帧内的重复推进。Shadow Pass 和九个 Terrain Chunk 都会触发 Prepare，但气候固定步只执行一次。三个 Compute Shader 也接入事务式热重载，编译失败时继续使用上一份有效程序。

调试入口在 `Debug → Overview → Runtime Climate`。这里可以控制 Play、Single Step 和 Reset，修改二维 Wind Velocity，设置下一次 Reset 使用的 Initial Moisture，并在 Temperature、Atmospheric Moisture、Rainfall、Vegetation Potential 四种视图间切换。`Readback` 显示各场范围，`Run GPU Contract` 会执行受控 `3×1` 小网格。

Terrain Shader 使用 slot 28 到 31 读取四个诊断场。水文和气候诊断同时开启时，水文优先；两者关闭后仍走正常 Terrain PBR。颜色只用于辨认数值分布：Temperature 从蓝、绿过渡到红，Moisture 从褐色过渡到青色，Rainfall 使用暗紫到亮青，VegetationPotential 使用褐色到绿色。

GTX 1050 / OpenGL 4.6 上，`ClimateSource`、`ClimateAdvection`、`ClimateResponse` 和 Terrain Shader 编译成功。`GLIMMER_CLIMATE_VALIDATE=1` 的 GPU Contract 得到 Downwind Moisture `1`、Rising/Flat Rain `0.2/0`、Frame Partition Delta `0`。同轮 `Debug | x64` 构建和当时的 123 项无窗口断言全部通过。

项目里已经导入 Quaternius Ultimate Nature Pack，包含 OBJ、FBX 和作为源文件保留的 Blend。气候 Runtime 没有硬编码引用这些模型，整包也不会自动写入 AssetRegistry。树木、灌木和草地仍要等适生度接入材质权重后，再设计物种参数、LOD 与实例批次。

## GPU 气候与水文守恒耦合

气候场能产生 Rainfall，也能从 SurfaceWater 蒸发水分。真正接入水文时，最先要解决的是写入权：如果 Climate 和 Hydrology 各自修改 Water，两个 Ping-Pong 状态很快就会失去一致性。

最终约定是 Hydrology 继续独占 Water 写入。`TerrainClimateGPU` 新增 Evaporation 与有符号 WaterSource 两张 `R32F` 输出，最后一个气候 Pass 只计算请求量：

```text
WaterSource = Rainfall - Evaporation
```

正值表示向地表补水，负值表示移除水深。Climate 不直接写 Water；Hydrology 的 Flux、Water Update 和 Sediment Transport 都读取同一份 Source/Sink。负值会按当前可用水深限幅，避免蒸发把 Water 拉成负数。

为了让统计反映 GPU 真正执行的结果，`HydrologyUpdate` 会把经过限幅的实际应用量累加到 WaterSourceBudget Ping-Pong。显式 Readback 再从这张预算纹理重建 Expected Water。CPU 端不假设所有请求都已成功，因此极端蒸发条件下的质量误差仍有可信含义。

调度统一交给 `TerrainEnvironmentGPU`。原来的 Hydrology 和 Climate Play/Single Step 入口仍然保留，但它们进入同一个固定步累加器：

```text
ClimateSource → ClimateAdvection → ClimateResponse
  → ClimateWaterSource → Barrier
  → HydrologyFlux → HydrologyUpdate → Sediment/Erosion
```

这个顺序让当前水面先参与蒸发与降雨计算，随后把完整 WaterSource 交给水流。两套 Runtime 不再各用一个 Accumulator，自然也不会在同一帧多跑或漏跑一步。任一 Reset 都会共同恢复气候、水文和预算状态；这些数据依旧不写 TerrainComponent 或 Scene YAML。

Temperature 诊断后来增加了 `Temperature Lapse`，单位是摄氏度/世界单位，近似温差为 `Lapse × HeightScale`。默认 `0.0065` 接近常用大气递减率，但几十单位高的测试地形色差很弱。调试时可以先设为 `0.05` 到 `0.10`，再 Reset 并推进几步。这个参数只改变温度目标，不会缩放地形几何。

Hydrology 的 `Validate / Readback` 与 Climate 的 `Readback` 都会刷新耦合统计。Climate 面板会显示 Atmospheric + Surface Total Water、Expected Total 和 Coupled Water Error。Hydrology 的标量 Rainfall 仍被视为外部水源并计入预算；想观察封闭自然循环时，需要把它设为 `0`。

验证时，新增 `ClimateWaterSource` 与修改后的水文 Shader 在 GTX 1050 / OpenGL 4.6 上编译通过。原水文 Contract 保持通过，相对水量误差为 `9.83321e-7`；空间 Source/Sink Contract 先施加 `+0.10`，再施加 `-0.04`，最终水深 `0.06`，预算误差 `0`。气候 Contract 仍得到 Downwind Moisture `1`、Rising/Flat Rain `0.2/0` 和 Frame Partition Delta `0`。当次 Windows 增量构建及 114 项无窗口回归全部通过。

耦合完成后，开发顺序先转向材质反馈，暂不生成植被实体。当前主线会把 Humidity、Temperature 和 VegetationPotential 接入 Terrain Material Weight，定义动态生态权重如何与已有 Height、Slope、Curvature 权重组合并保持归一化。

## 模型 Shader ABI 与多 Pass 法线外扩

自定义 3D Shader 以前能通过 `.glmat` 接入 Renderer3D，但代价是复制 `PBRModel.glsl` 里的整套声明：相机、实例数据、灯光、材质纹理、CSM、IBL、Alpha 和 EntityID 都要自己维护。Renderer 增加字段后，旧 Shader 可能继续编译，读到的内容却已经错位。这个问题比直接报错更麻烦，因为画面坏了，日志仍然安静。

这轮开发做了两件事。第一件是把模型 Shader 的公共输入和输出整理成 ABI；第二件是让需要重复绘制几何的效果进入 Material Pass。Toon Outline 也因此改成真正的法线外扩壳层，不再依赖片元 Fresnel 模拟轮廓。

### 公共 GLSL ABI

公共文件放在 `assets/shaders/Glimmer`，后缀使用 `.glslinc`。AssetRegistry 不会把它们当成可独立链接的 Shader：

| 文件 | 负责内容 |
| --- | --- |
| `ModelVertexABI.glslinc` | location 0 到 8 的模型/实例输入、Transform、EntityID 和标准顶点输出 |
| `ForwardFragmentABI.glslinc` | LightEnvironment binding 1、材质纹理、CSM、IBL、相机和 Scene MRT |
| `Surface.glslinc` | BaseColor、Normal、AO、AlphaMode 解析，以及 Color/EntityID 输出 |
| `ShadowCSM.glslinc` | 四级联选择、级联过渡和 `3×3 PCF` |

图形 Shader 预处理器现在支持递归 `#include`：

```glsl
#include "LocalFunctions.glslinc"               // 相对当前文件
#include <Glimmer/ModelVertexABI.glslinc>       // 相对顶层 Shader 目录
```

Include 文件缺失、语法不完整或形成循环时，编译会带着来源路径失败。成功链接后，主文件与每个递归依赖都会创建 FileWatcher。修改一份公共 ABI，所有使用它的 Shader 都会进入原有热重载流程；新 Program 编译失败时，运行中仍保留上一份有效版本。

`PBRModel.glsl` 也已经改为消费这套 ABI。这样公共契约有了独立来源，不需要再从某个具体 PBR Shader 中复制。自定义 Shader 的顶点阶段可以缩到：

```glsl
#type vertex
#version 450 core
#include <Glimmer/ModelVertexABI.glslinc>

void main()
{
    GlimmerWriteModelVertex(a_Position, a_Normal, a_Tangent, a_TexCoord);
}
```

片元阶段包含 `Surface.glslinc` 后，调用 `GlimmerResolveSurface()` 得到统一材质表面，再实现自己的光照，最后通过 `GlimmerWriteColor()` 同步写出 Color 和 EntityID。`ToonSurface.glsl` 就沿用这条路径，只额外定义色阶阈值、Rim 及 Toon 光照。

### `.glmat` 多 Pass 契约

旧材质仍读取顶层 `Shader`，没有 `Passes` 时，Renderer3D 会生成一个兼容 Pass，原来的单 Pass 行为不变。材质显式声明 `Passes` 后，每个 Mesh 会展开为多条 RenderItem。例如 Toon 材质包含 Outline 和 Forward：

```yaml
Passes:
  - Name: Outline
    Shader: 17101010101010101001
    Order: 0
    Cull: Front
    DepthWrite: true
    Queue: Opaque
    Parameters:
      u_OutlineWidth: 0.025
      u_OutlineColor: [0.025, 0.02, 0.04, 1]

  - Name: Forward
    Shader: 17101010101010101002
    Order: 100
    Cull: Back
    DepthWrite: true
    Queue: Material
    Parameters:
      u_ToonShadowThreshold: 0.2
      u_ToonLightThreshold: 0.72
      u_ToonRimStrength: 0.18
```

`Order` 先划分 Opaque 队列中的 Pass 阶段，同 Order 内再按 Shader、Material、Texture 和 Mesh 排序。`Cull` 支持 `None`、`Back`、`Front`。`Queue: Material` 跟随基础 AlphaMode；`Queue: Opaque` 会把壳层留在不透明阶段，适合先于表面完成的 Outline。

`DepthWrite` 对不透明 Pass 生效，透明队列仍会强制关闭深度写入。`Parameters` 当前只支持 Float 和 Float4，上传顺序位于标准 Material Uniform 之后，所以 Pass 可以覆盖同名参数。Pass 的 Shader、状态和参数都参与合批兼容判断；两个内容相同的 Outline 仍可走实例化，状态不同的 Pass 则会拆开。队列结束后，Cull、DepthWrite、Blend 和 DepthFunc 都会恢复默认状态，避免影响后面的 Renderer。

实体 `MaterialOverrides` 仍只修改表面属性，不会替换共享材质的 Pass 结构或参数。`MaterialState` 已经包含完整 Pass 列表，因此保存、重载和材质 Undo 能够往返这些数据。Inspector 目前没有 Pass 列表编辑器，新增或调整 Pass 仍需复制示例 `.glmat` 并编辑 YAML。

### 法线外扩 Toon

`DefaultToonOutline.glmat` 可以直接拖给带 ModelRenderer 的实体。第一个 Pass 使用 `ToonOutline.glsl`，沿世界空间法线外扩顶点并启用 Front Cull，只绘制模型背面的壳层；第二个 Pass 使用 Back Cull 的 `ToonSurface.glsl` 绘制原表面。

两个 Pass 都写入同一个 EntityID，所以点击外扩轮廓仍会选中原实体。ShadowRenderer 不展开材质 Pass，它继续按基础 Material 的 Opaque、Mask、Blend 契约提交一次原模型。Outline 壳层不会被重复写进四张级联阴影图，这也避免轮廓宽度改变阴影体积。

验证同时覆盖无窗口数据回归和真实 GPU 路径。回归测试确认旧材质默认值保持一致，并往返保存 Pass 顺序、Cull、Queue、DepthWrite、Float 和 Float4 参数；VS2026 `Debug | x64` 的 Glimmer、编辑器与回归目标构建通过。设置 `GLIMMER_TOON_LAB_AUTORUN=1` 后，GTX 1050 / OpenGL 4.6 成功编译 PBR 与 Toon 的递归 Include，隔离场景中的 6 个球各提交 2 个 Pass，最终渲染 `12/12` RenderItem，没有跳过模型；四级联 Shadow 候选保持 `24/24`。
