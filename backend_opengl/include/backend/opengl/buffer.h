#pragma once
#include <backend/buffer.h>
#include <backend/opengl/object.h>


r2_begin_

enum class gl_buffer_error : std::int32_t {
    buffer_generation,
    buffer_creation,
    buffer_update
};

class gl_buffer : public buffer,
                  protected gl_object {
private:
    GLuint buffer_id_{ 0u };
    GLenum target_{ GL_ARRAY_BUFFER };

public:
    gl_buffer(gl_context* ctx, const buffer_desc& desc, const void* data = nullptr);
    ~gl_buffer();

public:
    virtual void update(const void* data, std::size_t size) override;

    [[nodiscard]] auto buffer() const noexcept { 
        return buffer_id_;
    }
    [[nodiscard]] auto target() const noexcept { 
        return target_; 
    }

public:
    static [[nodiscard]] GLenum to_gl_target(buffer_usage usage) noexcept;
};

r2_end_