local function font_dependency(name, base, build_root, int_root, groups)
    project(name)
        kind "StaticLib"
        targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
        targetdir(build_root)
        objdir(int_root)
        location(path.join(base, "out/font-dependencies", name))
        r2.set_common_project_settings(groups)
        warnings "Off"
        defines { "_CRT_SECURE_NO_WARNINGS" }
        for _, config in ipairs((groups or {}).release or { "Release*" }) do
            filter("configurations:" .. config)
                optimize "Speed"
        end
        filter {}
end

function r2.add_font_projects(base, build_root, int_root, groups)
    local zlib = path.join(base, "ext/zlib")
    local png = path.join(base, "ext/libpng")
    local ft = path.join(base, "ext/freetype")
    local hb = path.join(base, "ext/harfbuzz")
    local generated = path.join(base, "out/font-dependencies/include")
    os.mkdir(generated)
    assert(os.copyfile(path.join(png, "scripts/pnglibconf.h.prebuilt"), path.join(generated, "pnglibconf.h")))

    font_dependency("r2_zlib", base, build_root, int_root, groups)
        language "C"
        files {
            path.join(zlib, "*.h"),
            path.join(zlib, "adler32.c"), path.join(zlib, "crc32.c"),
            path.join(zlib, "inffast.c"), path.join(zlib, "inflate.c"),
            path.join(zlib, "inftrees.c"), path.join(zlib, "zutil.c"),
            path.join(zlib, "compress.c"), path.join(zlib, "uncompr.c"),
            path.join(zlib, "deflate.c"), path.join(zlib, "trees.c"),
        }
        includedirs { zlib }

    font_dependency("r2_png", base, build_root, int_root, groups)
        language "C"
        files { path.join(png, "*.c"), path.join(png, "*.h") }
        removefiles { path.join(png, "pngtest.c"), path.join(png, "example.c") }
        includedirs { generated, png, zlib }
        defines { "PNG_ARM_NEON_OPT=0", "PNG_INTEL_SSE_OPT=0", "PNG_POWERPC_VSX_OPT=0", "PNG_LOONGARCH_LSX_OPT=0", "PNG_MIPS_MSA_OPT=0" }
        links { "r2_zlib" }

    font_dependency("r2_freetype", base, build_root, int_root, groups)
        language "C"
        includedirs { path.join(ft, "include"), generated, png, zlib }
        defines { "FT2_BUILD_LIBRARY", "FT_CONFIG_OPTION_USE_PNG", "FT_CONFIG_OPTION_SYSTEM_ZLIB" }
        files { path.join(ft, "include/**.h") }
        local modules = {
            "autofit/autofit.c", "base/ftbase.c", "base/ftbbox.c", "base/ftbitmap.c",
            "base/ftdebug.c", "base/ftfstype.c", "base/ftgasp.c", "base/ftglyph.c",
            "base/ftinit.c", "base/ftmm.c", "base/ftstroke.c", "base/ftsynth.c", "base/ftsystem.c",
            "bdf/bdf.c", "cff/cff.c", "cid/type1cid.c", "gzip/ftgzip.c", "lzw/ftlzw.c",
            "pcf/pcf.c", "pfr/pfr.c", "psaux/psaux.c", "pshinter/pshinter.c", "psnames/psmodule.c",
            "raster/raster.c", "sfnt/sfnt.c", "smooth/smooth.c", "sdf/sdf.c", "svg/svg.c",
            "truetype/truetype.c", "type1/type1.c", "type42/type42.c", "winfonts/winfnt.c",
        }
        for _, source in ipairs(modules) do
            files { path.join(ft, "src", source) }
        end
        links { "r2_png", "r2_zlib" }

    font_dependency("r2_harfbuzz", base, build_root, int_root, groups)
        files { path.join(hb, "src/harfbuzz.cc"), path.join(hb, "src/**.h"), path.join(hb, "src/**.hh") }
        includedirs { path.join(hb, "src") }
        defines { "HB_NO_SUBSET", "HB_NO_MMAP", "HB_NO_OPEN", "HB_NO_GETENV" }
        buildoptions { "/bigobj" }
end