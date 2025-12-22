#include <backend/opengl/sampler.h>
#include <utility>
#include <assert.h>


r2_begin_
 
GLenum gl_sampler::to_gl_filter(const sampler_desc& desc) noexcept
{
    switch (desc.filter) {
    case sampler_filter::nearest:
        return GL_NEAREST;
    case sampler_filter::linear:
        return GL_LINEAR;
    case sampler_filter::anisotropic:
        return GL_LINEAR;
    default:
        assert(false);
        return {};
    }
}

GLenum gl_sampler::to_gl_address(sampler_address_mode m) noexcept
{
    switch (m) {
    case sampler_address_mode::repeat:          return GL_REPEAT;
    case sampler_address_mode::clamp_to_edge:   return GL_CLAMP_TO_EDGE;
    case sampler_address_mode::clamp_to_border: return GL_CLAMP_TO_BORDER;
    case sampler_address_mode::mirror:          return GL_MIRRORED_REPEAT;
    default:
        assert(false);
        return {};
    }
}

GLenum gl_sampler::to_gl_compare(sampler_compare_func f) noexcept
{
    switch (f) {
    case sampler_compare_func::less_equal:    return GL_LEQUAL;
    case sampler_compare_func::greater_equal: return GL_GEQUAL;
    case sampler_compare_func::less:          return GL_LESS;
    case sampler_compare_func::greater:       return GL_GREATER;
    case sampler_compare_func::equal:         return GL_EQUAL;
    case sampler_compare_func::not_equal:     return GL_NOTEQUAL;
    case sampler_compare_func::always:        return GL_ALWAYS;
    case sampler_compare_func::never:         return GL_NEVER;
    case sampler_compare_func::none:          return GL_ALWAYS;
    default:
        assert(false);
        return {};
    }
}

gl_sampler::gl_sampler(gl_context* ctx, const sampler_desc& desc)
    : r2::sampler(desc),
      gl_object(ctx)
{
    clear_gl_errors();

    glGenSamplers(1, &sampler_);
    if (drain_gl_errors() != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_sampler_error::sampler_generation)
        );
        return;
    }

    const GLenum min_filter = to_gl_filter(desc);
    const GLenum mag_filter = to_gl_filter(desc);

    glSamplerParameteri(sampler_, GL_TEXTURE_MIN_FILTER, min_filter);
    glSamplerParameteri(sampler_, GL_TEXTURE_MAG_FILTER, mag_filter);

    glSamplerParameteri(sampler_, GL_TEXTURE_WRAP_S, to_gl_address(desc.address_u));
    glSamplerParameteri(sampler_, GL_TEXTURE_WRAP_T, to_gl_address(desc.address_v));
    glSamplerParameteri(sampler_, GL_TEXTURE_WRAP_R, to_gl_address(desc.address_w));

    glSamplerParameterfv(sampler_, GL_TEXTURE_BORDER_COLOR, desc.border_color);

    glSamplerParameterf(sampler_, GL_TEXTURE_LOD_BIAS, 0.f);
    glSamplerParameterf(sampler_, GL_TEXTURE_MIN_LOD,  0.f);
    glSamplerParameterf(sampler_, GL_TEXTURE_MAX_LOD,  0.f);

    if (desc.compare_func != sampler_compare_func::none) {
        glSamplerParameteri(sampler_, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glSamplerParameteri(sampler_, GL_TEXTURE_COMPARE_FUNC, to_gl_compare(desc.compare_func));
    }
    else {
        glSamplerParameteri(sampler_, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    }

    if (desc.filter == sampler_filter::anisotropic &&
        desc.max_anisotropy > 1.0f) {
#ifdef GL_TEXTURE_MAX_ANISOTROPY_EXT
        glSamplerParameterf(sampler_, GL_TEXTURE_MAX_ANISOTROPY_EXT, 
            static_cast<float>(desc.max_anisotropy));
#endif
    }

    GLenum gl_err = drain_gl_errors();
    if (gl_err != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_sampler_error::sampler_creation),
            static_cast<std::int32_t>(gl_err)
        );
    }
}

gl_sampler::~gl_sampler()
{
    if (sampler_ != 0u) {
        glDeleteSamplers(1, &sampler_);
        sampler_ = 0u;
    }
}

r2_end_