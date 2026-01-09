function r2_location(subdir, base_path)
    local base = base_path or ""
    local sub  = subdir or ""

    if base == "" then
        return sub
    end

    return path.join(base, sub)
end

function r2_define_projects(base_path)
    local p = base_path or ""
project "backend"
    kind "StaticLib"
    targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location (r2_location("backend", base_path))

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
    location (r2_location("backend_d3d11", base_path))

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
    location (r2_location("backend_opengl", base_path))

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
    location (r2_location("r2", base_path))
    
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
end