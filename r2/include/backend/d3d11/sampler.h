#pragma once
#include <backend/sampler.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>


r2_begin_

enum class d3d11_sampler_error : std::int32_t {
    sampler_creation
};

class d3d11_sampler : public sampler, 
                      protected d3d11_object {
private:
    d3d_pointer<ID3D11SamplerState> sampler_;

public:
    d3d11_sampler(d3d11_context* ctx, const sampler_desc& desc);
    ~d3d11_sampler();

public:
    [[nodiscard]] auto* sampler() const noexcept {
        return sampler_.get(); 
    }

public:
    static [[nodiscard]] D3D11_FILTER to_d3d11_filter(const sampler_desc& desc) noexcept;
    static [[nodiscard]] D3D11_TEXTURE_ADDRESS_MODE to_d3d11_address(sampler_address_mode m) noexcept;
    static [[nodiscard]] D3D11_COMPARISON_FUNC to_d3d11_compare(sampler_compare_func f) noexcept;
};

r2_end_