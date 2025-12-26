#include <backend/opengl/rasterizerstate.h>
#include <assert.h>


r2_begin_

GLenum gl_rasterizerstate::to_gl_fill(fill_mode m) noexcept
{
    switch (m) {
    case fill_mode::wireframe: return GL_LINE;
    case fill_mode::solid:     return GL_FILL;
    default:
        assert(false);
        return {};
    }
}

GLenum gl_rasterizerstate::to_gl_cull(cull_mode m) noexcept
{
    switch (m) {
    case cull_mode::none:  return 0; 
    case cull_mode::front: return GL_FRONT;
    case cull_mode::back:  return GL_BACK;
    default:
        assert(false);
        return {};
    }
}

gl_rasterizerstate::gl_rasterizerstate(gl_context* ctx, const rasterizerstate_desc& desc)
    : r2::rasterizerstate(desc),
      gl_object(ctx)
{
    //
}

gl_rasterizerstate::~gl_rasterizerstate()
{
}

void gl_rasterizerstate::bind() const
{
    // fill
    {
        const GLenum mode = to_gl_fill(desc_.fill);
        glPolygonMode(GL_FRONT_AND_BACK, mode);
    }

    // culling
    {
        const GLenum cull = to_gl_cull(desc_.cull);
        if (cull == 0) {
            glDisable(GL_CULL_FACE);
        }
        else {
            glEnable(GL_CULL_FACE);
            glCullFace(cull);
        }

        // front-face winding
        glFrontFace(desc_.front_ccw ? GL_CCW : GL_CW);
    }

    // depth bias
    if (desc_.depth_bias != 0 || 
        desc_.slope_scaled_bias != 0.0f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(desc_.slope_scaled_bias, static_cast<GLfloat>(desc_.depth_bias));
    }
    else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    if (desc_.depth_clip_enable)
        glDisable(GL_DEPTH_CLAMP);
    else
        glEnable(GL_DEPTH_CLAMP);

    if (desc_.scissor_enable)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    if (desc_.multisample_enable)
        glEnable(GL_MULTISAMPLE);
    else
        glDisable(GL_MULTISAMPLE);

    if (desc_.antialiased_lines)
        glEnable(GL_LINE_SMOOTH);
    else
        glDisable(GL_LINE_SMOOTH);
}

r2_end_