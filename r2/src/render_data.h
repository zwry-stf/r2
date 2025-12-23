#pragma once
#include <backend/context.h>


r2_begin_

class render_data {
public:
    std::unique_ptr<buffer> vertex_buffer;
    std::unique_ptr<buffer> index_buffer;
    std::unique_ptr<buffer> constant_buffer;
    std::unique_ptr<shaderprogram> shader;
    std::unique_ptr<inputlayout> input_layout;
    std::unique_ptr<blendstate> blend_state;
    std::unique_ptr<rasterizerstate> rasterizer_state;
    std::unique_ptr<rasterizerstate> rasterizer_state_ms;
    std::unique_ptr<sampler> sampler;
    std::unique_ptr<depthstencilstate> depth_stencil_state;
    std::unique_ptr<texture2d> font_texture;
    std::unique_ptr<textureview> font_view;

    std::uint32_t vertex_count{ 1000 };
    std::uint32_t index_count{ 2000 };
};

r2_end_