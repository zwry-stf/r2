#include <backend/d3d11/context.h>
#include <assert.h>

//
#include <backend/d3d11/blendstate.h>
#include <backend/d3d11/buffer.h>
#include <backend/d3d11/depthstencilstate.h>
#include <backend/d3d11/inputlayout.h>
#include <backend/d3d11/pixelshader.h>
#include <backend/d3d11/rasterizerstate.h>
#include <backend/d3d11/sampler.h>
#include <backend/d3d11/shaderprogram.h>
#include <backend/d3d11/texture2d.h>
#include <backend/d3d11/textureview.h>
#include <backend/d3d11/vertexshader.h>
#include <backend/d3d11/compiled_shader.h>
#include <backend/d3d11/framebuffer.h>


r2_begin_

d3d11_context::d3d11_context(IDXGISwapChain* sc)
{
    if (sc == nullptr) {
        set_error(
            std::to_underlying(d3d11_context_error::invalid_param)
        );
        return;
    }

    sc->AddRef();
    sc_.reset(sc);
    
    HRESULT hr = sc_->GetDevice(__uuidof(ID3D11Device), (void**)device_.address_of());
    if (FAILED(hr)) {
        set_error(
            std::to_underlying(d3d11_context_error::device), 
            hr
        );
        return;
    }

    device_->GetImmediateContext(context_.address_of());
}

/// get
rect d3d11_context::get_scissor_rect() const noexcept
{
    UINT count = 1;
    D3D11_RECT r;
    context_->RSGetScissorRects(&count, &r);

    return rect{
        static_cast<std::int32_t>(r.left),
        static_cast<std::int32_t>(r.top),
        static_cast<std::int32_t>(r.right),
        static_cast<std::int32_t>(r.bottom)
    };
}

primitive_topology d3d11_context::get_primitive_topology() const noexcept
{
    D3D11_PRIMITIVE_TOPOLOGY t;
    context_->IAGetPrimitiveTopology(&t);

    switch (t) {
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
        return primitive_topology::triangle_list;
    case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
        return primitive_topology::line_list;
    case D3D11_PRIMITIVE_TOPOLOGY_POINTLIST:
        return primitive_topology::point_list;

    default:
        return primitive_topology::unknown;
    }
}

viewport d3d11_context::get_viewport() const noexcept
{
    UINT count = 1;
    D3D11_VIEWPORT vp;
    context_->RSGetViewports(&count, &vp);

    return viewport{
        static_cast<float>(vp.TopLeftX),
        static_cast<float>(vp.TopLeftY),
        static_cast<float>(vp.Width),
        static_cast<float>(vp.Height),
        static_cast<float>(vp.MinDepth),
        static_cast<float>(vp.MaxDepth)
    };
}

/// create

std::unique_ptr<blendstate> d3d11_context::create_blendstate(const blendstate_desc& desc)
{
    return std::make_unique<d3d11_blendstate>(this, desc);
}

std::unique_ptr<buffer> d3d11_context::create_buffer(const buffer_desc& desc, const void* initial_data)
{
    return std::make_unique<d3d11_buffer>(this, desc, initial_data);
}

std::unique_ptr<depthstencilstate> d3d11_context::create_depthstencilstate(const depthstencilstate_desc& desc)
{
    return std::make_unique<d3d11_depthstencilstate>(this, desc);
}

std::unique_ptr<rasterizerstate> d3d11_context::create_rasterizerstate(const rasterizerstate_desc& desc)
{
    return std::make_unique<d3d11_rasterizerstate>(this, desc);
}

std::unique_ptr<sampler> d3d11_context::create_sampler(const sampler_desc& desc)
{
    return std::make_unique<d3d11_sampler>(this, desc);
}

std::unique_ptr<compiled_shader> d3d11_context::compile_vertex_shader(const char* source, std::size_t length)
{
    return std::make_unique<d3d11_compiled_shader>(this, source, length, "vs_4_0");
}

std::unique_ptr<vertexshader> d3d11_context::create_vertexshader(compiled_shader* shader_data)
{
    return create_vertexshader(
        shader_data->data(),
        shader_data->size()
    );
}

std::unique_ptr<vertexshader> d3d11_context::create_vertexshader(const void* data, std::size_t size_bytes)
{
    return std::make_unique<d3d11_vertexshader>(this, 
        reinterpret_cast<const std::uint8_t*>(data), 
        size_bytes
    );
}

