#pragma once
#include <backend/texture2d.h>


r2_begin_

enum class texture_aspect : std::uint8_t
{
    color,
    depth,
    stencil,
    depth_stencil
};

enum class view_usage : std::uint8_t
{
    shader_resource,
    render_target,
    depth_stencil
};

struct subresource_range
{
    std::uint16_t base_mip    = 0u;
    std::uint16_t mip_count   = 1u;
    std::uint16_t base_layer  = 0u;
    std::uint16_t layer_count = 1u;
};

struct textureview_desc
{
    view_usage usage = view_usage::shader_resource;
    texture_aspect aspect = texture_aspect::color;
    subresource_range range{};
    texture_format format_override = texture_format::unknown; // use texture format
};

class textureview : public object<textureview_desc> {
protected:
    using object<textureview_desc>::object;

public:
    virtual void* native_texture_handle() const noexcept = 0;
};

r2_end_