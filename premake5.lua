workspace "GlimmerEngine"
    architecture "x64"
    startproject "Sandbox"

    configurations { "Debug", "Release", "Dist" }

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Glimmer"
    location "Glimmer"
    kind "StaticLib"
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
        "%{prj.name}/src"
    }

    filter "system:windows"
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
        "Glimmer/src" -- 沙盒需要引用引擎的代码
    }

    links {
        "Glimmer" -- 沙盒链接引擎的静态库
    }

    filter "system:windows"
    systemversion "latest"
    defines {
        "GL_PLATFORM_WINDOWS"
    }