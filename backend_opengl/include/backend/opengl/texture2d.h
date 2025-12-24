#pragma once
#include <backend/texture2d.h>
#include <backend/opengl/object.h>
#include <assert.h>
#include <memory>


r2_begin_

enum class gl_texture2d_error : std::int32_t {
    texture_generation,
    texture_creation,
    texture_update
};

class gl_texture2d : public texture2d,
                     protected gl_object {
private:
    const bool backbuffer_handle_;
    GLuint texture_;

public:
    gl_texture2d(gl_context* ctx, const texture_desc& desc, const void* data = nullptr);
    gl_texture2d(gl_context* ctx, std::nullptr_t, const texture_desc& desc);
    ~gl_texture2d();

    static std::unique_ptr<gl_texture2d> from_backbuffer(gl_context* ctx);

public:
    virtual void update(const void* data, std::uint32_t row_pitch) override;

    [[nodiscard]] bool is_backbuffer_handle() const noexcept {
        return backbuffer_handle_;
    }
    [[nodiscard]] auto texture() const noexcept {
        assert(!is_backbuffer_handle());
        return texture_;
    }

public:
    static void to_gl_format(texture_format fmt, GLint& out_internal,
                             GLenum& out_format, GLenum& out_type) noexcept;
};

r2_end_