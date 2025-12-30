#pragma once
#include <backend/pixelshader.h>
#include <backend/opengl/object.h>
#include <backend/opengl/compiled_shader.h>


r2_begin_

enum class gl_pixelshader_error : std::int32_t {
    shader_generation,
    shader_compilation,
    shader_creation
};

class gl_pixelshader : public pixelshader,
                       protected gl_object {
private:
    GLuint shader_{ 0u };

public:
    gl_pixelshader(gl_context* ctx, gl_compiled_shader* shader_data);
    ~gl_pixelshader();

public:
    [[nodiscard]] auto shader() const noexcept {
        return shader_;
    }
};

r2_end_