#include <backend/d3d11/rasterizerstate.h>
#include <backend/d3d11/context.h>
#include <assert.h>


r2_begin_

D3D11_FILL_MODE d3d11_rasterizerstate::to_d3d11_fill(fill_mode m) noexcept
{
    switch (m) {
    case fill_mode::wireframe: return D3D11_FILL_WIREFRAME;
    case fill_mode::solid:     return D3D11_FILL_SOLID;
    default:
        assert(false);
        return {};
    }
}

D3D11_CULL_MODE d3d11_rasterizerstate::to_d3d11_cull(cull_mode m) noexcept
{
    switch (m) {
    case cull_mode::none:  return D3D11_CULL_NONE;
    case cull_mode::front: return D3D11_CULL_FRONT;
    case cull_mode::back:  return D3D11_CULL_BACK;
    default:
        assert(false);
        return {};
    }
}

d3d11_rasterizerstate::d3d11_rasterizerstate(d3d11_context* ctx, const rasterizerstate_desc& desc)
    : r2::rasterizerstate(desc),
      d3d11_object(ctx)
{
    D3D11_RASTERIZER_DESC d{};

    d.FillMode              = to_d3d11_fill(desc.fill);
    d.CullMode              = to_d3d11_cull(desc.cull);
    d.FrontCounterClockwise = desc.front_ccw;
    d.DepthBias             = desc.depth_bias;
    d.DepthBiasClamp        = desc.depth_bias_clamp;
    d.SlopeScaledDepthBias  = desc.slope_scaled_bias;
    d.DepthClipEnable       = desc.depth_clip_enable;
    d.ScissorEnable         = desc.scissor_enable;
    d.MultisampleEnable     = desc.multisample_enable;
    d.AntialiasedLineEnable = desc.antialiased_lines;

    HRESULT hr = ctx->get_device()->CreateRasterizerState(
        &d,
        state_.address_of()
    );
    if (FAILED(hr)) {
        set_error(
            std::to_underlying(d3d11_rasterizerstate_error::state_creation),
            hr
        );
    }
}

d3d11_rasterizerstate::~d3d11_rasterizerstate()
{
    state_.reset();
}

r2_end_