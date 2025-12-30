#pragma once
#include <backend/vertexshader.h>
#include <backend/opengl/object.h>
#include <backend/opengl/compiled_shader.h>


r2_begin_

enum class gl_vertexshader_error : std::int32_t {
    shader_generation,
    shader_compilation
};

class gl_vertexshader : public vertexshader,
                        protected gl_object {
private:
    GLuint shader_{ 0u };

public:
    gl_vertexshader(gl_context* ctx, gl_compiled_shader* shader_data);
    ~gl_vertexshader();

public:
    [[nodiscard]] auto shader() const noexcept {
        return shader_;
    }
};

r2_end_