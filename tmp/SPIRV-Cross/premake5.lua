project "SPIRV-Cross"
    location "."
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("../../bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../../bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "**.h",
        "**.cpp"
    }

    removefiles {
        "main.cpp"  -- CLI 工具，不参与库编译
    }

    includedirs {
        ".",
        "../Vulkan-Headers/include"
    }

    defines { "SPIRV_CROSS_STATIC" }

    filter "system:windows"
        systemversion "latest"
