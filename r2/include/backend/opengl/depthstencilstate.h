#pragma once
#include <backend/depthstencilstate.h>
#include <backend/opengl/object.h>


r2_begin_

class gl_depthstencilstate : public depthstencilstate,
                             protected gl_object {
public:
    gl_depthstencilstate(gl_context* ctx, const depthstencilstate_desc& desc);
    ~gl_depthstencilstate();

public:
    void bind(std::uint32_t stencil_ref = 0u) const;

public:
    static [[nodiscard]] GLenum to_gl_comp(comparison_func f) noexcept;
    static [[nodiscard]] GLenum to_gl_stencil(stencil_op op) noexcept;
};

r2_end_