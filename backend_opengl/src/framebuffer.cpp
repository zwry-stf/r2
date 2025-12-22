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
    assert(desc.color_attachment.view->desc().usage == view_usage::render_target);

    if (desc.depth_attachment.view != nullptr) {
        assert(desc.depth_attachment.view->desc().usage == view_usage::depth_stencil);

        auto* gdsv = to_native(desc.depth_attachment.view);
        auto* res = gdsv->resource();
        const auto& td = res->desc();

        const GLenum tgt = (td.sample_desc.count > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        const GLenum attachment = to_gl_depth_attachment(td.format);
        const GLint level = (td.sample_desc.count > 1) ? 0 : static_cast<GLint>(gdsv->desc().range.base_mip);
        gl_call(glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, tgt, res->texture(), level));

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        const GLenum gl_err = drain_gl_errors();
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            set_error(
                std::to_underlying(gl_framebuffer_error::link_depth_view),
                static_cast<std::int32_t>(status)
            );
        }
        else if (gl_err != GL_NO_ERROR) {
            set_error(
                std::to_underlying(gl_framebuffer_error::link_depth_view),
                static_cast<std::int32_t>(gl_err)
            );
        }
    }
}

gl_framebuffer::~gl_framebuffer()
{
}

r2_end_