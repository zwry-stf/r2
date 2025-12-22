#include <backend/d3d11/sampler.h>
#include <backend/d3d11/context.h>
#include <assert.h>


r2_begin_

D3D11_FILTER d3d11_sampler::to_d3d11_filter(const sampler_desc& desc) noexcept
{
    const bool is_compare = (desc.compare_func != sampler_compare_func::none);

    switch (desc.filter) {
    case sampler_filter::nearest:
        return is_compare ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT
            : D3D11_FILTER_MIN_MAG_MIP_POINT;
    case sampler_filter::linear:
        return is_compare ? D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR
            : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    case sampler_filter::anisotropic:
        return is_compare ? D3D11_FILTER_COMPARISON_ANISOTROPIC
            : D3D11_FILTER_ANISOTROPIC;
    default:
        assert(false);
        return {};
    }
}

D3D11_TEXTURE_ADDRESS_MODE d3d11_sampler::to_d3d11_address(sampler_address_mode m) noexcept
{
    switch (m) {
    case sampler_address_mode::repeat:          return D3D11_TEXTURE_ADDRESS_WRAP;
    case sampler_address_mode::clamp_to_edge:   return D3D11_TEXTURE_ADDRESS_CLAMP;
    case sampler_address_mode::clamp_to_border: return D3D11_TEXTURE_ADDRESS_BORDER;
    case sampler_address_mode::mirror:          return D3D11_TEXTURE_ADDRESS_MIRROR;
    default:
        assert(false);
        return {};
    }
}

D3D11_COMPARISON_FUNC d3d11_sampler::to_d3d11_compare(sampler_compare_func f) noexcept
{
    switch (f) {
    case sampler_compare_func::less_equal:    return D3D11_COMPARISON_LESS_EQUAL;
    case sampler_compare_func::greater_equal: return D3D11_COMPARISON_GREATER_EQUAL;
    case sampler_compare_func::less:          return D3D11_COMPARISON_LESS;
    case sampler_compare_func::greater:       return D3D11_COMPARISON_GREATER;
    case sampler_compare_func::equal:         return D3D11_COMPARISON_EQUAL;
    case sampler_compare_func::not_equal:     return D3D11_COMPARISON_NOT_EQUAL;
    case sampler_compare_func::always:        return D3D11_COMPARISON_ALWAYS;
    case sampler_compare_func::never:         return D3D11_COMPARISON_NEVER;
    case sampler_compare_func::none:          return D3D11_COMPARISON_ALWAYS;
    default:
        assert(false);
        return {};
    }
}

d3d11_sampler::d3d11_sampler(d3d11_context* ctx, const sampler_desc& desc)
    : r2::sampler(desc),
      d3d11_object(ctx)
{
    D3D11_SAMPLER_DESC d{};
    d.Filter   = to_d3d11_filter(desc);
    d.AddressU = to_d3d11_address(desc.address_u);
    d.AddressV = to_d3d11_address(desc.address_v);
    d.AddressW = to_d3d11_address(desc.address_w);
    d.MipLODBias = 0.f;
    d.MinLOD = 0.f;
    d.MaxLOD = 0.f;
    d.MaxAnisotropy = desc.max_anisotropy;
    d.ComparisonFunc = to_d3d11_compare(desc.compare_func);

    d.BorderColor[0] = desc.border_color[0];
    d.BorderColor[1] = desc.border_color[1];
    d.BorderColor[2] = desc.border_color[2];
    d.BorderColor[3] = desc.border_color[3];

    HRESULT hr = ctx->get_device()->CreateSamplerState(
        &d, sampler_.address_of()
    );
    if (FAILED(hr)) {
        set_error(
            std::to_underlying(d3d11_sampler_error::sampler_creation),
            hr
        );
    }
}

d3d11_sampler::~d3d11_sampler()
{
    sampler_.reset();
}

r2_end_