#pragma once
#include <backend/rasterizerstate.h>
#include <backend/opengl/object.h>


r2_begin_

class gl_rasterizerstate : public rasterizerstate,
                           protected gl_object {
public:
    gl_rasterizerstate(gl_context* ctx, const rasterizerstate_desc& desc);
    ~gl_rasterizerstate();

public:
    virtual void bind() override;

public:
    static [[nodiscard]] GLenum to_gl_fill(fill_mode m) noexcept;
    static [[nodiscard]] GLenum to_gl_cull(cull_mode m) noexcept;
};

r2_end_