#include <backend/opengl/shaderprogram.h>
#include <backend/opengl/context.h>
#include <backend/opengl/vertexshader.h>
#include <backend/opengl/pixelshader.h>
#include <assert.h>


r2_begin_

gl_shaderprogram::gl_shaderprogram(gl_context* ctx, gl_vertexshader* vs, gl_pixelshader* ps)
    : r2::shaderprogram(),
      gl_object(ctx)
{
    assert(vs != nullptr);
    assert(ps != nullptr);

    clear_gl_errors();

    program_ = glCreateProgram();
    if (program_ == 0u ||
        drain_gl_errors() != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_shaderprogram_error::program_generation)
        );
        return;
    }

    glAttachShader(program_, vs->shader());
    glAttachShader(program_, ps->shader());

    glLinkProgram(program_);

    GLint linked = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        set_error(
            std::to_underlying(gl_shaderprogram_error::program_creation)
        );
        return;
    }

    glDetachShader(program_, vs->shader());
    glDetachShader(program_, ps->shader());
}

gl_shaderprogram::~gl_shaderprogram()
{
    if (program_ != 0u) {
        glDeleteProgram(program_);
        program_ = 0u;
    }
}

r2_end_