#include <r2/renderer.h>
#include <r2/render_data.h>
#include <r2/error.h>
#include <r2/font/font_atlas.h>
#include <r2/font/font.h>
#include <backend/util.h>


r2_begin_

renderer::renderer()
{
}

renderer::~renderer()
{
    destroyed_.store(true, std::memory_order_release);
    if (update_thread_.joinable())
        update_thread_.join();
}

error renderer::init(const platform_init_data& pinit, const backend_init_data& binit)
{
    context_ = r2::context::make_context(pinit, binit, true);
    if (context_->has_error()) {
        return error(
            error_code::context_initialization,
            context_->get_error(),
            context_->get_detail()
        );
    }

    return do_init();
}

error renderer::init(r2::context* ctx)
{
    assert(ctx != nullptr);

    context_.reset(ctx);

    return do_init();
}

error renderer::do_init()
{
    assert((bool)!render_data_);
    render_data_ = std::make_unique<r2::render_data>();
    if (!font_atlas_)
        font_atlas_ = std::make_unique<r2::font_atlas>(this);

    // d3d11 does not require any bidning during resource creation
#if defined(R2_BACKEND_OPENGL)
    backup_render_state();
#endif // R2_BACKEND_OPENGL

    context_->acquire_backbuffer();
    if (context_->has_error()) {
        return error(
            error_code::context_backbuffer,
            context_->get_error(), context_->get_detail()
        );
    }

    const auto res = create_resources();
    if (res.get_code() != error_code::none) {
        return res;
    }

#if defined(R2_BACKEND_OPENGL)
    restore_render_state();
#endif // R2_BACKEND_OPENGL

    destroyed_.store(false, std::memory_order_release);
    update_thread_ = std::thread(
        [this]() {
            this->font_update_thread();
        }
    );

    if (!update_display_size()) {
        return error(error_code::context_backbuffer);
    }

    resources_created_ = true;

    return error(error_code::none);
}

void renderer::destroy()
{
    resources_created_ = false;
    is_initialized_ = false;
    destroyed_.store(true, std::memory_order_release);

    if (context_)
        context_->release_backbuffer();

    render_data_.reset();
    context_.reset();

    if (update_thread_.joinable())
        update_thread_.join();

    fonts_.clear();
    font_atlas_.reset();
}

bool renderer::build_fonts()
{
    assert(resources_created_ && "call init first");

    if (!font_atlas_->build()) {
        return false;
    }

    // fonts
    std::lock_guard<std::mutex> lock(font_mutex_);
    for (auto& font : fonts_) {
        if (!font->build(true /* initial build */)) {
            return false;
        }
    }

    is_initialized_ = true;

    return true;
}

bool renderer::create_font_texture()
{
    assert(font_atlas_->get_width() > 0 &&
           font_atlas_->get_height() > 0);
    assert(font_atlas_->get_width() * font_atlas_->get_height() == 
           font_atlas_->get_data32().size());

    texture_desc d{};
    d.width = font_atlas_->get_width();
    d.height = font_atlas_->get_height();
    d.usage = texture_usage::shader_resource;
    d.format = texture_format::rgba8_unorm;

    render_data_->font_texture = context_->create_texture2d(
        d, 
        font_atlas_->get_data32().data()
    );
    if (render_data_->font_texture->has_error()) {
        return false;
    }

    textureview_desc vd{};
    render_data_->font_view = context_->create_textureview(
        render_data_->font_texture.get(),
        vd
    );
    if (render_data_->font_view->has_error()) {
        return false;
    }

    font_atlas_->get_data32().clear();

    return true;
}

void renderer::pre_resize()
{
    assert(is_initialized());

    context_->release_backbuffer();
}

bool renderer::post_resize()
{
    assert(is_initialized());

    context_->acquire_backbuffer();
    if (context_->has_error()) {
        return false;
    }

    if (!update_display_size()) {
        return false;
    }

    return true;
}

bool renderer::update_display_size()
{
    display_size_ = r2::vec2(
        static_cast<float>(context_->get_backbuffer()->desc().width),
        static_cast<float>(context_->get_backbuffer()->desc().height)
    );

    if (display_size_.x == 0.f ||
        display_size_.y == 0.f) {
        return false;
    }

    vec4 cb_data(
        display_size_.x, display_size_.y,
        0.f, 0.f
    );
    render_data_->constant_buffer->update(&cb_data, sizeof(cb_data));

    return true;
}

