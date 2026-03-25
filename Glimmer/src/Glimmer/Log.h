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

// 引擎层日志宏 (Core)
#define GL_CORE_TRACE(...)  ::gl::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define GL_CORE_ERROR(...)  ::gl::Log::GetCoreLogger()->error(__VA_ARGS__)
#define GL_CORE_WARN(...)   ::gl::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define GL_CORE_INFO(...)   ::gl::Log::GetCoreLogger()->info(__VA_ARGS__)
#define GL_CORE_FATAL(...)  ::gl::Log::GetCoreLogger()->critical(__VA_ARGS__)

// 游戏层日志宏 (Client)
#define GL_TRACE(...)       ::gl::Log::GetClientLogger()->trace(__VA_ARGS__)
#define GL_ERROR(...)       ::gl::Log::GetClientLogger()->error(__VA_ARGS__)
#define GL_WARN(...)        ::gl::Log::GetClientLogger()->warn(__VA_ARGS__)
#define GL_INFO(...)        ::gl::Log::GetClientLogger()->info(__VA_ARGS__)
#define GL_FATAL(...)       ::gl::Log::GetClientLogger()->critical(__VA_ARGS__)