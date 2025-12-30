#pragma once
#include <backend/object.h>


r2_begin_

enum class fill_mode : std::uint8_t {
    solid,
    wireframe,
};

enum class cull_mode : std::uint8_t {
    none,
    front,
    back,
};

struct rasterizerstate_desc {
    fill_mode fill      = fill_mode::solid;
    cull_mode cull      = cull_mode::back;
    bool      front_ccw = false;

    int   depth_bias        = 0;
    float depth_bias_clamp  = 0.0f;
    float slope_scaled_bias = 0.0f;

    bool depth_clip_enable  = true;
    bool scissor_enable     = false;
    bool multisample_enable = false;
    bool antialiased_lines  = false;
};

class rasterizerstate : public object<rasterizerstate_desc>
{
protected:
    using object<rasterizerstate_desc>::object;
};

r2_end_