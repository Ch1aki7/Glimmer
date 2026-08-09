project "GlimmerRegressionTests"
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
        "../GlimmerEditor-CyouBranch/src/Editor/EditorCommand.cpp"
    }

    includedirs {
        "../Glimmer/src",
		"../GlimmerEditor-CyouBranch/src",
        "../" .. IncludeDir["spdlog"],
		"../" .. IncludeDir["ImGui"],
        "../" .. IncludeDir["glm"],
        "../" .. IncludeDir["entt"],
		"../" .. IncludeDir["yaml-cpp"],
		"../" .. IncludeDir["ImGuizmo"]
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
