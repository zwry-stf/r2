#include <backend/opengl/texture2d.h>
#include <assert.h>
#include <utility>


r2_begin_

void gl_texture2d::to_gl_format(texture_format fmt, GLint& out_internal, GLenum& out_format, GLenum& out_type) noexcept
{
    switch (fmt)
    {
    case texture_format::rgba8_unorm:
        out_internal = GL_RGBA8;
        out_format   = GL_RGBA;
        out_type     = GL_UNSIGNED_BYTE;
        break;

    case texture_format::bgra8_unorm:
        out_internal = GL_RGBA8;
        out_format   = GL_BGRA;
        out_type     = GL_UNSIGNED_BYTE;
        break;

    case texture_format::r8_unorm:
        out_internal = GL_R8;
        out_format = GL_RED;
        out_type = GL_UNSIGNED_BYTE;
        break;

    case texture_format::r16_float:
        out_internal = GL_R16F;
        out_format   = GL_RED;
        out_type     = GL_HALF_FLOAT;
        break;

    case texture_format::r32_float:
        out_internal = GL_R32F;
        out_format   = GL_RED;
        out_type     = GL_FLOAT;
        break;

    case texture_format::d24s8:
        out_internal = GL_DEPTH24_STENCIL8;
        out_format   = GL_DEPTH_STENCIL;
        out_type     = GL_UNSIGNED_INT_24_8;
        break;

    case texture_format::d32_float:
        out_internal = GL_DEPTH_COMPONENT32F;
        out_format   = GL_DEPTH_COMPONENT;
        out_type     = GL_FLOAT;
        break;
    }
}

std::uint32_t bytes_per_pixel(texture_format fmt) noexcept
{
    switch (fmt)
    {
    case texture_format::rgba8_unorm: return 4;
    case texture_format::bgra8_unorm: return 4;
    case texture_format::r8_unorm:    return 1;
    case texture_format::r16_float:   return 2;
    case texture_format::r32_float:   return 4;
    case texture_format::d24s8:       return 4;
    case texture_format::d32_float:   return 4;
    }
    return 0;
}

gl_texture2d::gl_texture2d(gl_context* ctx, const texture_desc& desc, const void* data)
    : r2::texture2d(desc),
      gl_object(ctx)
{
    assert(desc.width > 0 && desc.height > 0);

    const bool multisampled = desc.sample_desc.count > 1;

    GLint  internal_format = 0;
    GLenum format = 0;
    GLenum type = 0;
    to_gl_format(desc.format, internal_format, format, type);

    glGenTextures(1, &texture_);
    if (drain_gl_errors() != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_texture2d_error::texture_generation)
        );
        return;
    }

    if (multisampled) {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture_);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                                desc.sample_desc.count,
                                internal_format,
                                desc.width,
                                desc.height,
                                GL_TRUE
        );
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    }
    else {
        GLint backup_unpack_alignment;
        GLint backup_unpack_row_length;
        GLint backup_unpack_skip_rows;
        GLint backup_unpack_skip_pixels;

        glGetIntegerv(GL_UNPACK_ALIGNMENT,   &backup_unpack_alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,  &backup_unpack_row_length);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS,   &backup_unpack_skip_rows);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &backup_unpack_skip_pixels);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

        glBindTexture(GL_TEXTURE_2D, texture_);

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     internal_format,
                     desc.width,
                     desc.height,
                     0,
                     format,
                     type,
                     data
        );

        glPixelStorei(GL_UNPACK_ALIGNMENT,   backup_unpack_alignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,  backup_unpack_row_length);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,   backup_unpack_skip_rows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, backup_unpack_skip_pixels);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GLenum gl_err = drain_gl_errors();
    if (gl_err != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_texture2d_error::texture_creation),
            static_cast<std::int32_t>(gl_err)
        );
    }
}

gl_texture2d::~gl_texture2d()
{
    if (texture_ != 0u) {
        glDeleteTextures(1, &texture_);
        texture_ = 0;
    }
}

void gl_texture2d::update(const void* data, std::uint32_t row_pitch)
{
    const auto& desc = desc_;

    assert(texture_ != 0u);
    assert(desc.width > 0 && desc.height > 0);

    assert(desc.sample_desc.count == 1u);

    GLint  internal_format = 0;
    GLenum format = 0;
    GLenum type = 0;
    to_gl_format(desc.format, internal_format, format, type);

    const std::uint32_t bpp = bytes_per_pixel(desc.format);
    assert(bpp != 0);

    GLint backup_unpack_alignment;
    GLint backup_unpack_row_length;
    GLint backup_unpack_skip_rows;
    GLint backup_unpack_skip_pixels;

    glGetIntegerv(GL_UNPACK_ALIGNMENT,   &backup_unpack_alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH,  &backup_unpack_row_length);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,   &backup_unpack_skip_rows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &backup_unpack_skip_pixels);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

    const std::uint32_t tight_pitch = desc.width * bpp;

    glBindTexture(GL_TEXTURE_2D, texture_);

    if (row_pitch == 0 || row_pitch == tight_pitch) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0, 0,
                        desc.width,
                        desc.height,
                        format,
                        type,
                        data);
    }
    else if ((row_pitch % bpp) == 0) {
        const GLint row_length_pixels = static_cast<GLint>(row_pitch / bpp);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, row_length_pixels);

        glTexSubImage2D(GL_TEXTURE_2D,
                        0,
                        0, 0,
                        desc.width,
                        desc.height,
                        format,
                        type,
                        data);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }
    else {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        const auto* bytes = static_cast<const std::uint8_t*>(data);
        for (GLint y = 0; y < static_cast<GLint>(desc.height); ++y)
        {
            const void* row_ptr = bytes + static_cast<std::size_t>(y) * row_pitch;

            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0, y,
                            desc.width,
                            1,
                            format,
                            type,
                            row_ptr);
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    glPixelStorei(GL_UNPACK_ALIGNMENT,   backup_unpack_alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,  backup_unpack_row_length);
    glPixelStorei(GL_UNPACK_SKIP_ROWS,   backup_unpack_skip_rows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, backup_unpack_skip_pixels);

    const GLenum gl_err = drain_gl_errors();
    if (gl_err != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_texture2d_error::texture_update),
            static_cast<std::int32_t>(gl_err)
        );
    }
}

r2_end_