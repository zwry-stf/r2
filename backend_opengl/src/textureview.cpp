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
    if (usage & view_usage::render_target)
        return GL_COLOR_ATTACHMENT0;

    if (usage & view_usage::depth_stencil) {
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

    assert(!tex->is_backbuffer_handle() && "backbuffer only supported as render target");

    assert(resource_ != nullptr);
    
    const auto& td = resource_->desc();
    const bool msaa = td.sample_desc.count > 1;

    view_format_ =
        (desc_.format_override == texture_format::unknown) ?
            td.format : desc_.format_override;

    view_target_ = (msaa) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    if (msaa) {
        assert(td.mip_levels == 1u);
        assert(desc_.range.base_mip == 0u);
        view_level_ = 0;
    } else {
        assert(desc_.range.base_mip < td.mip_levels);
        view_level_ = static_cast<GLint>(desc_.range.base_mip);
    }

    if (desc_.usage & view_usage::shader_resource) {
        assert(td.usage & texture_usage::shader_resource);
        view_texture_ = tex->texture();
    }
}

r2_end_