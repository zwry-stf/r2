#include <backend/opengl/textureview.h>
#include <backend/opengl/texture2d.h>
#include <assert.h>
#include <utility>


r2_begin_

GLenum gl_textureview::to_gl_target(const texture_desc& td) noexcept
{
    return (td.sample_desc.count > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
}

GLenum gl_textureview::to_gl_attachment(view_usage usage, texture_format fmt) noexcept
{
    if (usage == view_usage::render_target)
        return GL_COLOR_ATTACHMENT0;

    if (usage == view_usage::depth_stencil) {
        if (fmt == texture_format::d24s8)
            return GL_DEPTH_STENCIL_ATTACHMENT;
        return GL_DEPTH_ATTACHMENT;
    }

    return 0;
}

GLint gl_textureview::to_gl_format(texture_format fmt) noexcept
{
    GLint internal_format = 0;
    GLenum format = 0;
    GLenum type = 0;
    gl_texture2d::to_gl_format(fmt, internal_format, format, type);
    return internal_format;
}

gl_textureview::gl_textureview(gl_context* ctx, gl_texture2d* tex, const textureview_desc& desc)
    : r2::textureview(desc),
      gl_object(ctx),
      resource_(tex)
{
    assert(tex != nullptr);

    if (tex->is_backbuffer_handle() &&
        desc.usage == view_usage::render_target)
        return;

    assert(!tex->is_backbuffer_handle());

    assert(resource_ != nullptr);

    const auto& td = resource_->desc();
    const bool msaa = td.sample_desc.count > 1;

    const texture_format view_fmt =
        (desc_.format_override == texture_format::unknown) ? 
            td.format : desc_.format_override;

    const GLenum base_target = to_gl_target(td);
    view_target_ = base_target;

    if (msaa) {
        assert(td.mip_levels == 1u);
        assert(desc_.range.base_mip == 0u);
    } else {
        assert(desc_.range.base_mip < td.mip_levels);
        if (desc_.range.mip_count != 0u) {
            assert(static_cast<std::uint32_t>(desc_.range.base_mip + desc_.range.mip_count) <= td.mip_levels);
        }
    }

    if (desc_.usage == view_usage::shader_resource) {
        assert((td.usage & texture_usage::shader_resource) != texture_usage::none);

        view_texture_ = tex->texture();
    }

    else if (desc_.usage == view_usage::render_target || 
        desc_.usage == view_usage::depth_stencil) {

        if (desc_.usage == view_usage::render_target)
            assert((td.usage & texture_usage::render_target) != texture_usage::none);
        else
            assert((td.usage & texture_usage::depth_stencil) != texture_usage::none);

        glGenFramebuffers(1, &fbo_);
        if (drain_gl_errors() != GL_NO_ERROR || fbo_ == 0u) {
            set_error(std::to_underlying(gl_textureview_error::framebuffer_generation));
            return;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

        const GLenum attachment = to_gl_attachment(desc_.usage, view_fmt);
        assert(attachment != 0);

        const GLuint tex_id = resource_->texture();

        if (msaa) {
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D_MULTISAMPLE, tex_id, 0);
        } else {
            const GLint level = static_cast<GLint>(desc_.range.base_mip);
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex_id, level);
        }

        if (desc_.usage == view_usage::render_target) {
            GLenum draw_buf = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &draw_buf);
        } else {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE) {
            set_error(
                std::to_underlying(gl_textureview_error::framebuffer_incomplete),
                static_cast<std::int32_t>(status)
            );
            return;
        }

        GLenum gl_err = drain_gl_errors();
        if (gl_err != GL_NO_ERROR) {
            set_error(
                std::to_underlying(gl_textureview_error::gl_error),
                static_cast<std::int32_t>(gl_err)
            );
            return;
        }
    }
}

gl_textureview::~gl_textureview()
{
    if (fbo_ != 0u) {
        glDeleteFramebuffers(1, &fbo_);
        fbo_ = 0u;
    }
}

r2_end_