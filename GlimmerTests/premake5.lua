project "GlimmerRegressionTests"
    location "."
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"
    incrementallink "Off"

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

    filter "configurations:Debug"
        editandcontinue "Off"
        -- Glimmer is a reusable Debug static library compiled with /ZI. The
        -- regression executable intentionally links with /INCREMENTAL:NO.
        linkoptions { "/ignore:4075" }

    filter "system:windows"
        buildoptions { "/utf-8" }
        systemversion "latest"

        defines {
            "GL_PLATFORM_WINDOWS"
        }
