workspace "r2"
    configurations { "Debug_d3d11", "Debug_opengl", "Release_d3d11", "Release_opengl" }
    platforms { "Win32", "x64" }
    language "C++"
    cppdialect "C++23"
    staticruntime "Off"

    filter "platforms:Win32"
        architecture "x86"
    filter "platforms:x64"
        architecture "x86_64"
    filter {}
    
    local build_root = "build/%{prj.name}"
    local int_root   = "build/%{prj.name}/%{cfg.buildcfg}/%{cfg.platform}"

    local action = _ACTION
    if action == nil or action == "" then
        action = "unknown_action"
    end
    local host = os.host() or "unknown_host"
        
    location ("out/" .. action .. "/" .. host)
    
    flags { "MultiProcessorCompile" }
    warnings "Extra"
    fatalwarnings { "All" }

    filter "action:vs*"
        buildoptions { "/sdl" }
    filter {}
    
    filter "configurations:Debug_*"
        symbols "On"
        defines { "_DEBUG" }
    filter "configurations:Release_*"
        optimize "On"
        intrinsics "On"
        linktimeoptimization "On"
        defines { "NDEBUG" }

        filter "action:vs*"
            buildoptions { "/Gy" }
        filter {}
    filter {}
    
    -- platform
    filter "system:windows"
        systemversion "latest"
        defines { "R2_PLATFORM_WINDOWS" }
    filter { "system:windows", "platforms:Win32" }
        defines { "R2_PLATFORM_WINDOWS_X86" }
    filter { "system:windows", "platforms:x64" }
        defines { "R2_PLATFORM_WINDOWS_X64" }
    filter {}
    
    -- backend
    filter { "configurations:*_d3d11" }
        defines { "R2_BACKEND_D3D11" }
    filter { "configurations:*_opengl" }
        defines { "R2_BACKEND_OPENGL" }
    filter {}
    
project "backend"
    kind "StaticLib"
    targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location "backend"

    files {
        "backend/include/**.h",
        "backend/include/**.inl",
        "backend/src/**.h",
        "backend/src/**.cpp"
    }

    includedirs {
        "backend/include"
    }
    
    filter { "configurations:*_d3d11" }
        includedirs {
            "backend_d3d11/include"
        }
    filter { "configurations:*_opengl" }
        includedirs {
            "backend_opengl/include",
            "backend_opengl/ext"
        }
        defines { "GLEW_STATIC" }
    filter {}
    
project "backend_d3d11"
    kind "StaticLib"
    targetname "%{prj.name}_%{cfg.buildcfg:match('^[^_]+')}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location "backend_d3d11"

    files {
        "backend_d3d11/include/**.h",
        "backend_d3d11/include/**.inl",
        "backend_d3d11/src/**.h",
        "backend_d3d11/src/**.cpp"
    }

    includedirs {
        "backend_d3d11/include",
        "backend/include"
    }
    
    filter "configurations:*_opengl"
        removefiles { "**" }
        flags { "ExcludeFromBuild" }
    filter {}
    
project "backend_opengl"
    kind "StaticLib"
    targetname "%{prj.name}_%{cfg.buildcfg:match('^[^_]+')}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location "backend_opengl"

    files {
        "backend_opengl/include/**.h",
        "backend_opengl/include/**.inl",
        "backend_opengl/src/**.h",
        "backend_opengl/src/**.cpp",
        "backend_opengl/ext/**.h"
    }

    includedirs {
        "backend_opengl/include",
        "backend_opengl/ext",
        "backend/include"
    }

    defines { "GLEW_STATIC" }
    
    filter "configurations:*_d3d11"
        removefiles { "**" }
        flags { "ExcludeFromBuild" }
    filter {}

project "r2"
    kind "StaticLib"
    targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location "r2"
    
    files {
        "r2/include/**.h",
        "r2/include/**.inl",
        "r2/src/**.h",
        "r2/src/**.cpp",
        "r2/src/**.inl",
        "r2/ext/**.h"
    }

    includedirs {
        "backend/include",
        "r2/include",
        "r2/src",
        "r2/ext"
    }
    
    dependson { "backend", "backend_d3d11", "backend_opengl" }
    links     { "backend", "backend_d3d11", "backend_opengl" }
    
project "TestRun"
    kind "WindowedApp"
    targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location "TestRun"

    files {
        "TestRun/**.h",
        "TestRun/**.cpp"
    }

    includedirs {
        "TestRun/ext",
        "r2/include",
        "backend/include",
        "backend_d3d11/include"
    }

    dependson { "r2" }
    links     { "r2", "backend" }

    filter { "system:windows", "platforms:x64" }
        links { "TestRun/ext/GLFW/windows/x64/glfw3" }
    filter { "system:windows", "platforms:Win32" }
        links { "TestRun/ext/GLFW/windows/x86/glfw3" }
    filter { }
    
    filter { "configurations:*_opengl", "system:windows", "platforms:x64" }
        links { "TestRun/ext/gl/windows/x64/glew32s" }
    filter { "configurations:*_opengl", "system:windows", "platforms:Win32" }
        links { "TestRun/ext/gl/windows/x64/glew32s" }
    filter { }
    
    filter { "configurations:*_d3d11" }
        links { "backend_d3d11", "d3d11", "d3dcompiler" }
    filter { }
    
    filter { "configurations:*_opengl" }
        links { "backend_opengl", "opengl32" }
    filter { }
