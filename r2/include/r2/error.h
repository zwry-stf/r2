#pragma once
#include <backend/def.h>
#include <cstdint>
#include <string>


r2_begin_

enum class error_code : std::int32_t {
    context_initialization,
    vertex_shader_compile,
    vertex_shader_create,
    input_layout_create,
    constant_buffer_create,
    pixel_shader_compile,
    pixel_shader_create,
    shader_program_create,
    blend_state_create,
    rasterizer_state_create,
    depth_stencil_state_create,
    font_build,
    font_tex_create,
    font_atlas_full,
    sampler_create,
};

class error {
private:
    error_code code_;
    std::int32_t error_;
    std::int32_t detail_;

public:
    error(error_code code, std::int32_t error, std::int32_t detail) noexcept
        : code_(code), 
          error_(error), 
          detail_(detail) { }

    error(error_code code) noexcept
        : error(code, -1, 0) { }

public:
    [[nodiscard]] auto get_code() const noexcept {
        return code_;
    }
    [[nodiscard]] auto get_error() const noexcept {
        return error_;
    }
    [[nodiscard]] auto get_detail() const noexcept {
        return detail_;
    }

    [[nodiscard]] std::string to_string() const {
        std::string ret;
        
        switch (code_) {
        case error_code::context_initialization:
            ret = "Context initialization failed.";
            break;

        case error_code::vertex_shader_compile:
            ret = "Vertex shader compilation failed.";
            break;

        case error_code::vertex_shader_create:
            ret = "Vertex shader creation failed.";
            break;

        case error_code::input_layout_create:
            ret = "Input layout creation failed.";
            break;

        case error_code::constant_buffer_create:
            ret = "Constant buffer creation failed.";
            break;

        case error_code::pixel_shader_compile:
            ret = "Pixel shader compilation failed.";
            break;

        case error_code::pixel_shader_create:
            ret = "Pixel shader creation failed.";
            break;

        case error_code::shader_program_create:
            ret = "Shader program creation failed.";
            break;

        case error_code::blend_state_create:
            ret = "Blend state creation failed.";
            break;

        case error_code::rasterizer_state_create:
            ret = "Rasterizer state creation failed.";
            break;

        case error_code::depth_stencil_state_create:
            ret = "Depth stencil state creation failed.";
            break;

        case error_code::font_build:
            ret = "Font build failed.";
            break;

        case error_code::font_tex_create:
            ret = "Font texture creation failed.";
            break;

        case error_code::font_atlas_full:
            ret = "Font atlas is full.";
            break;

        case error_code::sampler_create:
            ret = "Sampler creation failed.";
            break;

        default:
            ret = "Unknown error.";
            break;
        }

        if (error_ != -1) {
            ret += " ( ";
            ret += std::to_string(error_);
            if (detail_ != 0) {
                ret += " - ";
                ret += std::to_string(detail_);
            }

            ret += " )";
        }

        return ret;
    }
};

r2_end_