std::unique_ptr<compiled_shader> d3d11_context::compile_pixel_shader(const char* source, std::size_t length)
{
    return std::make_unique<d3d11_compiled_shader>(this, source, length, "ps_4_0");
}

std::unique_ptr<pixelshader> d3d11_context::create_pixelshader(compiled_shader* shader_data)
{
    return create_pixelshader(
        shader_data->data(),
        shader_data->size()
    );
}

std::unique_ptr<pixelshader> d3d11_context::create_pixelshader(const void* data, std::size_t size_bytes)
{
    return std::make_unique<d3d11_pixelshader>(this,
        reinterpret_cast<const std::uint8_t*>(data), 
        size_bytes
    );
}

std::unique_ptr<shaderprogram> d3d11_context::create_shaderprogram(vertexshader* vs, pixelshader* ps)
{
    return std::make_unique<d3d11_shaderprogram>(this, 
        reinterpret_cast<d3d11_vertexshader*>(vs),
        reinterpret_cast<d3d11_pixelshader*>(ps)
    );
}

std::unique_ptr<inputlayout> d3d11_context::create_inputlayout(const vertex_attribute_desc* desc, std::uint32_t count,
                                                               const void* vs_data, std::size_t vs_data_size)
{
    return std::make_unique<d3d11_inputlayout>(this, desc, count,
        reinterpret_cast<const std::uint8_t*>(vs_data), vs_data_size
    );
}

std::unique_ptr<texture2d> d3d11_context::create_texture2d(const texture_desc& desc, const void* initial_data)
{
    return std::make_unique<d3d11_texture2d>(this, desc, initial_data);
}

std::optional<std::unique_ptr<texture2d>> d3d11_context::acquire_backbuffer()
{
    d3d_pointer<ID3D11Texture2D> back_buffer;
    HRESULT res = sc_->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
    if (FAILED(res))
        return std::nullopt;

    return d3d11_texture2d::from_existing(this, back_buffer.get());
}

std::unique_ptr<textureview> d3d11_context::create_textureview(texture2d* tex, const textureview_desc& desc)
{
    return std::make_unique<d3d11_textureview>(this, to_native(tex), desc);
}

std::unique_ptr<framebuffer> d3d11_context::create_framebuffer(const framebuffer_desc& desc)
{
    return std::make_unique<d3d11_framebuffer>(this, desc);
}

/// bind

void d3d11_context::set_vertex_buffer(buffer* vb, std::uint32_t slot)
{
    assert(vb == nullptr || vb->desc().usage == buffer_usage::vertex);

    auto* vbuf = vb == nullptr ? nullptr : to_native(vb)->buffer();

    const UINT offset = 0u;
    const UINT stride = static_cast<UINT>(vb->desc().vb_stride);

    context_->IASetVertexBuffers(
        static_cast<UINT>(slot),
        1u,
        &vbuf,
        &stride,
        &offset
    );
}

void d3d11_context::set_index_buffer(buffer* ib)
{
    assert(ib == nullptr || ib->desc().usage == buffer_usage::index);

    context_->IASetIndexBuffer(
        ib == nullptr ? nullptr : to_native(ib)->buffer(),
        ib->desc().ib_type == index_buffer_type::u16 ?
            DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
        0u
    );
}

void d3d11_context::set_uniform_buffer(buffer* ub, shader_bind_type stage, std::uint32_t slot)
{
    auto* cbuf = ub == nullptr ? nullptr : to_native(ub)->buffer();

    switch (stage) {
    case shader_bind_type::ps:
        context_->PSSetConstantBuffers(
            static_cast<UINT>(slot),
            1u,
            &cbuf
        );
        break;
    case shader_bind_type::vs:
        context_->VSSetConstantBuffers(
            static_cast<UINT>(slot),
            1u,
            &cbuf
        );
        break;
    case shader_bind_type::cs:
        context_->CSSetConstantBuffers(
            static_cast<UINT>(slot),
            1u,
            &cbuf
        );
        break;
    }
}

void d3d11_context::set_texture(textureview* srv, shader_bind_type stage, std::uint32_t slot)
{
    assert(srv == nullptr || srv->desc().usage == view_usage::shader_resource);

    auto* tex = srv == nullptr ? nullptr : to_native(srv)->srv();

    set_texture_native(tex, stage, slot);
}

