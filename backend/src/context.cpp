#include <backend/context.h>

#if defined(R2_BACKEND_D3D11)
#include <backend/d3d11/context.h>
#elif defined(R2_BACKEND_OPENGL)
#include <backend/opengl/context.h>
#else
#error "no backend selected"
#endif


r2_begin_

std::unique_ptr<context> context::make_context(const platform_init_data& pinit, const backend_init_data& binit, bool common_origin)
{
#if defined(R2_BACKEND_D3D11)
    (void)common_origin;
    return std::make_unique<d3d11_context>(pinit, binit.sc);
#elif defined(R2_BACKEND_OPENGL)
    (void)binit;
    return std::make_unique<gl_context>(pinit, common_origin);
#endif // R2_BACKEND_OPENGL
}

context::context(const platform_init_data& pinit)
#if defined(R2_PLATFORM_WINDOWS)
    : hwnd_(pinit.hwnd)
#endif
{
#if defined(R2_PLATFORM_WINDOWS)
    assert(pinit.hwnd != NULL);
#endif
}

void context::release_backbuffer()
{
    backbuffer_.reset();
}

r2_end_