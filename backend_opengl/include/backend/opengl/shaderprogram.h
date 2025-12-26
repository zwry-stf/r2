#pragma once
#include <backend/shaderprogram.h>
#include <backend/opengl/object.h>


r2_begin_

enum class gl_shaderprogram_error : std::int32_t {
    program_generation,
    program_creation
};

class gl_shaderprogram : public shaderprogram, 
                         protected gl_object {
private:
    GLuint program_{ 0u };

public:
    gl_shaderprogram(gl_context* ctx, class gl_vertexshader* vs, class gl_pixelshader* ps);
    ~gl_shaderprogram() override;

public:
    [[nodiscard]] auto program() const noexcept { 
        return program_;
    }
};


r2_end_