#pragma once
#include <backend/depthstencilstate.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>


r2_begin_

enum class d3d11_depthstencilstate_error : std::int32_t {
    depthstencilstate_creation
};

class d3d11_depthstencilstate : public depthstencilstate,
                                protected d3d11_object {
private:
    d3d_pointer<ID3D11DepthStencilState> ds_state_;

public:
    d3d11_depthstencilstate(d3d11_context* ctx, const depthstencilstate_desc& desc);
    ~d3d11_depthstencilstate();

public:
    [[nodiscard]] auto* state() const noexcept {
        return ds_state_.get();
    }

public:
    static [[nodiscard]] D3D11_COMPARISON_FUNC to_d3d11_comp(comparison_func f) noexcept;
    static [[nodiscard]] D3D11_STENCIL_OP to_d3d11_stencil(stencil_op op) noexcept;
};

r2_end_