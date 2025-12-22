#include <backend/d3d11/buffer.h>
#include <backend/d3d11/context.h>
#include <assert.h>


r2_begin_

UINT d3d11_buffer::to_d3d11_usage(buffer_usage usage) noexcept
{
    switch (usage) {
    case buffer_usage::vertex:
        return D3D11_BIND_VERTEX_BUFFER;
    case buffer_usage::index:
        return D3D11_BIND_INDEX_BUFFER;
    case buffer_usage::uniform:
        return D3D11_BIND_CONSTANT_BUFFER;
    default:
        assert(false);
        return {};
    }
}

d3d11_buffer::d3d11_buffer(d3d11_context* ctx, const buffer_desc& desc,
                           const void* data)
    : r2::buffer(desc), 
      d3d11_object(ctx)
{
    assert(desc.usage != buffer_usage::vertex || desc.vb_stride != 0u);

    D3D11_BUFFER_DESC d{};

    d.Usage = desc.dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    d.BindFlags = to_d3d11_usage(desc.usage);

    if (desc.usage == buffer_usage::uniform)
        assert((desc.size_bytes % 16) == 0 && 
            "constant buffer size must be multiple of 16");

    if (desc.dynamic)
        d.CPUAccessFlags |= D3D11_CPU_ACCESS_WRITE;

    d.ByteWidth = desc.size_bytes;

    // initial data
    D3D11_SUBRESOURCE_DATA initial_data;
    initial_data.pSysMem = data;

    // create buffer
    HRESULT hr = ctx->get_device()->CreateBuffer(
        &d,
        data != nullptr ? &initial_data : nullptr,
        buffer_.address_of()
    );

    if (FAILED(hr)) {
        set_error(
            std::to_underlying(d3d11_buffer_error::creation_failed),
            hr
        );
    }
}

d3d11_buffer::~d3d11_buffer()
{
    buffer_.reset();
}

void d3d11_buffer::update(const void* data, std::size_t size)
{
    assert(buffer_);
    assert(size <= desc_.size_bytes);
    assert(data != nullptr);
    assert(size > 0ull);

    if (desc_.dynamic) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = context()->get_context()->Map(buffer_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) {
            set_error(
                std::to_underlying(d3d11_buffer_error::update_failed),
                hr
            );
            return;
        }

        std::memcpy(mapped.pData, data, size);

        context()->get_context()->Unmap(buffer_, 0);
    }
    else {
        assert(size == desc_.size_bytes && "static buffer updates should match creation size");

        context()->get_context()->UpdateSubresource(buffer_, 0, nullptr, data, 0, 0);
    }
}

r2_end_