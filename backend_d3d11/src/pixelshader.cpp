#include <backend/d3d11/pixelshader.h>
#include <backend/d3d11/context.h>
#include <assert.h>


r2_begin_

d3d11_pixelshader::d3d11_pixelshader(d3d11_context* ctx, const std::uint8_t* data, std::size_t data_size)
    : pixelshader(),
      d3d11_object(ctx)
{
    assert(data != nullptr && data_size > 0ull);

    HRESULT hr = ctx->get_device()->CreatePixelShader(
        data,
        data_size,
        nullptr, shader_.address_of()
    );
    if (FAILED(hr)) {
        set_error(
            std::to_underlying(d3d11_pixelshader_error::shader_creation),
            hr
        );
    }
}

d3d11_pixelshader::~d3d11_pixelshader()
{
    shader_.reset();
}

r2_end_