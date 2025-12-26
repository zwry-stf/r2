#include <backend/opengl/framebuffer.h>
#include <backend/opengl/context.h>
#include <backend/opengl/textureview.h>
#include <backend/opengl/texture2d.h>


r2_begin_

GLenum gl_framebuffer::to_gl_depth_attachment(texture_format fmt) noexcept
{
    return (fmt == texture_format::d24s8) ?
        GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
}

gl_framebuffer::gl_framebuffer(gl_context* ctx, const framebuffer_desc& desc)
    : r2::framebuffer(desc),
      gl_object(ctx)
{
    clear_gl_errors();
    

    assert(desc.color_attachment.view != nullptr);
    assert(desc.color_attachment.view->desc().usage & view_usage::render_target);

    auto* rtv = to_native(desc.color_attachment.view);

    if (rtv->resource()->is_backbuffer_handle()) {
        assert(desc.depth_attachment.view == nullptr);
        return;
    }

    glGenFramebuffers(1, &fbo_);
    if (drain_gl_errors() != GL_NO_ERROR ||
        fbo_ == 0u) {
        set_error(
            std::to_underlying(gl_framebuffer_error::framebuffer_generation)
        );
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // color
    {
        auto* res = rtv->resource();
        const auto& td = res->desc();

        assert(td.usage & texture_usage::render_target);

        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            rtv->view_target(),
            res->texture(),
            rtv->view_level()
        );

        GLenum draw_buf = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &draw_buf);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }

    // depth
    if (desc.depth_attachment.view != nullptr) {
        assert(desc.depth_attachment.view->desc().usage & view_usage::depth_stencil);

        auto* dsv = to_native(desc.depth_attachment.view);
        auto* res = dsv->resource();
        const auto& td = res->desc();

        assert(td.usage & texture_usage::depth_stencil);

        const GLenum attachment = to_gl_depth_attachment(td.format);

        glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            attachment,
            dsv->view_target(),
            res->texture(),
            dsv->view_level()
        );
    }

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    const GLenum gl_err = drain_gl_errors();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        set_error(
            std::to_underlying(gl_framebuffer_error::framebuffer_incomplete),
            static_cast<std::int32_t>(status)
        );
        return;
    }

    if (gl_err != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_framebuffer_error::gl_error),
            static_cast<std::int32_t>(gl_err)
        );
        return;
    }
}

gl_framebuffer::~gl_framebuffer()
{
    if (fbo_ != 0u) {
        glDeleteFramebuffers(1u, &fbo_);
        fbo_ = 0u;
    }
}

r2_end_