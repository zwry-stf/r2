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

struct backup_render_data {
    bool captured = false;

    std::uint32_t  scissor_rects_count, viewports_count;
    D3D11_RECT     scissor_rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];

    d3d_pointer<ID3D11RasterizerState> rasterizer_state;

    d3d_pointer<ID3D11BlendState> blend_state;
    FLOAT blend_factor[4];
    std::uint32_t sample_mask;

    std::uint32_t stencil_ref;
    d3d_pointer<ID3D11DepthStencilState> depth_stencil_state;

    d3d_pointer<ID3D11ShaderResourceView> ps_shader_resource;
    d3d_pointer<ID3D11SamplerState>       ps_sampler;

    d3d_pointer<ID3D11PixelShader>  pixel_shader;
    d3d_pointer<ID3D11VertexShader> vertex_shader;

    std::uint32_t        ps_instances_count, vs_instances_count;
    ID3D11ClassInstance* ps_instances[256], * vs_instances[256];

    D3D11_PRIMITIVE_TOPOLOGY primitive_topology;

    d3d_pointer<ID3D11Buffer> index_buffer, vertex_buffer, constant_buffer;
    std::uint32_t             index_buffer_offset, vertex_buffer_stride, vertex_buffer_offset;
    DXGI_FORMAT               index_buffer_format;

    d3d_pointer<ID3D11InputLayout> input_layout;
};

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

    backup_data_ = std::make_unique<backup_render_data>();
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

void d3d11_context::copy_subresource(framebuffer* dst, const framebuffer* src,
                                     const rect& src_rect, const rect& dst_rect)
{
    assert(dst_rect.right - dst_rect.left == src_rect.right - src_rect.left);
    assert(dst_rect.bottom - dst_rect.top == src_rect.bottom - src_rect.top);
    assert(src != nullptr);
    assert(dst != nullptr);

    const auto* src_texture = to_native(src->desc().color_attachment.view);
    auto* dst_texture = to_native(dst->desc().color_attachment.view);
    assert(src_texture != nullptr);
    assert(dst_texture != nullptr);

    D3D11_BOX box{ 
        static_cast<UINT>(src_rect.left), 
        static_cast<UINT>(src_rect.top),
        0u, /* front */
        static_cast<UINT>(src_rect.right),
        static_cast<UINT>(src_rect.bottom),
        1u /* back */
    };
    context_->CopySubresourceRegion(
        dst_texture->resource()->texture(), 0u,
        static_cast<UINT>(dst_rect.left),
        static_cast<UINT>(dst_rect.top),
        0u, /* dst z */
        src_texture->resource()->texture(),
        0u,
        &box
    );
}

DXGI_FORMAT d3d11_context::get_format_no_srgb(DXGI_FORMAT curr) noexcept
{
    switch (curr) {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_BC1_UNORM_SRGB:
        return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_UNORM_SRGB:
        return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_UNORM_SRGB:
        return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return DXGI_FORMAT_BC7_UNORM;
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    }

    return curr;
}

