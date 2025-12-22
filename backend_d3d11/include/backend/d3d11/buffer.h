#pragma once
#include <backend/buffer.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>


r2_begin_

enum class d3d11_buffer_error : std::int32_t {
    creation_failed,
    update_failed
};

class d3d11_buffer : public buffer,
                     public d3d11_object {
private:
    d3d_pointer<ID3D11Buffer> buffer_;

public:
    d3d11_buffer(d3d11_context* ctx, const buffer_desc& desc, const void* data = nullptr);
    ~d3d11_buffer();

public:
    virtual void update(const void* data, std::size_t size) override;

    [[nodiscard]] auto* buffer() const noexcept { 
        return buffer_.get();
    }

public:
    static [[nodiscard]] UINT to_d3d11_usage(buffer_usage usage) noexcept;
};

r2_end_