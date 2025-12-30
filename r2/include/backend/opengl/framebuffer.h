#pragma once
#include <backend/framebuffer.h>
#include <backend/opengl/object.h>


r2_begin_

enum class gl_framebuffer_error : std::int32_t {
    link_depth_view,
    framebuffer_generation,
    framebuffer_incomplete,
    gl_error,
};

class gl_framebuffer : public framebuffer,
                       protected gl_object {
private:
    GLuint fbo_{ 0u };

public:
    gl_framebuffer(gl_context* ctx, const framebuffer_desc& desc);
    ~gl_framebuffer();

public:
    [[nodiscard]] auto fbo() const noexcept {
        return fbo_;
    }

    static [[nodiscard]] GLenum to_gl_depth_attachment(texture_format fmt) noexcept;
};

r2_end_