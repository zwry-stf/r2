function r2_define_common()
    flags { "MultiProcessorCompile" }
    warnings "Extra"
    fatalwarnings { "All" }

    filter "action:vs*"
        buildoptions { "/sdl" }
    filter {}
    
    filter "configurations:Debug*"
        symbols "On"
        defines { "_DEBUG" }
    filter "configurations:Release*"
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
    filter { "system:windows", "platforms:x86" }
        defines { "R2_PLATFORM_WINDOWS_X86" }
    filter { "system:windows", "platforms:x64" }
        defines { "R2_PLATFORM_WINDOWS_X64" }
    filter {}
    
    -- backend
    filter { "options:backend=d3d11" }
        defines { "R2_BACKEND_D3D11" }
    filter { "options:backend=opengl" }
        defines { "R2_BACKEND_OPENGL" }
    filter {}
end