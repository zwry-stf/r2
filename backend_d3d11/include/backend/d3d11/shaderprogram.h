#pragma once
#include <backend/shaderprogram.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>


r2_begin_

class d3d11_shaderprogram : public shaderprogram,
                            protected d3d11_object {
private:
    d3d_pointer<ID3D11VertexShader> vs_;
    d3d_pointer<ID3D11PixelShader> ps_;

public:
    d3d11_shaderprogram(d3d11_context* ctx, class d3d11_vertexshader* vs, class d3d11_pixelshader* ps);
    ~d3d11_shaderprogram();

public:
    virtual void bind() const override;

    [[nodiscard]] auto* vs() const noexcept {
        return vs_.get();
    }

    [[nodiscard]] auto* ps() const noexcept {
        return ps_.get();
    }
};

r2_end_