void renderer::set_flags(renderer_flags f)
{
    flags_ = f;
}

font* renderer::add_font(const font_cfg& cfg)
{
#if defined(_DEBUG)
    assert_render_thread();
#endif

    std::lock_guard<std::mutex> lock(font_mutex_);
    fonts_.push_back(
        std::make_unique<font>(font_atlas_.get(), cfg)
    );

    return fonts_.back().get();
}

void renderer::remove_font(font* font)
{
#if defined(_DEBUG)
    assert_render_thread();
#endif

    std::lock_guard<std::mutex> lock(font_mutex_);
    for (auto it = fonts_.begin(); it != fonts_.end(); it++) {
        if (it->get() == font) {
            it->get()->destroy();
            fonts_.erase(it);
            break;
        }
    }
}

bool renderer::is_initialized()
{
    return is_initialized_;
}

void renderer::update_fonts_on_frame()
{
#if defined(_DEBUG)
    assert_render_thread();
#endif
    assert(render_data_);
    assert(render_data_->font_texture);

    atlas_update_queued_ = false;

    std::lock_guard<std::mutex> lock(font_mutex_);
    for (auto& font : fonts_) {
        font->update_on_render();
    }
}

void renderer::setup_render_state()
{
#if defined(_DEBUG)
    assert_render_thread();
#endif
    assert(context_);
    assert(render_data_);

    context_->set_primitive_topology(primitive_topology::triangle_list);
    context_->set_inputlayout(render_data_->input_layout.get());
    context_->set_shaderprogram(render_data_->shader.get());
    context_->set_uniform_buffer(
        render_data_->constant_buffer.get(), shader_bind_type::vs);

    context_->set_sampler(render_data_->sampler.get());

    context_->set_blendstate(render_data_->blend_state.get());
    context_->set_depthstencilstate(render_data_->depth_stencil_state.get());
    context_->set_rasterizerstate(render_data_->rasterizer_state.get());

    context_->setup_render_state();
}

void renderer::backup_render_state()
{
    context_->backup_render_state();
}

void renderer::restore_render_state()
{
    context_->restore_render_state();
}

void renderer::render(const drawlist_base& list)
{
#if defined(_DEBUG)
    assert_render_thread();
#endif
    assert(list.clip_rect_stack_.size() == 1u);
    assert(list.texture_stack_.size() == 1u);
    assert(list.font_stack_.size() == 1u);

    if (list.indices_.empty())
        return;

    // update buffers
    ensure_capacity(
        static_cast<std::uint32_t>(list.indices_.size()),
        static_cast<std::uint32_t>(list.vertices_.size())
    );

    render_data_->index_buffer->update(
        list.indices_.data(),
        list.indices_.size() * sizeof(index)
    );
    assert(!render_data_->index_buffer->has_error());

    render_data_->vertex_buffer->update(
        list.vertices_.data(),
        list.vertices_.size() * sizeof(vertex)
    );
    assert(!render_data_->vertex_buffer->has_error());

    // draw
    context_->set_vertex_buffer(render_data_->vertex_buffer.get());
    context_->set_index_buffer(render_data_->index_buffer.get());

    for (std::size_t i = 0u; i < list.cmds_.size(); i++) {
        const auto& cmd = list.cmds_[i];
        assert(cmd.texture != nullptr);

        if (cmd.clip_rect.left >= cmd.clip_rect.right ||
            cmd.clip_rect.top >= cmd.clip_rect.bottom) [[unlikely]]
            continue;

        const bool end = i == list.cmds_.size() - 1;
        const std::uint32_t index_end = end ?
            static_cast<std::uint32_t>(list.indices_.size()) : list.cmds_[i + 1].index_start;
        assert(index_end >= cmd.index_start);
        const std::uint32_t count = index_end - cmd.index_start;
        if (count == 0u)
            continue;

        assert(count % 3 == 0);

        // bind + draw
        context_->set_scissor_rect(cmd.clip_rect);

        context_->set_texture_native(
            cmd.texture,
            shader_bind_type::ps, 
            0u
        );

        context_->draw_indexed(
            count,
            cmd.index_start,
            cmd.vertex_start
        );
    }
}

void renderer::set_multisampled(bool multisample)
{
    context_->set_rasterizerstate(
        multisample ?
            render_data_->rasterizer_state_ms.get() : render_data_->rasterizer_state.get()
    );
}

