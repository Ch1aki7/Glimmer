#pragma once

#ifdef GL_PLATFORM_WINDOWS

// 定义一个能触发断点 (Breakpoint) 的底层系统指令。
// 当程序崩在这里时，Visual Studio 会自动停在这一行，极其方便调试！
#if defined(_MSC_VER) // MSVC 编译器专用
#define GL_DEBUGBREAK() __debugbreak()
#else
#define GL_DEBUGBREAK() // 其他编译器暂时留空
#endif

#else
#error Glimmer currently only supports Windows!
#endif

// 只有在 Debug 模式下才开启断言（Release 和 Dist 模式下剥离以节省性能）
#ifdef GL_DEBUG

    // 引擎内部使用的断言
    // 用法: GL_CORE_ASSERT(window != nullptr, "Window is null!");
#define GL_CORE_ASSERT(x, ...) { if(!(x)) { GL_CORE_FATAL("Assertion Failed: {0}", __VA_ARGS__); GL_DEBUGBREAK(); } }

// 游戏层（Client）使用的断言
// 用法: GL_ASSERT(player.hp > 0, "Player is dead!");
#define GL_ASSERT(x, ...) { if(!(x)) { GL_FATAL("Assertion Failed: {0}", __VA_ARGS__); GL_DEBUGBREAK(); } }

#endif

// BIT(x) 宏： 1 << 0 = 1, 1 << 1 = 2, 1 << 2 = 4...
// 用于 EventCategory 的位掩码判定
#define BIT(x) (1 << x)

#define BIND_EVENT_FN(x) std::bind(&x, this, std::placeholders::_1)

namespace gl {
	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	constexpr Scope<T> CreateScope(Args&&... args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template<typename T, typename... Args>
	constexpr Ref<T> CreateRef(Args&&... args) {
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}