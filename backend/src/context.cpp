#include <backend/context.h>

#if defined(R2_BACKEND_D3D11)
#include <backend/d3d11/context.h>
#elif defined(R2_BACKEND_OPENGL)
#include <backend/opengl/context.h>
#else
#error "no backend selected"
#endif


r2_begin_

std::unique_ptr<context> context::make_context(const context_init_data& data, bool common_origin)
{
#if defined(R2_BACKEND_D3D11)
    (void)common_origin;
    return std::make_unique<d3d11_context>(data.sc);
#elif defined(R2_BACKEND_OPENGL)
    (void)data;
    return std::make_unique<gl_context>(data, common_origin);
#endif // R2_BACKEND_OPENGL
}

void context::release_backbuffer()
{
    backbuffer_.reset();
}

r2_end_