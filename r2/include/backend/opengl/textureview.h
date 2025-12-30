#pragma once
#include <backend/textureview.h>
#include <backend/opengl/object.h>

r2_begin_

enum class gl_textureview_error : std::int32_t {
    gl_error
};

class gl_texture2d;

class gl_textureview : public textureview,
                       protected gl_object {
private:
    gl_texture2d* const resource_;

    GLuint view_texture_{ 0u };
    GLenum view_target_{ 0u };
    GLint view_level_{ 0u };
    texture_format view_format_{ texture_format::unknown };

public:
    gl_textureview(gl_context* ctx, gl_texture2d* tex, const textureview_desc& desc);

public:
    virtual void* native_texture_handle() const noexcept override {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(view_texture_));
    }

    [[nodiscard]] auto* resource() const noexcept { 
        return resource_;
    }
    [[nodiscard]] auto texture() const noexcept {
        return view_texture_;
    }
    [[nodiscard]] auto view_target() const noexcept {
        return view_target_; 
    }
    [[nodiscard]] auto view_level() const noexcept {
        return view_level_;
    }

private:
    static [[nodiscard]] GLenum to_gl_target(const texture_desc& td) noexcept;
    static [[nodiscard]] GLenum to_gl_attachment(view_usage usage, texture_format fmt) noexcept;
    static [[nodiscard]] GLint to_gl_format(texture_format fmt) noexcept;
};

r2_end_