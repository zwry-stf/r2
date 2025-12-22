#pragma once
#include <backend/def.h>
#include <gl/glew.h>


r2_begin_

#if defined(_DEBUG)
#define gl_call(_call) do { _call; GLenum gl_err = glGetError(); \
    if (gl_err != 0) { __debugbreak(); } } while (0)
#else
#define gl_call(_call) _call
#endif // _DEBUG

inline static void clear_gl_errors() noexcept
{
    while (glGetError() != GL_NO_ERROR) {}
}

inline static GLenum drain_gl_errors() noexcept
{
    GLenum last = GL_NO_ERROR;
    for (GLenum e = glGetError(); e != GL_NO_ERROR; e = glGetError()) {
        last = e;
    }
    return last;
}

class gl_context;

class gl_object {
private:
    gl_context* const context_;

public:
    constexpr explicit gl_object(gl_context* ctx) noexcept
        : context_(ctx) { }

    virtual ~gl_object() = default;

    gl_object(const gl_object&) = delete;
    gl_object& operator=(const gl_object&) = delete;
    gl_object(gl_object&&) = delete;
    gl_object& operator=(gl_object&&) = delete;

protected:
    [[nodiscard]] gl_context* context() const noexcept {
        return context_;
    }
};

r2_end_