void d3d11_context::set_texture_native(void* handle, shader_bind_type stage, std::uint32_t slot)
{
    auto* tex = reinterpret_cast<ID3D11ShaderResourceView*>(handle);

    switch (stage) {
    case shader_bind_type::ps:
        context_->PSSetShaderResources(
            static_cast<UINT>(slot),
            1u,
            &tex
        );
        break;
    case shader_bind_type::vs:
        context_->VSSetShaderResources(
            static_cast<UINT>(slot),
            1u,
            &tex
        );
        break;
    case shader_bind_type::cs:
        context_->CSSetShaderResources(
            static_cast<UINT>(slot),
            1u,
            &tex
        );
        break;
    }
}

void d3d11_context::set_sampler(sampler* s, shader_bind_type stage, std::uint32_t slot)
{
    auto* sampler = s == nullptr ? nullptr : to_native(s)->sampler();

    switch (stage) {
    case shader_bind_type::ps:
        context_->PSSetSamplers(
            static_cast<UINT>(slot),
            1u,
            &sampler
        );
        break;
    case shader_bind_type::vs:
        context_->VSSetSamplers(
            static_cast<UINT>(slot),
            1u,
            &sampler
        );
        break;
    case shader_bind_type::cs:
        context_->CSSetSamplers(
            static_cast<UINT>(slot),
            1u,
            &sampler
        );
        break;
    }
}

void d3d11_context::set_framebuffer(framebuffer* fb)
{
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    if (fb != nullptr) {
        const auto& desc = fb->desc();

        rtv = to_native(desc.color_attachment.view)->rtv();

        if (desc.depth_attachment.view != nullptr) {
            dsv = to_native(desc.depth_attachment.view)->dsv();
        }
    }

    context_->OMSetRenderTargets(
        1u,
        &rtv,
        dsv
    );
}

void d3d11_context::clear_framebuffer(framebuffer* fb)
{
    constexpr FLOAT clear_color[] = { 0.f, 0.f, 0.f, 0.f };
    context_->ClearRenderTargetView(
        to_native(fb->desc().color_attachment.view)->rtv(),
        &clear_color[0]
    );

    if (fb->desc().depth_attachment.view != nullptr) {
        context_->ClearDepthStencilView(
            to_native(fb->desc().depth_attachment.view)->dsv(),
            0u,
            0.f,
            0u
        );
    }
}

void d3d11_context::draw(std::uint32_t count, std::uint32_t vertex_start)
{
    context_->Draw(
        static_cast<UINT>(count), 
        static_cast<UINT>(vertex_start)
    );
}

void d3d11_context::draw_indexed(std::uint32_t count, std::uint32_t index_start, std::uint32_t vertex_start)
{
    context_->DrawIndexed(
        static_cast<UINT>(count),
        static_cast<UINT>(index_start),
        static_cast<UINT>(vertex_start)
    );
}

void d3d11_context::set_scissor_rect(const rect& rect)
{
    assert(rect.left <= rect.right);
    assert(rect.top <= rect.bottom);

    D3D11_RECT r{
        static_cast<LONG>(rect.left),
        static_cast<LONG>(rect.top),
        static_cast<LONG>(rect.right),
        static_cast<LONG>(rect.bottom)
    };

    context_->RSSetScissorRects(1u, &r);
}

void d3d11_context::set_primitive_topology(primitive_topology t)
{
    D3D11_PRIMITIVE_TOPOLOGY tp;

    switch (t) {
    case primitive_topology::triangle_list:
        tp = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    case primitive_topology::line_list:
        tp = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        break;
    case primitive_topology::point_list:
        tp = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        break;
    default:
        return;
    }

    context_->IASetPrimitiveTopology(tp);
}

void d3d11_context::set_viewport(const viewport& v)
{
    D3D11_VIEWPORT vp{
        static_cast<FLOAT>(v.top_left_x),
        static_cast<FLOAT>(v.top_left_y),
        static_cast<FLOAT>(v.width),
        static_cast<FLOAT>(v.height),
        static_cast<FLOAT>(v.min_depth),
        static_cast<FLOAT>(v.max_depth)
    };

    context_->RSSetViewports(1u, &vp);
}

void d3d11_context::update_display_size(std::uint32_t width, std::uint32_t height)
{
    (void)width;
    (void)height;
}

void d3d11_context::backup_render_state()
{
}

void d3d11_context::restore_render_state()
{
}

void d3d11_context::setup_render_state()
{
    context_->GSSetShader(nullptr, nullptr, 0);
    context_->HSSetShader(nullptr, nullptr, 0);
    context_->DSSetShader(nullptr, nullptr, 0);
    context_->CSSetShader(nullptr, nullptr, 0);
}

r2_end_