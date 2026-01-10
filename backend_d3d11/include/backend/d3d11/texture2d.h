#pragma once
#include <backend/texture2d.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>
#include <memory>


r2_begin_

enum class d3d11_texture2d_error : std::int32_t {
    texture_creation
};

class d3d11_texture2d : public texture2d,
                        protected d3d11_object {
private:
    d3d_pointer<ID3D11Texture2D> texture_;

public:
    d3d11_texture2d(d3d11_context* ctx, const texture_desc& desc, const void* data = nullptr);
    d3d11_texture2d(d3d11_context* ctx, const texture_desc& desc, ID3D11Texture2D* tex);
    ~d3d11_texture2d();

    static std::unique_ptr<d3d11_texture2d> from_existing(d3d11_context* ctx, ID3D11Texture2D* tex, bool is_backbuffer);

public:
    virtual void update(const void* data, std::uint32_t row_pitch) override;

    [[nodiscard]] auto* texture() const noexcept {
        return texture_.get();
    }

public:
    static [[nodiscard]] DXGI_FORMAT to_dxgi_format(texture_format fmt) noexcept;
    static [[nodiscard]] std::uint32_t bytes_per_pixel(texture_format fmt) noexcept;
};


r2_end_