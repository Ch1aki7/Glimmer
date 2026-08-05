project "GlimmerEditor"
    location "."
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "src/**.h",
        "src/**.cpp",
        "../resources/windows/Glimmer.rc"
    }

    includedirs {
        "../Glimmer/src",
        "../" .. IncludeDir["spdlog"],
        "../" .. IncludeDir["ImGui"],
        "../" .. IncludeDir["glm"],
        "../" .. IncludeDir["entt"]
    }

    links {
        "Glimmer"
    }

    filter "system:windows"
        buildoptions { "/utf-8" }
        systemversion "latest"

        defines {
            "GL_PLATFORM_WINDOWS"
        }
