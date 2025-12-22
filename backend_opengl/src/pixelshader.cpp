#include <backend/opengl/pixelshader.h>
#include <utility>
#include <assert.h>
#include <string>


r2_begin_

gl_pixelshader::gl_pixelshader(gl_context* ctx, gl_compiled_shader* shader_data)
    : r2::pixelshader(),
      gl_object(ctx)
{
    assert(shader_data->source() != nullptr &&
           shader_data->source_length() > 0);

    clear_gl_errors();

    GLuint shader = glCreateShader(GL_FRAGMENT_SHADER);
    if (shader == 0u ||
        drain_gl_errors() != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_pixelshader_error::shader_generation)
        );
        return;
    }
    
    auto* src = shader_data->source();
    static_assert(std::is_same_v<char, GLchar>);
    auto size = static_cast<GLint>(shader_data->source_length());

    glShaderSource(shader, 1, &src, &size);
    glCompileShader(shader);
    GLenum gl_err = drain_gl_errors();
    if (gl_err != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_pixelshader_error::shader_compilation),
            static_cast<std::int32_t>(gl_err)
        );
        return;
    }

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (status != GL_TRUE) {
#ifdef _DEBUG
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        std::string log;
        log.resize(log_length);
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());
        assert(false && "failed to compile pixel shader");
#endif

        set_error(
            std::to_underlying(gl_pixelshader_error::shader_compilation)
        );

        glDeleteShader(shader);
        return;
    }

    shader_ = shader;
}

gl_pixelshader::~gl_pixelshader()
{
    if (shader_ != 0u) {
        glDeleteShader(shader_);
        shader_ = 0u;
    }
}

r2_end_