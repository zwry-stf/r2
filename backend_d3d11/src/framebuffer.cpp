#include <backend/d3d11/framebuffer.h>
#include <assert.h>


r2_begin_

d3d11_framebuffer::d3d11_framebuffer(d3d11_context* ctx, const framebuffer_desc& desc)
    : r2::framebuffer(std::move(desc)),
      d3d11_object(ctx)
{
    assert(desc.color_attachment.view != nullptr);
    assert(desc.color_attachment.view->desc().usage & view_usage::render_target);

    if (desc.depth_attachment.view != nullptr) {
        assert(desc.depth_attachment.view->desc().usage & view_usage::depth_stencil);
    }
}

d3d11_framebuffer::~d3d11_framebuffer()
{
}

r2_end_