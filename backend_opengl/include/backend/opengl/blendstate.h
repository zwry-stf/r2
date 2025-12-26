#pragma once
#include <backend/blendstate.h>
#include <backend/opengl/object.h>


r2_begin_

class gl_blendstate : public blendstate,
                      protected gl_object {
public:
    gl_blendstate(gl_context* ctx, const blendstate_desc& desc);
    ~gl_blendstate();

public:
    void bind(const float(&factor)[4] = { 0.f, 0.f, 0.f, 0.f }, std::uint32_t sample_mask = 0xffffffffu) const;

public:
    static [[nodiscard]] GLenum to_gl_op(blend_op op) noexcept;
    static [[nodiscard]] GLenum to_gl_blend(blend_factor f) noexcept;
};

r2_end_