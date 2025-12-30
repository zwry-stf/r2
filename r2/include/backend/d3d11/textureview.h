#pragma once
#include <backend/textureview.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>


r2_begin_

enum class d3d11_textureview_error : std::int32_t {
    srv_creation,
    rtv_creation,
    dsv_creation
};

class d3d11_textureview : public textureview,
                          protected d3d11_object {
private:
    class d3d11_texture2d* const resource_;

    d3d_pointer<ID3D11ShaderResourceView> srv_;
    d3d_pointer<ID3D11RenderTargetView>   rtv_;
    d3d_pointer<ID3D11DepthStencilView>   dsv_;

public:
    d3d11_textureview(d3d11_context* ctx, class d3d11_texture2d* tex, const textureview_desc& desc);
    ~d3d11_textureview();

public:
    virtual void* native_texture_handle() const noexcept override {
        return reinterpret_cast<void*>(srv_.get());
    }

    [[nodiscard]] auto* resource() const noexcept {
        return resource_;
    }

    [[nodiscard]] auto* srv() const noexcept { 
        return srv_.get();
    }
    [[nodiscard]] auto* rtv() const noexcept {
        return rtv_.get();
    }
    [[nodiscard]] auto* dsv() const noexcept { 
        return dsv_.get(); 
    }

public:
    static [[nodiscard]] DXGI_FORMAT to_dxgi_format_srv(texture_format fmt) noexcept;
    static [[nodiscard]] DXGI_FORMAT to_dxgi_format_dsv(texture_format fmt) noexcept;
};

r2_end_