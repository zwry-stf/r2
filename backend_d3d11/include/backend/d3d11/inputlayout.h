#pragma once
#include <backend/inputlayout.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>
#include <cstddef>


r2_begin_

enum class d3d11_inputlayout_error : std::int32_t {
    inputlayout_creation
};

class d3d11_inputlayout : public inputlayout,
                          protected d3d11_object {
private:
    d3d_pointer<ID3D11InputLayout> layout_;

public:
    d3d11_inputlayout(d3d11_context* ctx, const vertex_attribute_desc* desc, std::uint32_t count,
                      const std::uint8_t* vs_data, std::size_t vs_data_size);
    ~d3d11_inputlayout();

public:
    virtual void bind() const override;

    [[nodiscard]] auto* layout() const noexcept {
        return layout_.get(); 
    }

public:
    static [[nodiscard]] DXGI_FORMAT to_d3d11_format(vertex_attribute_format format) noexcept;
};

r2_end_