void d3d11_context::resolve_subresource(framebuffer* dst, const framebuffer* src, std::optional<texture_format> format,
                                        const rect& src_rect, const rect& dst_rect)
{
    assert(dst_rect.right - dst_rect.left == src_rect.right - src_rect.left);
    assert(dst_rect.bottom - dst_rect.top == src_rect.bottom - src_rect.top);
    assert(src != nullptr);
    assert(dst != nullptr);

    (void)src_rect;
    (void)dst_rect;

    const auto* src_texture = to_native(src->desc().color_attachment.view);
    auto* dst_texture = to_native(dst->desc().color_attachment.view);
    assert(src_texture != nullptr);
    assert(dst_texture != nullptr);

    DXGI_FORMAT fmt;
    if (!format.has_value()) {
        if (dst_texture->resource() == backbuffer_.get()) {
            fmt = backbuffer_format_no_srgb_; // use cached value for performance
        }
        else {
            D3D11_TEXTURE2D_DESC d;
            dst_texture->resource()->texture()->GetDesc(&d);

            fmt = get_format_no_srgb(d.Format);
        }
    }
    else {
        fmt = d3d11_texture2d::to_dxgi_format(format.value());
    }

    context_->ResolveSubresource(
        dst_texture->resource()->texture(),
        0,
        src_texture->resource()->texture(),
        0,
        fmt
    );
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

std::unique_ptr<compiled_shader> d3d11_context::compile_vertexshader(const char* source, std::size_t length)
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

std::unique_ptr<compiled_shader> d3d11_context::compile_pixelshader(const char* source, std::size_t length)
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

void d3d11_context::acquire_backbuffer()
{
    d3d_pointer<ID3D11Texture2D> back_buffer;
    HRESULT res = sc_->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
    if (FAILED(res)) {
        set_error(
            std::to_underlying(d3d11_context_error::backbuffer),
            res
        );
        return;
    }

    backbuffer_ = d3d11_texture2d::from_existing(this, back_buffer.get());
    if (backbuffer_->has_error()) {
        set_error(
            std::to_underlying(d3d11_context_error::backbuffer),
            backbuffer_->get_detail()
        );
    }

    D3D11_TEXTURE2D_DESC d;
    back_buffer->GetDesc(&d);

    backbuffer_format_no_srgb_ = get_format_no_srgb(d.Format);
}

std::unique_ptr<textureview> d3d11_context::create_textureview(texture2d* tex, const textureview_desc& desc)
{
    return std::make_unique<d3d11_textureview>(this, to_native(tex), desc);
}

std::unique_ptr<framebuffer> d3d11_context::create_framebuffer(const framebuffer_desc& desc)
{
    return std::make_unique<d3d11_framebuffer>(this, desc);
}

void d3d11_context::set_blendstate(const blendstate* bs, const float(&factor)[4], std::uint32_t sample_mask)
{
    auto* state = bs == nullptr ? nullptr : to_native(bs)->state();

    FLOAT cfactor[4] = {
        static_cast<FLOAT>(factor[0]),
        static_cast<FLOAT>(factor[1]),
        static_cast<FLOAT>(factor[2]),
        static_cast<FLOAT>(factor[3]),
    };

    context_->OMSetBlendState(
        state,
        &cfactor[0], 
        static_cast<UINT>(sample_mask)
    );
}

void d3d11_context::set_depthstencilstate(const depthstencilstate* ds, std::uint32_t stencil_ref)
{
    auto* state = ds == nullptr ? nullptr : to_native(ds)->state();

    context_->OMSetDepthStencilState(
        state,
        static_cast<UINT>(stencil_ref)
    );
}

void d3d11_context::set_inputlayout(const inputlayout* il)
{
    auto* layout = il == nullptr ? nullptr : to_native(il)->layout();

    context_->IASetInputLayout(layout);
}

void d3d11_context::set_rasterizerstate(const rasterizerstate* rs)
{
    auto* state = rs == nullptr ? nullptr : to_native(rs)->state();

    context_->RSSetState(state);
}

void d3d11_context::set_shaderprogram(const shaderprogram* s)
{
    auto* vs = s == nullptr ? nullptr : to_native(s)->vs();
    auto* ps = s == nullptr ? nullptr : to_native(s)->ps();

    context_->VSSetShader(vs, nullptr, 0u);
    context_->PSSetShader(ps, nullptr, 0u);
}

/// bind

void d3d11_context::set_vertex_buffer(const buffer* vb, std::uint32_t slot)
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

void d3d11_context::set_index_buffer(const buffer* ib)
{
    assert(ib == nullptr || ib->desc().usage == buffer_usage::index);

    context_->IASetIndexBuffer(
        ib == nullptr ? nullptr : to_native(ib)->buffer(),
        ib->desc().ib_type == index_buffer_type::u16 ?
            DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
        0u
    );
}

void d3d11_context::set_uniform_buffer(const buffer* ub, shader_bind_type stage, std::uint32_t slot)
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

void d3d11_context::set_texture(const textureview* srv, shader_bind_type stage, std::uint32_t slot)
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

void d3d11_context::set_sampler(const sampler* s, shader_bind_type stage, std::uint32_t slot)
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

void d3d11_context::set_framebuffer(const framebuffer* fb)
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

void d3d11_context::clear_framebuffer(const framebuffer* fb)
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

void d3d11_context::backup_render_state()
{
    assert(!backup_data_->captured);

    // Grab scissor rects / viewports
    backup_data_->scissor_rects_count =
        backup_data_->viewports_count =
            D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

    get_context()->RSGetScissorRects(
        &backup_data_->scissor_rects_count,
        backup_data_->scissor_rects
    );
    get_context()->RSGetViewports(
        &backup_data_->viewports_count,
        backup_data_->viewports
    );

    // Rasterizer / blend / depth-stencil state
    get_context()->RSGetState(backup_data_->rasterizer_state.address_of());
    get_context()->OMGetBlendState(
        backup_data_->blend_state.address_of(),
        backup_data_->blend_factor,
        &backup_data_->sample_mask
    );
    get_context()->OMGetDepthStencilState(
        backup_data_->depth_stencil_state.address_of(),
        &backup_data_->stencil_ref
    );

    // PS resources
    get_context()->PSGetShaderResources(0, 1, backup_data_->ps_shader_resource.address_of());
    get_context()->PSGetSamplers(0, 1, backup_data_->ps_sampler.address_of());

    // Shaders + class instances
    backup_data_->ps_instances_count =
        backup_data_->vs_instances_count =
            static_cast<std::uint32_t>(_countof(backup_data_->ps_instances));

    get_context()->PSGetShader(
        backup_data_->pixel_shader.address_of(),
        backup_data_->ps_instances,
        &backup_data_->ps_instances_count
    );
    get_context()->VSGetShader(
        backup_data_->vertex_shader.address_of(),
        backup_data_->vs_instances,
        &backup_data_->vs_instances_count
    );

    // Constant buffer
    get_context()->VSGetConstantBuffers(
        0u,
        1u,
        backup_data_->constant_buffer.address_of()
    );

    // IA state
    get_context()->IAGetPrimitiveTopology(&backup_data_->primitive_topology);
    get_context()->IAGetIndexBuffer(
        backup_data_->index_buffer.address_of(),
        &backup_data_->index_buffer_format,
        &backup_data_->index_buffer_offset
    );
    get_context()->IAGetVertexBuffers(
        0,
        1,
        backup_data_->vertex_buffer.address_of(),
        &backup_data_->vertex_buffer_stride,
        &backup_data_->vertex_buffer_offset
    );
    get_context()->IAGetInputLayout(backup_data_->input_layout.address_of());

    backup_data_->captured = true;
}

void d3d11_context::restore_render_state()
{
    assert(backup_data_->captured);
    backup_data_->captured = false;

    auto release_instances = [](ID3D11ClassInstance** instances, std::uint32_t& count)
    {
        for (std::uint32_t i = 0; i < count; ++i) {
            if (instances[i]) {
                instances[i]->Release();
                instances[i] = nullptr;
            }
        }
        count = 0u;
    };

    get_context()->RSSetScissorRects(
        backup_data_->scissor_rects_count,
        backup_data_->scissor_rects
    );
    get_context()->RSSetViewports(
        backup_data_->viewports_count,
        backup_data_->viewports
    );

    get_context()->RSSetState(backup_data_->rasterizer_state);
    backup_data_->rasterizer_state.reset();

    get_context()->OMSetBlendState(
        backup_data_->blend_state,
        backup_data_->blend_factor,
        backup_data_->sample_mask
    );
    backup_data_->blend_state.reset();

    get_context()->OMSetDepthStencilState(
        backup_data_->depth_stencil_state,
        backup_data_->stencil_ref
    );
    backup_data_->depth_stencil_state.reset();

    get_context()->PSSetShaderResources(
        0u,
        1u,
        backup_data_->ps_shader_resource.address_of()
    );
    backup_data_->ps_shader_resource.reset();

    get_context()->PSSetSamplers(
        0u,
        1u,
        backup_data_->ps_sampler.address_of()
    );
    backup_data_->ps_sampler.reset();

    get_context()->PSSetShader(
        backup_data_->pixel_shader,
        backup_data_->ps_instances,
        backup_data_->ps_instances_count
    );
    backup_data_->pixel_shader.reset();
    release_instances(backup_data_->ps_instances, backup_data_->ps_instances_count);

    get_context()->VSSetShader(
        backup_data_->vertex_shader,
        backup_data_->vs_instances,
        backup_data_->vs_instances_count
    );
    backup_data_->vertex_shader.reset();
    release_instances(backup_data_->vs_instances, backup_data_->vs_instances_count);

    get_context()->VSSetConstantBuffers(
        0u,
        1u,
        backup_data_->constant_buffer.address_of()
    );
    backup_data_->constant_buffer.reset();

    get_context()->IASetPrimitiveTopology(backup_data_->primitive_topology);

    get_context()->IASetIndexBuffer(
        backup_data_->index_buffer,
        backup_data_->index_buffer_format,
        backup_data_->index_buffer_offset
    );
    backup_data_->index_buffer.reset();
    backup_data_->index_buffer_format = DXGI_FORMAT_UNKNOWN;
    backup_data_->index_buffer_offset = 0;

    get_context()->IASetVertexBuffers(
        0u,
        1u,
        backup_data_->vertex_buffer.address_of(),
        &backup_data_->vertex_buffer_stride,
        &backup_data_->vertex_buffer_offset
    );
    backup_data_->vertex_buffer.reset();
    backup_data_->vertex_buffer_stride = 0;
    backup_data_->vertex_buffer_offset = 0;

    get_context()->IASetInputLayout(backup_data_->input_layout);
    backup_data_->input_layout.reset();

    backup_data_->scissor_rects_count = 0u;
    backup_data_->viewports_count     = 0u;
    backup_data_->primitive_topology  = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    backup_data_->sample_mask         = 0u;
    backup_data_->stencil_ref         = 0u;
}

void d3d11_context::setup_render_state()
{
    context_->GSSetShader(nullptr, nullptr, 0);
    context_->HSSetShader(nullptr, nullptr, 0);
    context_->DSSetShader(nullptr, nullptr, 0);
    context_->CSSetShader(nullptr, nullptr, 0);
}

r2_end_