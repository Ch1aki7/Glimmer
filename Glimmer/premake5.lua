project "Glimmer"
    location "."
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "glpch.h"
    pchsource "src/glpch.cpp"

    files {
        "src/**.h",
        "src/**.cpp",
        "vendor/glm/glm/**.hpp",
        "vendor/glm/glm/**.inl",
        "vendor/stb_image/**.h",
        "vendor/stb_image/**.cpp",
        "vendor/tinyobjloader/tiny_obj_loader.h",
        "vendor/tinyobjloader/tiny_obj_loader.cpp"
    }

    includedirs {
        "src",
        "vendor/spdlog/include",
        "vendor/GLFW/include",
        "vendor/Glad/include",
        "vendor/imgui",
        "vendor/imgui/backends",
        "vendor/glm",
        "vendor/stb_image",
        "vendor/tinyobjloader",
		"vendor/assimp/include",
        "vendor/entt/src",
		"vendor/yaml-cpp/include",
		"vendor/ImGuizmo/src"
    }

    links {
        "GLFW",
        "Glad",
        "ImGui",
        "yaml-cpp",
		"ImGuizmo",
		"opengl32.lib"
    }

	filter "configurations:Debug"
		includedirs { "vendor/assimp-build/vs2026-Debug/include" }
		libdirs { "vendor/assimp-build/vs2026-Debug/lib",
			"vendor/assimp-build/vs2026-Debug/contrib/zlib" }
		links { "assimp-vc145-mtd.lib", "zlibstaticd.lib" }
		prebuildcommands { 'call "$(ProjectDir)..\\scripts\\Win-EnsureAssimp-vs2026.bat" Debug' }

	filter "configurations:Release or Dist"
		includedirs { "vendor/assimp-build/vs2026-Release/include" }
		libdirs { "vendor/assimp-build/vs2026-Release/lib",
			"vendor/assimp-build/vs2026-Release/contrib/zlib" }
		links { "assimp-vc145-mt.lib", "zlibstatic.lib" }
		prebuildcommands { 'call "$(ProjectDir)..\\scripts\\Win-EnsureAssimp-vs2026.bat" Release' }

    filter "system:windows"
        buildoptions { "/utf-8" }
        systemversion "latest"

        defines {
            "GL_PLATFORM_WINDOWS",
            "GL_BUILD_DLL",
			"YAML_CPP_STATIC_DEFINE"
        }
