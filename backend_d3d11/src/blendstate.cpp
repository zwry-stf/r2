#include <backend/d3d11/blendstate.h>
#include <backend/d3d11/context.h>
#include <backend/util.h>
#include <assert.h>


r2_begin_

D3D11_BLEND d3d11_blendstate::to_d3d11_blend(blend_factor f) noexcept 
{
    switch (f)
    {
    case blend_factor::zero:              return D3D11_BLEND_ZERO;
    case blend_factor::one:               return D3D11_BLEND_ONE;
    case blend_factor::src_color:         return D3D11_BLEND_SRC_COLOR;
    case blend_factor::inv_src_color:     return D3D11_BLEND_INV_SRC_COLOR;
    case blend_factor::src_alpha:         return D3D11_BLEND_SRC_ALPHA;
    case blend_factor::inv_src_alpha:     return D3D11_BLEND_INV_SRC_ALPHA;
    case blend_factor::dest_alpha:        return D3D11_BLEND_DEST_ALPHA;
    case blend_factor::inv_dest_alpha:    return D3D11_BLEND_INV_DEST_ALPHA;
    case blend_factor::dest_color:        return D3D11_BLEND_DEST_COLOR;
    case blend_factor::inv_dest_color:    return D3D11_BLEND_INV_DEST_COLOR;
    case blend_factor::src_alpha_sat:     return D3D11_BLEND_SRC_ALPHA_SAT;
    case blend_factor::blend_factor:      return D3D11_BLEND_BLEND_FACTOR;
    case blend_factor::inv_blend_factor:  return D3D11_BLEND_INV_BLEND_FACTOR;
    default:
        assert(false);
        return {};
    }
}

D3D11_BLEND_OP d3d11_blendstate::to_d3d11_op(blend_op op) noexcept 
{
    switch (op)
    {
    case blend_op::add:          return D3D11_BLEND_OP_ADD;
    case blend_op::subtract:     return D3D11_BLEND_OP_SUBTRACT;
    case blend_op::rev_subtract: return D3D11_BLEND_OP_REV_SUBTRACT;
    case blend_op::min:          return D3D11_BLEND_OP_MIN;
    case blend_op::max:          return D3D11_BLEND_OP_MAX;
    default:
        assert(false);
        return {};
    }
}

UINT8 d3d11_blendstate::to_d3d11_write_mask(color_write_mask m) noexcept 
{
    return static_cast<UINT8>(m);
}

d3d11_blendstate::d3d11_blendstate(d3d11_context* ctx, const blendstate_desc& desc)
    : r2::blendstate(desc),
      d3d11_object(ctx)
{
    D3D11_BLEND_DESC d{};
    d.AlphaToCoverageEnable = false;
    d.IndependentBlendEnable = desc.independent_blend_enable;

    const std::uint32_t rt_count = desc.independent_blend_enable ? 
        v_count_of(desc.targets) : 1u;

    for (std::uint32_t i = 0u; i < rt_count; i++) {
        const auto& src = desc.targets[i];
        auto& dst       = d.RenderTarget[i];

        dst.BlendEnable           = src.blend_enable;
        dst.SrcBlend              = to_d3d11_blend(src.src_color_factor);
        dst.DestBlend             = to_d3d11_blend(src.dst_color_factor);
        dst.BlendOp               = to_d3d11_op(src.color_op);
        dst.SrcBlendAlpha         = to_d3d11_blend(src.src_alpha_factor);
        dst.DestBlendAlpha        = to_d3d11_blend(src.dst_alpha_factor);
        dst.BlendOpAlpha          = to_d3d11_op(src.alpha_op);
        dst.RenderTargetWriteMask = to_d3d11_write_mask(src.write_mask);
    }

    HRESULT hr = ctx->get_device()->CreateBlendState(
        &d, 
        blend_state_.address_of()
    );
    if (FAILED(hr)) {
        set_error(
            std::to_underlying(d3d11_blendstate_error::blendstate_creation),
            hr
        );
    }
}

d3d11_blendstate::~d3d11_blendstate()
{
    blend_state_.reset();
}

void d3d11_blendstate::bind(const float(&factor)[4], std::uint32_t mask)
{
    assert(blend_state_);

    context()->get_context()->OMSetBlendState(
        blend_state_.get(),
        factor,
        mask
    );
}

r2_end_