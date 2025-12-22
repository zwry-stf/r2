#include <backend/d3d11/shaderprogram.h>
#include <backend/d3d11/context.h>
#include <backend/d3d11/vertexshader.h>
#include <backend/d3d11/pixelshader.h>
#include <assert.h>


r2_begin_

d3d11_shaderprogram::d3d11_shaderprogram(d3d11_context* ctx, d3d11_vertexshader* vs, d3d11_pixelshader* ps)
    : r2::shaderprogram(),
      d3d11_object(ctx)
{
    assert(vs != nullptr);
    assert(ps != nullptr);

    vs_.reset(vs->shader());
    vs_->AddRef();

    ps_.reset(ps->shader());
    ps_->AddRef();
}

d3d11_shaderprogram::~d3d11_shaderprogram()
{
    vs_.reset();
    ps_.reset();
}

void d3d11_shaderprogram::bind() const
{
    assert(ps_ && vs_);

    context()->get_context()->PSSetShader(ps_, nullptr, 0u);
    context()->get_context()->VSSetShader(vs_, nullptr, 0u);
}

r2_end_