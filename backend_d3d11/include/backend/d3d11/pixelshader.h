#pragma once
#include <backend/pixelshader.h>
#include <backend/d3d11/compiled_shader.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>
#include <cstddef>


r2_begin_

enum class d3d11_pixelshader_error : std::int32_t {
    shader_creation,
};

class d3d11_pixelshader : public pixelshader,
                          protected d3d11_object {
private:
    d3d_pointer<ID3D11PixelShader> shader_;

public:
    d3d11_pixelshader(d3d11_context* ctx, const std::uint8_t* data, std::size_t data_size);
    ~d3d11_pixelshader();

public:
    [[nodiscard]] auto* shader() const noexcept {
        return shader_.get();
    }
};

r2_end_