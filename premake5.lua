workspace "GlimmerEngine"
    architecture "x64"
    startproject "Sandbox"

    configurations { "Debug", "Release", "Dist" }

        -- 针对 Debug 配置，定义 GL_DEBUG 宏
    filter "configurations:Debug"
        defines "GL_DEBUG"
        symbols "On" 

    -- 针对 Release 配置
    filter "configurations:Release"
        defines "GL_RELEASE"
        optimize "On" 

    -- 针对 Dist (最终发布) 配置
    filter "configurations:Dist"
        defines "GL_DIST"
        optimize "On"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "Dependencies"
include "Glimmer/vendor/GLFW"
include "Glimmer/vendor/Glad"
include "Glimmer/vendor/imgui"
group ""

project "Glimmer"
    location "Glimmer"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "glpch.h" -- 告诉编译器 PCH 的名字
    pchsource "Glimmer/src/glpch.cpp" -- 只有 Glimmer 项目需要这个源文件

    files {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
    }
    
    includedirs {
        "%{prj.name}/src",
        "%{prj.name}/vendor/spdlog/include",
        "%{prj.name}/vendor/GLFW/include",
        "%{prj.name}/vendor/Glad/include",
        "%{prj.name}/vendor/imgui",
        "%{prj.name}/vendor/imgui/backends",
        "%{prj.name}/vendor/glm"
    }

    links {
        "GLFW", -- 直接写项目名，Premake 会自动处理静态库链接
        "opengl32.lib",
        "Glad",
        "ImGui"
    }

    filter "system:windows"
    buildoptions { "/utf-8" }
    systemversion "latest"
    defines {
        "GL_PLATFORM_WINDOWS",
        "GL_BUILD_DLL" -- 预留，虽然我们现在是静态库
    }

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
    }

    includedirs {
        "Glimmer/src", -- 沙盒需要引用引擎的代码
        "Glimmer/vendor/spdlog/include",
        "Glimmer/vendor/imgui",
        "Glimmer/vendor/glm"
    }

    links {
        "Glimmer" -- 沙盒链接引擎的静态库
    }

    filter "system:windows"
    buildoptions { "/utf-8" }
    systemversion "latest"
    defines {
        "GL_PLATFORM_WINDOWS"
    }