#include <backend/opengl/blendstate.h>
#include <backend/util.h>
#include <assert.h>


r2_begin_

GLenum gl_blendstate::to_gl_blend(blend_factor f) noexcept
{
    switch (f) {
    case blend_factor::zero:              return GL_ZERO;
    case blend_factor::one:               return GL_ONE;
    case blend_factor::src_color:         return GL_SRC_COLOR;
    case blend_factor::inv_src_color:     return GL_ONE_MINUS_SRC_COLOR;
    case blend_factor::src_alpha:         return GL_SRC_ALPHA;
    case blend_factor::inv_src_alpha:     return GL_ONE_MINUS_SRC_ALPHA;
    case blend_factor::dest_alpha:        return GL_DST_ALPHA;
    case blend_factor::inv_dest_alpha:    return GL_ONE_MINUS_DST_ALPHA;
    case blend_factor::dest_color:        return GL_DST_COLOR;
    case blend_factor::inv_dest_color:    return GL_ONE_MINUS_DST_COLOR;
    case blend_factor::src_alpha_sat:     return GL_SRC_ALPHA_SATURATE;
    case blend_factor::blend_factor:      return GL_CONSTANT_COLOR; 
    case blend_factor::inv_blend_factor:  return GL_ONE_MINUS_CONSTANT_COLOR;
    default:
        assert(false);
        return {};
    }
}

GLenum gl_blendstate::to_gl_op(blend_op op) noexcept
{
    switch (op) {
    case blend_op::add:          return GL_FUNC_ADD;
    case blend_op::subtract:     return GL_FUNC_SUBTRACT;
    case blend_op::rev_subtract: return GL_FUNC_REVERSE_SUBTRACT;
    case blend_op::min:          return GL_MIN;
    case blend_op::max:          return GL_MAX;
    default:
        assert(false);
        return {};
    }
}

static void apply_color_mask_i(GLuint index, color_write_mask m) noexcept
{
    const GLboolean r = (static_cast<std::uint32_t>(m) & static_cast<std::uint32_t>(color_write_mask::r)) ?
        GL_TRUE : GL_FALSE;
    const GLboolean g = (static_cast<std::uint32_t>(m) & static_cast<std::uint32_t>(color_write_mask::g)) ? 
        GL_TRUE : GL_FALSE;
    const GLboolean b = (static_cast<std::uint32_t>(m) & static_cast<std::uint32_t>(color_write_mask::b)) ?
        GL_TRUE : GL_FALSE;
    const GLboolean a = (static_cast<std::uint32_t>(m) & static_cast<std::uint32_t>(color_write_mask::a)) ?
        GL_TRUE : GL_FALSE;

    glColorMaski(index, r, g, b, a);
}

gl_blendstate::gl_blendstate(gl_context* ctx, const blendstate_desc& desc)
    : r2::blendstate(desc),
      gl_object(ctx)
{
    //
}

gl_blendstate::~gl_blendstate()
{
}

void gl_blendstate::bind(const float(&factor)[4], std::uint32_t mask) const
{
    glBlendColor(factor[0], factor[1], factor[2], factor[3]);
    glSampleMaski(0, mask);

    const bool indep = desc_.independent_blend_enable;
    const std::uint32_t rt_count = indep ? 
        static_cast<std::uint32_t>(v_count_of(desc_.targets)) : 1u;

    for (std::uint32_t i = 0u; i < rt_count; ++i) {
        const auto& src = desc_.targets[i];

        if (src.blend_enable) {
            if (i == 0u)
                glEnable(GL_BLEND);
            else
                glEnablei(GL_BLEND, i);

            // color blend
            const GLenum src_rgb = to_gl_blend(src.src_color_factor);
            const GLenum dst_rgb = to_gl_blend(src.dst_color_factor);
            const GLenum src_a = to_gl_blend(src.src_alpha_factor);
            const GLenum dst_a = to_gl_blend(src.dst_alpha_factor);

            if (i == 0u)
                glBlendFuncSeparate(src_rgb, dst_rgb, src_a, dst_a);
            else
                glBlendFuncSeparatei(i, src_rgb, dst_rgb, src_a, dst_a);

            // blend op
            const GLenum op_rgb = to_gl_op(src.color_op);
            const GLenum op_a = to_gl_op(src.alpha_op);

            if (i == 0u)
                glBlendEquationSeparate(op_rgb, op_a);
            else
                glBlendEquationSeparatei(i, op_rgb, op_a);
        }
        else
        {
            if (i == 0u)
                glDisable(GL_BLEND);
            else
                glDisablei(GL_BLEND, i);
        }

        // write mask
        apply_color_mask_i(i, src.write_mask);

        if (!indep)
            break;
    }
}

r2_end_