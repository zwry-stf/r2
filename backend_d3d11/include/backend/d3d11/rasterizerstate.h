#pragma once
#include <backend/rasterizerstate.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>


r2_begin_

enum class d3d11_rasterizerstate_error : std::int32_t {
    state_creation
};

class d3d11_rasterizerstate : public rasterizerstate,
                              protected d3d11_object {
private:
    d3d_pointer<ID3D11RasterizerState> state_;

public:
    d3d11_rasterizerstate(d3d11_context* ctx, const rasterizerstate_desc& desc);
    ~d3d11_rasterizerstate();

public:
    [[nodiscard]] auto* state() const noexcept { 
        return state_.get(); 
    }

public:
    static [[nodiscard]] D3D11_CULL_MODE to_d3d11_cull(cull_mode m) noexcept;
    static [[nodiscard]] D3D11_FILL_MODE to_d3d11_fill(fill_mode m) noexcept;
};

r2_end_