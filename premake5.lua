workspace "GlimmerEngine"
    architecture "x64"
    startproject "Sandbox"

    configurations {
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

filter "configurations:Debug"
    defines "GL_DEBUG"
    symbols "On"

filter "configurations:Release"
    defines "GL_RELEASE"
    optimize "On"

filter "configurations:Dist"
    defines "GL_DIST"
    optimize "On"

filter "system:windows"
    buildoptions { "/utf-8", "/wd4828" }  -- UTF-8 源文件 + 抑制 spdlog Unicode 字符警告

filter {}

IncludeDir = {}

IncludeDir["spdlog"] = "Glimmer/vendor/spdlog/include"
IncludeDir["GLFW"] = "Glimmer/vendor/GLFW/include"
IncludeDir["Glad"] = "Glimmer/vendor/Glad/include"
IncludeDir["ImGui"] = "Glimmer/vendor/imgui"
IncludeDir["glm"] = "Glimmer/vendor/glm"
IncludeDir["stb_image"] = "Glimmer/vendor/stb_image"
IncludeDir["tinyobjloader"] = "Glimmer/vendor/tinyobjloader"
IncludeDir["Assimp"] = "Glimmer/vendor/assimp/include"
IncludeDir["entt"] = "Glimmer/vendor/entt/src"
IncludeDir["yaml-cpp"] = "Glimmer/vendor/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "Glimmer/vendor/ImGuizmo/src"
IncludeDir["SPIRV-Cross"] = "Glimmer/vendor/SPIRV-Cross"

-- Vulkan SDK (系统安装) + Vulkan-Headers (git submodule)
local vulkanSDK = os.getenv("VULKAN_SDK")
if vulkanSDK then
	IncludeDir["VulkanSDK"] = vulkanSDK .. "/Include"
	libdirs { vulkanSDK .. "/Lib" }
else
	IncludeDir["Vulkan-Headers"] = "Glimmer/vendor/Vulkan-Headers/include"
end

group "Dependencies"
include "Glimmer/vendor/GLFW"
include "Glimmer/vendor/Glad"
include "Glimmer/vendor/imgui"
include "Glimmer/vendor/yaml-cpp"
include "Glimmer/vendor/ImGuizmo"
include "Glimmer/vendor/SPIRV-Cross"

-- The upstream SPIRV-Cross Premake script recursively includes standalone
-- samples and tests. They are not part of the engine's static library.
project "SPIRV-Cross"
    removefiles {
        "Glimmer/vendor/SPIRV-Cross/samples/**",
        "Glimmer/vendor/SPIRV-Cross/tests/**",
        "Glimmer/vendor/SPIRV-Cross/tests-other/**"
    }
group ""

include "Glimmer"
include "GlimmerTests"
include "Sandbox"
include "GlimmerEditor"
include "GlimmerEditor-CyouBranch"
