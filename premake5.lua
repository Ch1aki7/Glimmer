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

filter {}

IncludeDir = {}

IncludeDir["spdlog"] = "Glimmer/vendor/spdlog/include"
IncludeDir["GLFW"] = "Glimmer/vendor/GLFW/include"
IncludeDir["Glad"] = "Glimmer/vendor/Glad/include"
IncludeDir["ImGui"] = "Glimmer/vendor/imgui"
IncludeDir["glm"] = "Glimmer/vendor/glm"
IncludeDir["stb_image"] = "Glimmer/vendor/stb_image"
IncludeDir["tinyobjloader"] = "Glimmer/vendor/tinyobjloader"
IncludeDir["entt"] = "Glimmer/vendor/entt/src"

group "Dependencies"
include "Glimmer/vendor/GLFW"
include "Glimmer/vendor/Glad"
include "Glimmer/vendor/imgui"
group ""

include "Glimmer"
include "Sandbox"
include "GlimmerEditor"