void renderer::enable_depth(bool enabled)
{
    context_->set_depthstencilstate(
        enabled ?
            render_data_->depth_stencil_state_enabled.get() : render_data_->depth_stencil_state.get()
    );
}

#include "shader.h"

error renderer::create_resources()
{
    // create vertex shader
    vertex_attribute_desc vs_desc[] = {
        { "POSITION", 0, vertex_attribute_format::f32f32,         offsetof(vertex, pos),   false, 0 },
        { "TEXCOORD", 0, vertex_attribute_format::f32f32,         offsetof(vertex, uv),    false, 0 },
        { "TEXCOORD", 1, vertex_attribute_format::f32,            offsetof(vertex, depth), false, 0 },
        { "COLOR",    0, vertex_attribute_format::r8r8r8r8_unorm, offsetof(vertex, col),   false, 0 },
    };

    std::unique_ptr<compiled_shader> vs_data = context_->compile_vertexshader(
        &vs_source[0], sizeof(vs_source));
    if (vs_data->has_error()) {
        return error(
            error_code::vertex_shader_compile,
            vs_data->get_error(), 
            vs_data->get_detail()
        );
    }

    std::unique_ptr<vertexshader> vs(context_->create_vertexshader(
        vs_data->data(), vs_data->size()));
    if (vs->has_error()) {
        return error(
            error_code::vertex_shader_create,
            vs->get_error(),
            vs->get_detail()
        );
    }

    render_data_->input_layout = context_->create_inputlayout(vs_desc, v_count_of(vs_desc),
        vs_data->data(), vs_data->size());
    if (render_data_->input_layout->has_error()) {
        return error(
            error_code::input_layout_create,
            render_data_->input_layout->get_error(), 
            render_data_->input_layout->get_detail()
        );
    }

    // create pixel shader
    std::unique_ptr<compiled_shader> ps_data = context_->compile_pixelshader(
        &ps_source[0], sizeof(ps_source));
    if (ps_data->has_error())
        return error(error_code::vertex_shader_compile,
            ps_data->get_error(), ps_data->get_detail());

    std::unique_ptr<pixelshader> ps(context_->create_pixelshader(
        ps_data->data(), ps_data->size()));
    if (ps->has_error()) {
        return error(
            error_code::pixel_shader_create,
            ps->get_error(),
            ps->get_detail()
        );
    }

    render_data_->shader = context_->create_shaderprogram(vs.get(), ps.get());
    if (render_data_->shader->has_error()) {
        return error(
            error_code::shader_program_create,
            render_data_->shader->get_error(),
            render_data_->shader->get_detail()
        );
    }

    // create constant buffer
    buffer_desc cbdesc;
    cbdesc.size_bytes = sizeof(vec4);
    cbdesc.dynamic    = false;
    cbdesc.usage      = buffer_usage::uniform;

    render_data_->constant_buffer = context_->create_buffer(cbdesc);
    if (render_data_->constant_buffer->has_error()) {
        return error(
            error_code::constant_buffer_create,
            render_data_->constant_buffer->get_error(), 
            render_data_->constant_buffer->get_detail()
        );
    }

    // create blend state
    blendstate_desc bdesc;
    bdesc.independent_blend_enable = false;
    bdesc.alpha_to_coverage_enable = false;
    bdesc.targets[0].blend_enable  = true;
    bdesc.targets[0].src_color_factor = blend_factor::src_alpha;
    bdesc.targets[0].dst_color_factor = blend_factor::inv_src_alpha;
    bdesc.targets[0].color_op = blend_op::add;
    bdesc.targets[0].src_alpha_factor = blend_factor::one;
    bdesc.targets[0].dst_alpha_factor = blend_factor::inv_src_alpha;
    bdesc.targets[0].alpha_op   = blend_op::add;
    bdesc.targets[0].write_mask = color_write_mask::all;

    render_data_->blend_state = context_->create_blendstate(bdesc);
    if (render_data_->blend_state->has_error()) {
        return error(
            error_code::blend_state_create,
            render_data_->blend_state->get_error(),
            render_data_->blend_state->get_detail()
        );
    }

    // create rasterizer state
    rasterizerstate_desc rdesc;
    rdesc.fill = fill_mode::solid;
    rdesc.cull = cull_mode::none;
    rdesc.scissor_enable = true;
    rdesc.depth_clip_enable = false;

    render_data_->rasterizer_state = context_->create_rasterizerstate(rdesc);
    if (render_data_->rasterizer_state->has_error()) {
        return error(
            error_code::rasterizer_state_create,
            render_data_->rasterizer_state->get_error(),
            render_data_->rasterizer_state->get_detail()
        );
    }

    rdesc.multisample_enable = true;

    render_data_->rasterizer_state_ms = context_->create_rasterizerstate(rdesc);
    if (render_data_->rasterizer_state_ms->has_error()) {
        return error(
            error_code::rasterizer_state_create,
            render_data_->rasterizer_state_ms->get_error(),
            render_data_->rasterizer_state_ms->get_detail()
        );
    }

    // create depth stencil state
    depthstencilstate_desc ddesc;
    ddesc.depth_enable = false;
    ddesc.depth_write = false;
    ddesc.depth_func = comparison_func::always;
    ddesc.stencil_enable = false;
    ddesc.front_face.func = comparison_func::always;
    ddesc.front_face.depth_fail_op =
        ddesc.front_face.fail_op =
        ddesc.front_face.pass_op = stencil_op::keep;
    ddesc.back_face = ddesc.front_face;

    render_data_->depth_stencil_state = context_->create_depthstencilstate(ddesc);
    if (render_data_->depth_stencil_state->has_error()) {
        return error(
            error_code::depth_stencil_state_create,
            render_data_->depth_stencil_state->get_error(), 
            render_data_->depth_stencil_state->get_detail()
        );
    }

    ddesc.depth_enable = true;
    ddesc.depth_write = false;
    ddesc.depth_func = comparison_func::less_equal;

    render_data_->depth_stencil_state_enabled = context_->create_depthstencilstate(ddesc);
    if (render_data_->depth_stencil_state_enabled->has_error()) {
        return error(
            error_code::depth_stencil_state_create,
            render_data_->depth_stencil_state_enabled->get_error(),
            render_data_->depth_stencil_state_enabled->get_detail()
        );
    }

    // create texture sampler
    sampler_desc sdesc;
    sdesc.compare_func = sampler_compare_func::none;
    sdesc.address_u =
        sdesc.address_v =
        sdesc.address_w = sampler_address_mode::clamp_to_edge;
    sdesc.filter  = sampler_filter::linear;

    render_data_->sampler = context_->create_sampler(sdesc);
    if (render_data_->sampler->has_error()) {
        return error(
            error_code::sampler_create,
            render_data_->sampler->get_error(), render_data_->sampler->get_detail()
        );
    }

    return error(error_code::none);
}

