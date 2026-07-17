project "yaml-cpp"
    location "."
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("../../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../../bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "src/**.h",
        "src/**.cpp",
        "include/**.h"
    }

    includedirs {
        "include"
    }

    filter "system:windows"
        systemversion "latest"
        defines { "YAML_CPP_STATIC_DEFINE", "_CRT_SECURE_NO_WARNINGS" }

    filter "configurations:Debug"
        defines { "YAML_CPP_STATIC_DEFINE" }

    filter "configurations:Release or Dist"
        defines { "YAML_CPP_STATIC_DEFINE" }
