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