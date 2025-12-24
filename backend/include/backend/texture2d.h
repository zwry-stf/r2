#pragma once
#include <backend/object.h>
#include <utility>


r2_begin_

enum class texture_format : std::uint8_t {
    unknown = 0,
    rgba8_unorm,
    bgra8_unorm,
    r8_unorm,
    r16_float,
    r32_float,
    d24s8,
    d32_float,
    backbuffer,
};

enum class texture_usage : std::uint8_t {
    none = 0u,
    shader_resource = 1u << 0u,
    render_target   = 1u << 1u,
    depth_stencil   = 1u << 2u,
};

inline texture_usage operator|(const texture_usage& a, const texture_usage& b) {
    return static_cast<texture_usage>(
        std::to_underlying(a) | std::to_underlying(b)
        );
}

inline texture_usage operator&(const texture_usage& a, const texture_usage& b) {
    return static_cast<texture_usage>(
        std::to_underlying(a) & std::to_underlying(b)
        );
}

struct texture_desc
{
    std::uint32_t width  = 0u;
    std::uint32_t height = 0u;
    std::uint32_t mip_levels = 1u;
    texture_usage usage = texture_usage::shader_resource;
    texture_format format = texture_format::rgba8_unorm;

    struct
    {
        std::uint32_t count = 1;
        std::uint32_t quality = 0;
    } sample_desc;
};

class texture2d : public object<texture_desc> {
protected:
    using object<texture_desc>::object;

public:
    virtual void update(const void* data, std::uint32_t row_pitch) = 0;

    [[nodiscard]] auto width()  const noexcept { return desc_.width; }
    [[nodiscard]] auto height() const noexcept { return desc_.height; }
    [[nodiscard]] auto format() const noexcept { return desc_.format; }
};

r2_end_