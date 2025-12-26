#include <backend/opengl/depthstencilstate.h>
#include <assert.h>


r2_begin_

GLenum gl_depthstencilstate::to_gl_comp(comparison_func f) noexcept
{
    switch (f) {
    case comparison_func::never:         return GL_NEVER;
    case comparison_func::less:          return GL_LESS;
    case comparison_func::equal:         return GL_EQUAL;
    case comparison_func::less_equal:    return GL_LEQUAL;
    case comparison_func::greater:       return GL_GREATER;
    case comparison_func::not_equal:     return GL_NOTEQUAL;
    case comparison_func::greater_equal: return GL_GEQUAL;
    case comparison_func::always:        return GL_ALWAYS;
    default:
        assert(false);
        return {};
    }
}

GLenum gl_depthstencilstate::to_gl_stencil(stencil_op op) noexcept
{
    switch (op) {
    case stencil_op::keep:     return GL_KEEP;
    case stencil_op::zero:     return GL_ZERO;
    case stencil_op::replace:  return GL_REPLACE;
    case stencil_op::incr_sat: return GL_INCR_WRAP;
    case stencil_op::decr_sat: return GL_DECR_WRAP;
    case stencil_op::invert:   return GL_INVERT;
    case stencil_op::incr:     return GL_INCR;
    case stencil_op::decr:     return GL_DECR;
    default:
        assert(false);
        return {};
    }
}

gl_depthstencilstate::gl_depthstencilstate(gl_context* ctx, const depthstencilstate_desc& desc)
    : r2::depthstencilstate(desc),
      gl_object(ctx)
{
    //
}

gl_depthstencilstate::~gl_depthstencilstate()
{
}

void gl_depthstencilstate::bind(std::uint32_t stencil_ref) const
{
    if (desc_.depth_enable) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(to_gl_comp(desc_.depth_func));
    }
    else {
        glDisable(GL_DEPTH_TEST);
    }

    glDepthMask(desc_.depth_write ? GL_TRUE : GL_FALSE);

    if (desc_.stencil_enable) {
        glEnable(GL_STENCIL_TEST);

        // front face
        {
            const auto& f = desc_.front_face;
            glStencilFuncSeparate(
                GL_FRONT,
                to_gl_comp(f.func),
                stencil_ref,
                desc_.stencil_read_mask
            );
            glStencilOpSeparate(
                GL_FRONT,
                to_gl_stencil(f.fail_op),
                to_gl_stencil(f.depth_fail_op),
                to_gl_stencil(f.pass_op)
            );
            glStencilMaskSeparate(GL_FRONT, desc_.stencil_write_mask);
        }

        // back face
        {
            const auto& b = desc_.back_face;
            glStencilFuncSeparate(
                GL_BACK,
                to_gl_comp(b.func),
                stencil_ref,
                desc_.stencil_read_mask
            );
            glStencilOpSeparate(
                GL_BACK,
                to_gl_stencil(b.fail_op),
                to_gl_stencil(b.depth_fail_op),
                to_gl_stencil(b.pass_op)
            );
            glStencilMaskSeparate(GL_BACK, desc_.stencil_write_mask);
        }
    }
    else {
        glDisable(GL_STENCIL_TEST);
    }
}

r2_end_