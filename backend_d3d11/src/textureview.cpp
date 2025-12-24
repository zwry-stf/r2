#include <backend/d3d11/textureview.h>
#include <backend/d3d11/context.h>
#include <backend/d3d11/texture2d.h>
#include <assert.h>


r2_begin_

DXGI_FORMAT d3d11_textureview::to_dxgi_format_srv(texture_format fmt) noexcept
{
    switch (fmt) {
    case texture_format::d24s8:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case texture_format::d32_float:
        return DXGI_FORMAT_R32_FLOAT;
    default:
        return d3d11_texture2d::to_dxgi_format(fmt);
    }
}

DXGI_FORMAT d3d11_textureview::to_dxgi_format_dsv(texture_format fmt) noexcept
{
    switch (fmt) {
    case texture_format::d24s8:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case texture_format::d32_float:
        return DXGI_FORMAT_D32_FLOAT;
    default:
        return d3d11_texture2d::to_dxgi_format(fmt);
    }
}

d3d11_textureview::d3d11_textureview(d3d11_context* ctx, d3d11_texture2d* tex, const textureview_desc& desc)
    : r2::textureview(desc),
      d3d11_object(ctx),
      resource_(tex)
{
    assert(tex != nullptr);

    const auto& td = resource_->desc();
    const bool msaa = td.sample_desc.count > 1;

    const texture_format fmt =
        (desc_.format_override == texture_format::unknown) ?
            td.format : desc_.format_override;

    if (desc_.usage == view_usage::shader_resource) {
        assert((td.usage & texture_usage::shader_resource) != texture_usage::none);

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        if (fmt == texture_format::backbuffer) {
            sd.Format = ctx->get_backbuffer_format_no_srgb();
        }
        else {
            sd.Format = to_dxgi_format_srv(fmt);
        }
        sd.ViewDimension = msaa ?
            D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;

        if (sd.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D) {
            sd.Texture2D.MostDetailedMip = desc_.range.base_mip;
            sd.Texture2D.MipLevels = desc_.range.mip_count;
        }

        HRESULT hr = ctx->get_device()->CreateShaderResourceView(
            tex->texture(),
            &sd, 
            srv_.address_of()
        );
        if (FAILED(hr)) {
            set_error(
                std::to_underlying(d3d11_textureview_error::srv_creation),
                hr
            );
            return;
        }
    }

    else if (desc_.usage == view_usage::render_target) {
        assert((td.usage & texture_usage::render_target) != texture_usage::none);

        D3D11_RENDER_TARGET_VIEW_DESC rd{};
        if (fmt == texture_format::backbuffer) {
            rd.Format = ctx->get_backbuffer_format_no_srgb();
        }
        else {
            rd.Format = d3d11_texture2d::to_dxgi_format(fmt);
        }
        rd.ViewDimension = msaa ?
            D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;

        if (rd.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D) {
            rd.Texture2D.MipSlice = desc_.range.base_mip;
        }

        HRESULT hr = ctx->get_device()->CreateRenderTargetView(
            tex->texture(),
            &rd,
            rtv_.address_of()
        );
        if (FAILED(hr)) {
            set_error(
                std::to_underlying(d3d11_textureview_error::rtv_creation),
                hr
            );
            return;
        }
    }

    else if (desc_.usage == view_usage::depth_stencil) {
        assert((td.usage & texture_usage::depth_stencil) != texture_usage::none);

        D3D11_DEPTH_STENCIL_VIEW_DESC dd{};
        assert(fmt != texture_format::backbuffer);
        dd.Format = to_dxgi_format_dsv(fmt);
        dd.ViewDimension = msaa ?
            D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
        dd.Flags = 0;

        if (dd.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D) {
            dd.Texture2D.MipSlice = desc_.range.base_mip;
        }

        HRESULT hr = ctx->get_device()->CreateDepthStencilView(
            tex->texture(),
            &dd, 
            dsv_.address_of()
        );
        if (FAILED(hr)) {
            set_error(
                std::to_underlying(d3d11_textureview_error::dsv_creation),
                hr
            );
            return;
        }
    }
}

d3d11_textureview::~d3d11_textureview()
{
    srv_.reset();
    rtv_.reset();
    dsv_.reset();
}

r2_end_