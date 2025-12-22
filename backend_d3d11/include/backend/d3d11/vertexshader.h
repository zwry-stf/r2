#pragma once
#include <backend/vertexshader.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>


r2_begin_

enum class d3d11_vertexshader_error : std::int32_t {
    shader_creation
};

class d3d11_vertexshader : public vertexshader,
                           protected d3d11_object {
private:
    d3d_pointer<ID3D11VertexShader> shader_;

public:
    d3d11_vertexshader(d3d11_context* ctx, const std::uint8_t* data, std::size_t data_size);
    ~d3d11_vertexshader();

public:
    [[nodiscard]] auto shader() const noexcept {
        return shader_.get();
    }
};

r2_end_