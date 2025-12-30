#pragma once
#include <backend/object.h>


r2_begin_

enum class blend_factor : std::uint8_t {
    zero,
    one,
    src_color,
    inv_src_color,
    src_alpha,
    inv_src_alpha,
    dest_alpha,
    inv_dest_alpha,
    dest_color,
    inv_dest_color,
    src_alpha_sat,
    blend_factor,
    inv_blend_factor,
};

enum class blend_op : std::uint8_t {
    add,
    subtract,
    rev_subtract,
    min,
    max,
};

enum class color_write_mask : std::uint8_t {
    none = 0,
    r = 1 << 0,
    g = 1 << 1,
    b = 1 << 2,
    a = 1 << 3,
    all = r | g | b | a,
};

inline color_write_mask operator|(color_write_mask lhs, color_write_mask rhs) {
    return static_cast<color_write_mask>(
        static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs)
        );
}

inline color_write_mask operator&(color_write_mask lhs, color_write_mask rhs) {
    return static_cast<color_write_mask>(
        static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs)
        );
}

struct blend_target_desc {
    bool         blend_enable     = false;
    blend_factor src_color_factor = blend_factor::one;
    blend_factor dst_color_factor = blend_factor::zero;
    blend_op     color_op         = blend_op::add;

    blend_factor src_alpha_factor = blend_factor::one;
    blend_factor dst_alpha_factor = blend_factor::zero;
    blend_op     alpha_op         = blend_op::add;

    color_write_mask write_mask = color_write_mask::all;
};

struct blendstate_desc {
    bool alpha_to_coverage_enable = false;
    bool independent_blend_enable = false;

    blend_target_desc targets[8];
};

class blendstate : public object<blendstate_desc> {
protected:
    using object<blendstate_desc>::object;
};

r2_end_