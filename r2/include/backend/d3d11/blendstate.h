#pragma once
#include <backend/blendstate.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>


r2_begin_

enum class d3d11_blendstate_error : std::int32_t {
    blendstate_creation,
};

class d3d11_blendstate : public blendstate, 
                         protected d3d11_object {
private:
    d3d_pointer<ID3D11BlendState> blend_state_;

public:
    d3d11_blendstate(d3d11_context* ctx, const blendstate_desc& desc);
    ~d3d11_blendstate();

public:
    [[nodiscard]] auto* state() const noexcept { 
        return blend_state_.get(); 
    }

public:
    static [[nodiscard]] D3D11_BLEND to_d3d11_blend(blend_factor f) noexcept;
    static [[nodiscard]] D3D11_BLEND_OP to_d3d11_op(blend_op op) noexcept;
    static [[nodiscard]] UINT8 to_d3d11_write_mask(color_write_mask m) noexcept;
};

r2_end_