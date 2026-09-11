-- Glimmer-owned Premake adapters for third-party source trees.
-- Git submodules provide source only; never place required build scripts inside
-- their working trees because untracked files are absent from a fresh clone.

local repositoryRoot = path.getabsolute(path.join(_SCRIPT_DIR, "../.."))
local dependencyProjectRoot = path.join(
    repositoryRoot, "bin-int/projects/Dependencies")
local glfwRoot = path.join(repositoryRoot, "Glimmer/vendor/GLFW")
local gladRoot = path.join(repositoryRoot, "Glimmer/vendor/Glad")
local imguiRoot = path.join(repositoryRoot, "Glimmer/vendor/imgui")
local yamlRoot = path.join(repositoryRoot, "Glimmer/vendor/yaml-cpp")
local imGuizmoRoot = path.join(repositoryRoot, "Glimmer/vendor/ImGuizmo")
local glmRoot = path.join(repositoryRoot, "Glimmer/vendor/glm")
local spirvCrossRoot = path.join(repositoryRoot, "Glimmer/vendor/SPIRV-Cross")
local vulkanHeadersRoot = path.join(
    repositoryRoot, "Glimmer/vendor/Vulkan-Headers")

local function dependencyProject(name)
    project(name)
        location(dependencyProjectRoot .. "/" .. name)
        targetdir(path.join(
            repositoryRoot, "bin/" .. outputdir .. "/%{prj.name}"))
        objdir(path.join(
            repositoryRoot, "bin-int/" .. outputdir .. "/%{prj.name}"))
        staticruntime "on"
end

dependencyProject "GLFW"
    kind "StaticLib"
    language "C"
    warnings "off"

    files {
        glfwRoot .. "/include/GLFW/glfw3.h",
        glfwRoot .. "/include/GLFW/glfw3native.h",
        glfwRoot .. "/src/glfw_config.h",
        glfwRoot .. "/src/context.c",
        glfwRoot .. "/src/init.c",
        glfwRoot .. "/src/input.c",
        glfwRoot .. "/src/monitor.c",
        glfwRoot .. "/src/null_init.c",
        glfwRoot .. "/src/null_joystick.c",
        glfwRoot .. "/src/null_monitor.c",
        glfwRoot .. "/src/null_window.c",
        glfwRoot .. "/src/platform.c",
        glfwRoot .. "/src/vulkan.c",
        glfwRoot .. "/src/window.c"
    }

    filter "system:windows"
        systemversion "latest"
        files {
            glfwRoot .. "/src/win32_init.c",
            glfwRoot .. "/src/win32_joystick.c",
            glfwRoot .. "/src/win32_module.c",
            glfwRoot .. "/src/win32_monitor.c",
            glfwRoot .. "/src/win32_time.c",
            glfwRoot .. "/src/win32_thread.c",
            glfwRoot .. "/src/win32_window.c",
            glfwRoot .. "/src/wgl_context.c",
            glfwRoot .. "/src/egl_context.c",
            glfwRoot .. "/src/osmesa_context.c"
        }
        defines { "_GLFW_WIN32", "_CRT_SECURE_NO_WARNINGS" }

    filter "system:linux"
        pic "On"
        systemversion "latest"
        files {
            glfwRoot .. "/src/x11_init.c",
            glfwRoot .. "/src/x11_monitor.c",
            glfwRoot .. "/src/x11_window.c",
            glfwRoot .. "/src/xkb_unicode.c",
            glfwRoot .. "/src/posix_module.c",
            glfwRoot .. "/src/posix_poll.c",
            glfwRoot .. "/src/posix_time.c",
            glfwRoot .. "/src/posix_thread.c",
            glfwRoot .. "/src/glx_context.c",
            glfwRoot .. "/src/egl_context.c",
            glfwRoot .. "/src/osmesa_context.c",
            glfwRoot .. "/src/linux_joystick.c"
        }
        defines "_GLFW_X11"

    filter "system:macosx"
        pic "On"
        files {
            glfwRoot .. "/src/cocoa_init.m",
            glfwRoot .. "/src/cocoa_monitor.m",
            glfwRoot .. "/src/cocoa_window.m",
            glfwRoot .. "/src/cocoa_joystick.m",
            glfwRoot .. "/src/macos_time.c",
            glfwRoot .. "/src/nsgl_context.m",
            glfwRoot .. "/src/posix_thread.c",
            glfwRoot .. "/src/posix_module.c",
            glfwRoot .. "/src/osmesa_context.c",
            glfwRoot .. "/src/egl_context.c"
        }
        defines "_GLFW_COCOA"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "On"

    filter "configurations:Release or Dist"
        runtime "Release"
        optimize "Speed"

filter {}

dependencyProject "Glad"
    kind "StaticLib"
    language "C"
    files {
        gladRoot .. "/include/glad/glad.h",
        gladRoot .. "/include/KHR/khrplatform.h",
        gladRoot .. "/src/glad.c"
    }
    includedirs(gladRoot .. "/include")
    filter "system:windows"
        systemversion "latest"

filter {}

dependencyProject "ImGui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    files {
        imguiRoot .. "/imgui.cpp",
        imguiRoot .. "/imgui_draw.cpp",
        imguiRoot .. "/imgui_tables.cpp",
        imguiRoot .. "/imgui_widgets.cpp",
        imguiRoot .. "/imgui_demo.cpp",
        imguiRoot .. "/backends/imgui_impl_glfw.cpp",
        imguiRoot .. "/backends/imgui_impl_opengl3.cpp",
        imguiRoot .. "/backends/imgui_impl_glfw.h",
        imguiRoot .. "/backends/imgui_impl_opengl3.h"
    }
    includedirs {
        imguiRoot,
        glfwRoot .. "/include",
        gladRoot .. "/include"
    }
    filter "system:windows"
        systemversion "latest"

filter {}

dependencyProject "yaml-cpp"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    files {
        yamlRoot .. "/src/**.h",
        yamlRoot .. "/src/**.cpp",
        yamlRoot .. "/include/**.h"
    }
    includedirs(yamlRoot .. "/include")
    defines "YAML_CPP_STATIC_DEFINE"
    filter "system:windows"
        systemversion "latest"
        defines "_CRT_SECURE_NO_WARNINGS"

filter {}

dependencyProject "ImGuizmo"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    files {
        imGuizmoRoot .. "/src/**.h",
        imGuizmoRoot .. "/src/**.cpp"
    }
    includedirs {
        imGuizmoRoot .. "/src",
        imguiRoot,
        glmRoot
    }
    filter "system:windows"
        systemversion "latest"

filter {}

dependencyProject "SPIRV-Cross"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    files {
        spirvCrossRoot .. "/**.h",
        spirvCrossRoot .. "/**.cpp"
    }
    removefiles {
        spirvCrossRoot .. "/main.cpp",
        spirvCrossRoot .. "/samples/**",
        spirvCrossRoot .. "/tests/**",
        spirvCrossRoot .. "/tests-other/**"
    }
    includedirs {
        spirvCrossRoot,
        vulkanHeadersRoot .. "/include"
    }
    defines "SPIRV_CROSS_STATIC"
    filter "system:windows"
        systemversion "latest"

filter {}
