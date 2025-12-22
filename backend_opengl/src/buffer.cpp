#include <backend/opengl/buffer.h>
#include <assert.h>
#include <utility>


r2_begin_

GLenum gl_buffer::to_gl_target(buffer_usage usage) noexcept 
{
    switch (usage) {
    case buffer_usage::vertex:
        return GL_ARRAY_BUFFER;
    case buffer_usage::index:
        return GL_ELEMENT_ARRAY_BUFFER;
    case buffer_usage::uniform:
        return GL_UNIFORM_BUFFER;
    default:
        assert(false);
        return {};
    }
}

gl_buffer::gl_buffer(gl_context* ctx, const buffer_desc& desc, const void* data)
    : r2::buffer(desc),
      gl_object(ctx)
{
    assert(desc.usage != buffer_usage::vertex || desc.vb_stride != 0u);

    clear_gl_errors();
    
    glGenBuffers(1, &buffer_id_);
    if (drain_gl_errors() != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_buffer_error::buffer_generation)
        );
        return;
    }

    target_ = to_gl_target(desc.usage);

    glBindBuffer(target_, buffer_id_);

    const GLenum gl_usage = desc.dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;

    if (desc.usage == buffer_usage::uniform)
        assert((desc.size_bytes % 16) == 0 && 
            "uniform buffer size must be multiple of 16");

    glBufferData(target_, static_cast<GLsizeiptr>(desc.size_bytes), data, gl_usage);

    GLenum gl_err = drain_gl_errors();

    glBindBuffer(target_, 0);

    if (gl_err != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_buffer_error::buffer_creation),
            static_cast<std::int32_t>(gl_err)
        );
    }
}

gl_buffer::~gl_buffer()
{
    if (buffer_id_ != 0u) {
        glDeleteBuffers(1, &buffer_id_);
        buffer_id_ = 0u;
    }
}

void gl_buffer::update(const void* data, std::size_t size)
{
    assert(size <= desc_.size_bytes);
    assert(data != nullptr);
    assert(size > 0ull);

    glBindBuffer(target_, buffer_id_);

    if (desc_.dynamic) {
        glBufferData(
            target_,
            static_cast<GLsizeiptr>(desc_.size_bytes),
            nullptr,
            GL_DYNAMIC_DRAW
        );

        glBufferSubData(
            target_,
            0,
            static_cast<GLsizeiptr>(size),
            data
        );
    }
    else {
        glBufferSubData(
            target_,
            0,
            static_cast<GLsizeiptr>(size),
            data
        );
    }

    GLenum gl_err = glGetError();

    glBindBuffer(target_, 0);

    if (gl_err != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_buffer_error::buffer_update),
            static_cast<std::int32_t>(gl_err)
        );
    }
}

r2_end_