function r2_define_common(backend_name)
    multiprocessorcompile "On"
    warnings "Extra"
    fatalwarnings { "All" }

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
    if backend_name == "d3d11" then
        defines { "R2_BACKEND_D3D11" }
    elseif backend_name == "opengl" then
        defines { "R2_BACKEND_OPENGL" }
    else
        error("no valid backend found.")
    end
end