void renderer::ensure_capacity(std::uint32_t num_indices, std::uint32_t num_vertices)
{
    if (render_data_->index_count < num_indices ||
        !render_data_->index_buffer) {
        render_data_->index_buffer.reset();

        std::uint32_t count = num_indices > render_data_->index_count ?
            num_indices : render_data_->index_count;

        buffer_desc d{};
        d.usage = buffer_usage::index;
        d.dynamic = true;
        d.size_bytes = count * sizeof(index);
        d.ib_type = sizeof(index) == sizeof(std::uint32_t) ?
            index_buffer_type::u32 : index_buffer_type::u16;

        render_data_->index_buffer = context_->create_buffer(d);
        assert(!render_data_->index_buffer->has_error());
    }

    if (render_data_->vertex_count < num_vertices ||
        !render_data_->vertex_buffer) {
        render_data_->vertex_buffer.reset();

        std::uint32_t count = num_vertices > render_data_->vertex_count ?
            num_vertices : render_data_->vertex_count;

        buffer_desc d{};
        d.usage = buffer_usage::vertex;
        d.dynamic = true;
        d.size_bytes = count * sizeof(vertex);
        d.vb_stride = sizeof(vertex);

        render_data_->vertex_buffer = context_->create_buffer(d);
        assert(!render_data_->vertex_buffer->has_error());

        render_data_->input_layout->link(render_data_->vertex_buffer.get());
        assert(!render_data_->input_layout->has_error());
    }
}

void renderer::font_update_thread()
{
    while (!destroyed_.load(std::memory_order_acquire)) {
        {
            std::lock_guard<std::mutex> lock(font_mutex_);
            for (auto& font : fonts_) {
                font->update_worker();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

texture_handle renderer::font_texture() const noexcept
{
    assert(render_data_);
    assert(render_data_->font_view);
    return render_data_->font_view->native_texture_handle();
}

r2_end_
