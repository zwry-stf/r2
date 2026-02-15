

r2 = r2 or {}

local function join(base, rel)
    if base == nil or base == "" then
        return rel
    end
    return path.join(base, rel)
end

function r2.set_common_project_settings(groups)
    groups = groups or {}
    local debug_groups   = groups.debug   or { "Debug*" }
    local release_groups = groups.release or { "Release*" }

    language "C++"
    cppdialect "C++23"
    staticruntime "Off"

    multiprocessorcompile "On"
    warnings "Extra"
    fatalwarnings { "All" }

    for _, cfgpat in ipairs(debug_groups) do
        filter("configurations:" .. cfgpat)
            symbols "On"
            defines { "_DEBUG" }
    end

    for _, cfgpat in ipairs(release_groups) do
        filter("configurations:" .. cfgpat)
            optimize "On"
            intrinsics "On"
            linktimeoptimization "On"
            defines { "NDEBUG" }
    end

    filter {}
end

function r2.set_project_backend_defines(backend)
    if backend == "d3d11" then
        defines { "R2_BACKEND_D3D11" }
    elseif backend == "opengl" then
        defines { "R2_BACKEND_OPENGL", "GLEW_STATIC" }
    else
        error("r2: invalid backend (expected 'd3d11' or 'opengl').")
    end
    
    filter "system:windows"
        systemversion "latest"
        defines { "R2_PLATFORM_WINDOWS" }
    filter { "system:windows", "platforms:x86" }
        defines { "R2_PLATFORM_WINDOWS_X86" }
    filter { "system:windows", "platforms:x64" }
        defines { "R2_PLATFORM_WINDOWS_X64" }
    filter {}
end

function r2.use(opts)
    local base = opts.base or ""
    local backend = opts.backend
    local include_implementation = opts.include_impl or false

    includedirs {
        join(base, "backend/include"),
        join(base, "r2/include"),
        join(base, "r2/src"),
        join(base, "r2/ext"),
    }

    if include_implementation then
        if backend == "d3d11" then
            includedirs {
                join(base, "backend_d3d11/include"),
            }
        elseif backend == "opengl" then
            includedirs {
                join(base, "backend_opengl/include"),
                join(base, "backend_opengl/ext"),
            }
        else
            error("r2.use: invalid backend.")
        end
    end

    links { "r2", "backend" }
    
    if backend == "d3d11" then
        links { "backend_d3d11", "d3d11", "d3dcompiler" }
    elseif backend == "opengl" then
        links { "backend_opengl", "opengl32" }
    else
        error("r2.use: invalid backend.")
    end
end

function r2.add_projects(opts)
    local base = opts.base or ""
    local backend = opts.backend

    local build_root = opts.build_root or ("build/%{prj.name}")
    local int_root   = opts.int_root   or ("build/%{prj.name}/%{cfg.buildcfg}/%{cfg.platform}")

    -- backend
    project "backend"
        kind "StaticLib"
        targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
        targetdir (build_root)
        objdir    (int_root)
        location  (join(base, "backend"))

        r2.set_common_project_settings()
        r2.set_project_backend_defines(backend)

        files {
            join(base, "backend/include/**.h"),
            join(base, "backend/include/**.inl"),
            join(base, "backend/src/**.h"),
            join(base, "backend/src/**.cpp"),
        }

        includedirs {
            join(base, "backend/include"),
        }

        if backend == "d3d11" then
            includedirs { join(base, "backend_d3d11/include") }
        elseif backend == "opengl" then
            includedirs {
                join(base, "backend_opengl/include"),
                join(base, "backend_opengl/ext"),
            }
        end

    -- backend d3d11
    if backend == "d3d11" then
        project "backend_d3d11"
            kind "StaticLib"
            targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
            targetdir (build_root)
            objdir    (int_root)
            location  (join(base, "backend_d3d11"))
            
            r2.set_common_project_settings()
            r2.set_project_backend_defines(backend)

            files {
                join(base, "backend_d3d11/include/**.h"),
                join(base, "backend_d3d11/include/**.inl"),
                join(base, "backend_d3d11/src/**.h"),
                join(base, "backend_d3d11/src/**.cpp"),
            }

            includedirs {
                join(base, "backend_d3d11/include"),
                join(base, "backend/include"),
            }
    end

    -- backend opengl
    if backend == "opengl" then
        project "backend_opengl"
            kind "StaticLib"
            targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
            targetdir (build_root)
            objdir    (int_root)
            location  (join(base, "backend_opengl"))
            
            r2.set_common_project_settings()
            r2.set_project_backend_defines(backend)

            files {
                join(base, "backend_opengl/include/**.h"),
                join(base, "backend_opengl/include/**.inl"),
                join(base, "backend_opengl/src/**.h"),
                join(base, "backend_opengl/src/**.cpp"),
                join(base, "backend_opengl/ext/**.h"),
            }

            includedirs {
                join(base, "backend_opengl/include"),
                join(base, "backend_opengl/ext"),
                join(base, "backend/include"),
            }

            defines { "GLEW_STATIC" }
    end

    -- r2
    project "r2"
        kind "StaticLib"
        targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
        targetdir (build_root)
        objdir    (int_root)
        location  (join(base, "r2"))
        
        r2.set_common_project_settings()
        r2.set_project_backend_defines(backend)

        files {
            join(base, "r2/include/**.h"),
            join(base, "r2/include/**.inl"),
            join(base, "r2/src/**.h"),
            join(base, "r2/src/**.cpp"),
            join(base, "r2/src/**.inl"),
            join(base, "r2/ext/**.h"),
        }

        includedirs {
            join(base, "backend/include"),
            join(base, "r2/include"),
            join(base, "r2/src"),
            join(base, "r2/ext"),
        }

        links { "backend" }
        if backend == "d3d11" then links { "backend_d3d11" } end
        if backend == "opengl" then links { "backend_opengl" } end
end