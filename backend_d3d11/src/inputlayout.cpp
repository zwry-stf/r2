#include <backend/d3d11/inputlayout.h>
#include <backend/d3d11/context.h>
#include <assert.h>
#include <vector>


r2_begin_

DXGI_FORMAT d3d11_inputlayout::to_d3d11_format(vertex_attribute_format format) noexcept
{
    switch (format) {
    case vertex_attribute_format::i32:              return DXGI_FORMAT_R32_SINT;
    case vertex_attribute_format::i32i32:           return DXGI_FORMAT_R32G32_SINT;
    case vertex_attribute_format::i32i32i32:        return DXGI_FORMAT_R32G32B32_SINT;
    case vertex_attribute_format::i32i32i32i32:     return DXGI_FORMAT_R32G32B32A32_SINT;

    case vertex_attribute_format::u32:              return DXGI_FORMAT_R32_UINT;
    case vertex_attribute_format::u32u32:           return DXGI_FORMAT_R32G32_UINT;
    case vertex_attribute_format::u32u32u32:        return DXGI_FORMAT_R32G32B32_UINT;
    case vertex_attribute_format::u32u32u32u32:     return DXGI_FORMAT_R32G32B32A32_UINT;

    case vertex_attribute_format::f32:              return DXGI_FORMAT_R32_FLOAT;
    case vertex_attribute_format::f32f32:           return DXGI_FORMAT_R32G32_FLOAT;
    case vertex_attribute_format::f32f32f32:        return DXGI_FORMAT_R32G32B32_FLOAT;
    case vertex_attribute_format::f32f32f32f32:     return DXGI_FORMAT_R32G32B32A32_FLOAT;

    case vertex_attribute_format::i16:              return DXGI_FORMAT_R16_SINT;
    case vertex_attribute_format::i16i16:           return DXGI_FORMAT_R16G16_SINT;
    case vertex_attribute_format::i16i16i16i16:     return DXGI_FORMAT_R16G16B16A16_SINT;

    case vertex_attribute_format::u16:              return DXGI_FORMAT_R16_UINT;
    case vertex_attribute_format::u16u16:           return DXGI_FORMAT_R16G16_UINT;
    case vertex_attribute_format::u16u16u16u16:     return DXGI_FORMAT_R16G16B16A16_UINT;

    case vertex_attribute_format::f16:              return DXGI_FORMAT_R16_FLOAT;
    case vertex_attribute_format::f16f16:           return DXGI_FORMAT_R16G16_FLOAT;
    case vertex_attribute_format::f16f16f16f16:     return DXGI_FORMAT_R16G16B16A16_FLOAT;

    case vertex_attribute_format::i8:               return DXGI_FORMAT_R8_SINT;
    case vertex_attribute_format::i8i8:             return DXGI_FORMAT_R8G8_SINT;
    case vertex_attribute_format::i8i8i8i8:         return DXGI_FORMAT_R8G8B8A8_SINT;

    case vertex_attribute_format::u8:               return DXGI_FORMAT_R8_UINT;
    case vertex_attribute_format::u8u8:             return DXGI_FORMAT_R8G8_UINT;
    case vertex_attribute_format::u8u8u8u8:         return DXGI_FORMAT_R8G8B8A8_UINT;

    case vertex_attribute_format::r8_unorm:         return DXGI_FORMAT_R8_UNORM;
    case vertex_attribute_format::r8r8_unorm:       return DXGI_FORMAT_R8G8_UNORM;
    case vertex_attribute_format::r8r8r8r8_unorm:   return DXGI_FORMAT_R8G8B8A8_UNORM;
    default:
        assert(false);
        return {};
    }
}

d3d11_inputlayout::d3d11_inputlayout(d3d11_context* ctx, const vertex_attribute_desc* desc, std::uint32_t count, 
                                     const std::uint8_t* vs_data, std::size_t vs_data_size)
    : r2::inputlayout(),
      d3d11_object(ctx)
{
    assert(desc != nullptr && count > 0ull);

    std::vector<D3D11_INPUT_ELEMENT_DESC> layout_desc;
    layout_desc.resize(count);

    for (std::uint32_t i = 0u; i < count; i++) {
        const auto& _desc = desc[i];

        layout_desc[i].SemanticName = _desc.semantic_name;
        layout_desc[i].Format = to_d3d11_format(_desc.format);
        layout_desc[i].AlignedByteOffset = _desc.aligned_byte_offset;
        layout_desc[i].InputSlotClass = _desc.per_instance ?
            D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
        layout_desc[i].InstanceDataStepRate = _desc.instance_data_step_rate;

        layout_desc[i].SemanticIndex = 0;
        layout_desc[i].InputSlot = 0;
    }

    // create
    HRESULT hr = ctx->get_device()->CreateInputLayout(
        layout_desc.data(), count,
        vs_data, vs_data_size,
        layout_.address_of()
    );

    if (FAILED(hr)) {
        set_error(
            std::to_underlying(d3d11_inputlayout_error::inputlayout_creation),
            hr
        );
    }
}

d3d11_inputlayout::~d3d11_inputlayout()
{
    layout_.reset();
}

void d3d11_inputlayout::bind() const
{
    assert(layout_);

    context()->get_context()->IASetInputLayout(layout_);
}

r2_end_