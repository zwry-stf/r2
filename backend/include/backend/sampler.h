#pragma once
#include <backend/object.h>


r2_begin_

enum class sampler_filter : std::uint8_t {
    nearest,
    linear,
    anisotropic 
};

enum class sampler_address_mode : std::uint8_t {
    repeat,
    clamp_to_edge,
    clamp_to_border,
    mirror,
};

enum class sampler_compare_func : std::uint8_t {
    none,
    less_equal,
    greater_equal,
    less,
    greater,
    equal,
    not_equal,
    always,
    never
};

struct sampler_desc {
    sampler_filter       filter    = sampler_filter::linear;
    sampler_address_mode address_u = sampler_address_mode::repeat;
    sampler_address_mode address_v = sampler_address_mode::repeat;
    sampler_address_mode address_w = sampler_address_mode::repeat;

    sampler_compare_func compare_func = sampler_compare_func::none;

    std::uint32_t max_anisotropy = 1u;

    float border_color[4] = { 0.f, 0.f, 0.f, 0.f };
};

class sampler : public object<sampler_desc> {
protected:
    using object<sampler_desc>::object;
};

r2_end_