local function r2_join(base_path, rel)
    local base = base_path or ""
    if base == "" then
        return rel
    end
    return path.join(base, rel)
end

function r2_define_projects(base_path, build_root, int_root, backends, backend_name)
project "backend"
    kind "StaticLib"
    targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location (P("backend"))

    files {
        P("backend/include/**.h"),
        P("backend/include/**.inl"),
        P("backend/src/**.h"),
        P("backend/src/**.cpp")
    }

    includedirs {
        P("backend/include")
    }
    
    filter { "options:backend=d3d11" }
        includedirs { P("backend_d3d11/include") }
    filter { "options:backend=opengl" }
        includedirs {
            P("backend_opengl/include"),
            P("backend_opengl/ext")
        }
        defines { "GLEW_STATIC" }
    filter {}
    
project "backend_d3d11"
    kind "StaticLib"
    targetname "%{prj.name}_%{cfg.buildcfg:match('^[^_]+')}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location (P("backend_d3d11"))

    files {
        P("backend_d3d11/include/**.h"),
        P("backend_d3d11/include/**.inl"),
        P("backend_d3d11/src/**.h"),
        P("backend_d3d11/src/**.cpp")
    }

    includedirs {
        P("backend_d3d11/include"),
        P("backend/include")
    }
    
    filter { "options:backend=opengl" }
        removefiles { "**" }
        flags { "ExcludeFromBuild" }
    filter {}
    
project "backend_opengl"
    kind "StaticLib"
    targetname "%{prj.name}_%{cfg.buildcfg:match('^[^_]+')}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location (P("backend_opengl"))

    files {
        P("backend_opengl/include/**.h"),
        P("backend_opengl/include/**.inl"),
        P("backend_opengl/src/**.h"),
        P("backend_opengl/src/**.cpp"),
        P("backend_opengl/ext/**.h")
    }

    includedirs {
        P("backend_opengl/include"),
        P("backend_opengl/ext"),
        P("backend/include")
    }

    defines { "GLEW_STATIC" }
    
    filter { "options:backend=d3d11" }
        removefiles { "**" }
        flags { "ExcludeFromBuild" }
    filter {}

project "r2"
    kind "StaticLib"
    targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location (P("r2"))
    
    files {
        P("r2/include/**.h"),
        P("r2/include/**.inl"),
        P("r2/src/**.h"),
        P("r2/src/**.cpp"),
        P("r2/src/**.inl"),
        P("r2/ext/**.h")
    }

    includedirs {
        P("backend/include"),
        P("r2/include"),
        P("r2/src"),
        P("r2/ext")
    }
    
    dependson { "backend", "backend_d3d11", "backend_opengl" }
end