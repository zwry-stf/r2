#include <backend/d3d11/compiled_shader.h>
#include <backend/d3d11/context.h>
#include <assert.h>


r2_begin_

d3d11_compiled_shader::d3d11_compiled_shader(d3d11_context* ctx, const char* source, std::size_t length, const char* version)
    : r2::compiled_shader(),
      d3d11_object(ctx)
{
    UINT compile_flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#if defined(_DEBUG)
    d3d_pointer<ID3DBlob> error_blob;
#endif

    HRESULT hr = D3DCompile(
        source,
        length,
        nullptr,
        nullptr,
        nullptr,
        "main",
        version,
        compile_flags,
        0,
        shader_blob_.address_of(),
#if defined(_DEBUG)
        error_blob.address_of()
#else
        nullptr
#endif
    );
    if (FAILED(hr)) {
#if defined(_DEBUG)
        char* err = reinterpret_cast<char*>(error_blob->GetBufferPointer());
        assert(false && "failed to compile shader");
        (void)err;
#endif

        set_error(
            0,
            hr
        );
    }
}

d3d11_compiled_shader::~d3d11_compiled_shader()
{
    shader_blob_.reset();
}

const void* d3d11_compiled_shader::data() const noexcept
{
    assert(shader_blob_);

    return shader_blob_->GetBufferPointer();
}

std::size_t d3d11_compiled_shader::size() const noexcept
{
    assert(shader_blob_);

    return shader_blob_->GetBufferSize();
}

r2_end_