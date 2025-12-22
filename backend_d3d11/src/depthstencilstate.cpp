#include <backend/d3d11/depthstencilstate.h>
#include <backend/d3d11/context.h>
#include <assert.h>


r2_begin_

D3D11_COMPARISON_FUNC d3d11_depthstencilstate::to_d3d11_comp(comparison_func f) noexcept
{
    switch (f) {
    case comparison_func::never:         return D3D11_COMPARISON_NEVER;
    case comparison_func::less:          return D3D11_COMPARISON_LESS;
    case comparison_func::equal:         return D3D11_COMPARISON_EQUAL;
    case comparison_func::less_equal:    return D3D11_COMPARISON_LESS_EQUAL;
    case comparison_func::greater:       return D3D11_COMPARISON_GREATER;
    case comparison_func::not_equal:     return D3D11_COMPARISON_NOT_EQUAL;
    case comparison_func::greater_equal: return D3D11_COMPARISON_GREATER_EQUAL;
    case comparison_func::always:        return D3D11_COMPARISON_ALWAYS;
    default:
        assert(false);
        return {};
    }
}

D3D11_STENCIL_OP d3d11_depthstencilstate::to_d3d11_stencil(stencil_op op) noexcept
{
    switch (op) {
    case stencil_op::keep:     return D3D11_STENCIL_OP_KEEP;
    case stencil_op::zero:     return D3D11_STENCIL_OP_ZERO;
    case stencil_op::replace:  return D3D11_STENCIL_OP_REPLACE;
    case stencil_op::incr_sat: return D3D11_STENCIL_OP_INCR_SAT;
    case stencil_op::decr_sat: return D3D11_STENCIL_OP_DECR_SAT;
    case stencil_op::invert:   return D3D11_STENCIL_OP_INVERT;
    case stencil_op::incr:     return D3D11_STENCIL_OP_INCR;
    case stencil_op::decr:     return D3D11_STENCIL_OP_DECR;
    default:
        assert(false);
        return {};
    }
}

static void to_d3d11_face(D3D11_DEPTH_STENCILOP_DESC& dst, const depthstencil_op_desc& src) noexcept
{
    dst.StencilFailOp = d3d11_depthstencilstate::to_d3d11_stencil(src.fail_op);
    dst.StencilDepthFailOp = d3d11_depthstencilstate::to_d3d11_stencil(src.depth_fail_op);
    dst.StencilPassOp = d3d11_depthstencilstate::to_d3d11_stencil(src.pass_op);
    dst.StencilFunc = d3d11_depthstencilstate::to_d3d11_comp(src.func);
}

d3d11_depthstencilstate::d3d11_depthstencilstate(d3d11_context* ctx, const depthstencilstate_desc& desc)
    : r2::depthstencilstate(desc),
      d3d11_object(ctx)
{
    D3D11_DEPTH_STENCIL_DESC d{};
    d.DepthEnable    = desc.depth_enable;
    d.DepthWriteMask = desc.depth_write ? 
        D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    d.DepthFunc      = to_d3d11_comp(desc.depth_func);

    d.StencilEnable    = desc.stencil_enable;
    d.StencilReadMask  = desc.stencil_read_mask;
    d.StencilWriteMask = desc.stencil_write_mask;

    to_d3d11_face(d.FrontFace, desc.front_face);
    to_d3d11_face(d.BackFace,  desc.back_face);

    HRESULT hr = ctx->get_device()->CreateDepthStencilState(
        &d, 
        ds_state_.address_of()
    );

    if (FAILED(hr)) {
        set_error(
            std::to_underlying(d3d11_depthstencilstate_error::depthstencilstate_creation),
            hr
        );
    }
}

d3d11_depthstencilstate::~d3d11_depthstencilstate()
{
    ds_state_.reset();
}

void d3d11_depthstencilstate::bind(std::uint32_t stencil_ref)
{
    assert(ds_state_);

    context()->get_context()->OMSetDepthStencilState(
        ds_state_.get(),
        stencil_ref
    );
}

r2_end_