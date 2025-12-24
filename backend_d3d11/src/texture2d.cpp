#include <backend/d3d11/texture2d.h>
#include <backend/d3d11/context.h>
#include <assert.h>


r2_begin_


DXGI_FORMAT d3d11_texture2d::to_dxgi_format(texture_format fmt) noexcept
{
    switch (fmt) {
    case texture_format::rgba8_unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case texture_format::bgra8_unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case texture_format::r16_float:   return DXGI_FORMAT_R16_FLOAT;
    case texture_format::r32_float:   return DXGI_FORMAT_R32_FLOAT;
    case texture_format::r8_unorm:    return DXGI_FORMAT_R8_UNORM;

    case texture_format::d24s8:       return DXGI_FORMAT_R24G8_TYPELESS;
    case texture_format::d32_float:   return DXGI_FORMAT_R32_TYPELESS;
    case texture_format::backbuffer: assert(false);
    default:
        assert(false);
        return {};
    }
}

std::uint32_t d3d11_texture2d::bytes_per_pixel(texture_format fmt) noexcept
{
    switch (fmt) {
    case texture_format::rgba8_unorm:
    case texture_format::bgra8_unorm:
        return 4u;
    case texture_format::r8_unorm:
        return 1u;
    case texture_format::r16_float:
        return 2u;
    case texture_format::r32_float:
        return 4u;
    case texture_format::d24s8:
        return 4u;
    case texture_format::d32_float:
        return 4u;
    default:
        assert(false);
        return {};
    }
}

d3d11_texture2d::d3d11_texture2d(d3d11_context* ctx, const texture_desc& desc, ID3D11Texture2D* tex)
    : r2::texture2d(desc),
      d3d11_object(ctx)
{
    texture_.reset(tex);
    tex->AddRef();
}

d3d11_texture2d::d3d11_texture2d(d3d11_context* ctx, const texture_desc& desc, const void* data)
    : r2::texture2d(desc),
      d3d11_object(ctx)
{
    assert(desc.width > 0u && desc.height > 0u);

    assert(desc.sample_desc.count <= 1u ||
        desc.mip_levels == 1u);

    D3D11_TEXTURE2D_DESC d{};

    d.Width     = desc.width;
    d.Height    = desc.height;
    d.MipLevels = desc.mip_levels;
    d.ArraySize = 1;
    if (desc.format == texture_format::backbuffer) {
        d.Format = ctx->get_backbuffer_format_no_srgb();
    }
    else {
        d.Format = to_dxgi_format(desc.format);
    }
    d.SampleDesc.Count   = desc.sample_desc.count;
    d.SampleDesc.Quality = desc.sample_desc.quality;
    d.Usage = D3D11_USAGE_DEFAULT;

    // bind flags
    if ((desc.usage & texture_usage::shader_resource) != texture_usage::none)
        d.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    if ((desc.usage & texture_usage::render_target) != texture_usage::none)
        d.BindFlags |= D3D11_BIND_RENDER_TARGET;
    if ((desc.usage & texture_usage::depth_stencil) != texture_usage::none)
        d.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    const bool has_init_data = (data != nullptr);
    D3D11_SUBRESOURCE_DATA sdata{};
    D3D11_SUBRESOURCE_DATA* p_init = nullptr;

    if (has_init_data) {
        sdata.pSysMem = data;

        const std::uint32_t bpp = bytes_per_pixel(desc.format);
        sdata.SysMemPitch = desc.width * bpp;
        p_init = &sdata;
    }

    HRESULT hr = ctx->get_device()->CreateTexture2D(
        &d,
        p_init, 
        texture_.address_of()
    );
    if (FAILED(hr)) {
        set_error(
            std::to_underlying(d3d11_texture2d_error::texture_creation),
            hr
        );
    }
}

d3d11_texture2d::~d3d11_texture2d()
{
    texture_.reset();
}

std::unique_ptr<d3d11_texture2d> d3d11_texture2d::from_existing(d3d11_context* ctx, ID3D11Texture2D* tex)
{
    assert(tex != nullptr);

    D3D11_TEXTURE2D_DESC sd{};
    tex->GetDesc(&sd);

    texture_desc d{};
    d.width = static_cast<std::uint32_t>(sd.Width);
    d.height = static_cast<std::uint32_t>(sd.Height);
    switch (sd.Format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:  d.format = texture_format::rgba8_unorm; break;
    case DXGI_FORMAT_B8G8R8A8_UNORM:  d.format = texture_format::bgra8_unorm; break;
    case DXGI_FORMAT_R16_FLOAT:       d.format = texture_format::r16_float; break;
    case DXGI_FORMAT_R32_FLOAT:       d.format = texture_format::r32_float; break;
    case DXGI_FORMAT_R24G8_TYPELESS:  d.format = texture_format::d24s8; break;
    case DXGI_FORMAT_R32_TYPELESS:    d.format = texture_format::d32_float; break;
    default: d.format = texture_format::unknown; break;
    }

    d.mip_levels = static_cast<std::uint32_t>(sd.MipLevels);
    d.sample_desc.count = static_cast<std::uint32_t>(sd.SampleDesc.Count);
    d.sample_desc.quality = static_cast<std::uint32_t>(sd.SampleDesc.Quality);

    if (sd.BindFlags & D3D11_BIND_SHADER_RESOURCE)
        d.usage = d.usage | texture_usage::shader_resource;
    if (sd.BindFlags & D3D11_BIND_RENDER_TARGET)
        d.usage = d.usage | texture_usage::render_target;
    if (sd.BindFlags & D3D11_BIND_DEPTH_STENCIL)
        d.usage = d.usage | texture_usage::depth_stencil;

    return std::make_unique<d3d11_texture2d>(ctx, d, tex);
}

void d3d11_texture2d::update(const void* data, std::uint32_t row_pitch)
{
    context()->get_context()->UpdateSubresource(
        texture_.get(),
        0,
        nullptr,
        data,
        static_cast<UINT>(row_pitch),
        0 
    );
}

r2_end_