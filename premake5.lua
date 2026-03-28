workspace "r2"
    configurations { "Debug", "Release" }
    platforms { "x86", "x64" }

    filter "platforms:x86"
        architecture "x86"
    filter "platforms:x64"
        architecture "x86_64"
    filter {}

    language "C++"
    cppdialect "C++23"
    staticruntime "Off"
    
    startproject "TestRun"

    newoption {
        trigger     = "backend",
        value       = "API",
        description = "Rendering backend",
        allowed = {
            { "d3d11",  "Direct3D 11" },
            { "opengl", "OpenGL" }
        }
    }
    
    local build_root = "build/%{prj.name}"
    local int_root   = "build/%{prj.name}/%{cfg.buildcfg}/%{cfg.platform}"

    local action = _ACTION
    if action == nil or action == "" then
        action = "unknown_action"
    end
    local host = os.host() or "unknown_host"
        
    location ("out/" .. action .. "/" .. host)
    
include "premake/r2.lua"

r2.add_projects {
    base = "",
    build_root = build_root,
    int_root = int_root,
    backend = _OPTIONS["backend"],
}

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
    }

    r2.set_common_project_settings()
    r2.use { base = "", backend = _OPTIONS["backend"], include_impl = true }
    r2.set_project_backend_defines( _OPTIONS["backend"] )

    dependson { "r2" }

    filter { "system:windows", "platforms:x64" }
        links { "TestRun/ext/GLFW/windows/x64/glfw3" }
    filter { "system:windows", "platforms:x86" }
        links { "TestRun/ext/GLFW/windows/x86/glfw3" }
    filter {}

    filter { "options:backend=opengl", "system:windows", "platforms:x64" }
        links { "TestRun/ext/gl/windows/x64/glew32s" }
    filter { "options:backend=opengl", "system:windows", "platforms:x86" }
        links { "TestRun/ext/gl/windows/x86/glew32s" }
    filter {}