#include <r2/renderer.h>
#include <r2/render_data.h>
#include <r2/error.h>
#include <r2/font/font_atlas.h>


r2_begin_

renderer2d::renderer2d()
{
}

renderer2d::~renderer2d()
{
}

void renderer2d::init(const platform_init_data& pinit, const backend_init_data& binit)
{
    context_ = r2::context::make_context(pinit, binit, true);
    if (context_->has_error()) {
        throw error(
            error_code::context_initialization,
            context_->get_error(),
            context_->get_detail()
        );
    }

    do_init();
}

void renderer2d::init(r2::context* ctx)
{
    assert(ctx != nullptr);

    context_.reset(ctx);

    do_init();
}

void renderer2d::do_init()
{
    render_data_ = std::make_unique<r2::render_data>();
    font_atlas_ = std::make_unique<r2::font_atlas>(this);

    backup_render_state();

    context_->acquire_backbuffer();
    if (context_->has_error())
        throw error(error_code::blend_state_create,
            context_->get_error(), context_->get_detail());

    try {
        create_resources();
    }
    catch (const error& e) {
        restore_render_state();
        throw e;
    }

    restore_render_state();

    destroyed_.store(false, std::memory_order_release);
    update_thread_ = std::thread([this]()
        {
            this->font_update_thread();
        });

    update_display_size();

    resources_created_ = true;
}

void renderer2d::destroy()
{
    resources_created_ = false;
    is_initialized_ = true;
    destroyed_.store(true, std::memory_order_release);

    context_->release_backbuffer();

    render_data_.reset();
    context_.reset();

    if (update_thread_.joinable())
        update_thread_.join();
    
    font_atlas_.reset();
}

void renderer2d::build_fonts()
{
    assert(resources_created_ && "call init first");

    if (!font_atlas_->build())
        throw error(error_code::font_build);

    // fonts
    for (auto& font : fonts_) {
        if (!font->build())
            throw error(error_code::font_build);
    }

    is_initialized_ = true;
}

void renderer2d::create_font_texture()
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
        throw error(error_code::font_tex_create,
            render_data_->font_texture->get_error(),
            render_data_->font_texture->get_detail()
        );
    }

    textureview_desc vd{};
    render_data_->font_view = context_->create_textureview(
        render_data_->font_texture.get(),
        vd
    );
    if (render_data_->font_view->has_error()) {
        throw error(error_code::font_tex_create,
            render_data_->font_view->get_error(),
            render_data_->font_view->get_detail()
        );
    }
}

void renderer2d::pre_resize()
{
    assert(is_initialized());

    context_->release_backbuffer();
}

void renderer2d::post_resize()
{
    assert(is_initialized());

    context_->acquire_backbuffer();
    update_display_size();
}

void renderer2d::update_display_size()
{
    display_size_ = r2::vec2(
        static_cast<float>(context_->get_backbuffer()->desc().width),
        static_cast<float>(context_->get_backbuffer()->desc().height)
    );

    vec4 cb_data(
        display_size_.x, display_size_.y,
        0.f, 0.f
    );
    render_data_->constant_buffer->update(&cb_data, sizeof(cb_data));
}

void renderer2d::set_flags(renderer_flags f)
{
    flags_ = f;
}

font* renderer2d::add_font(const font_cfg& cfg)
{
    fonts_.push_back(std::make_unique<font>(font_atlas_.get(), cfg));

    if (current_font_ == nullptr)
        current_font_ = fonts_.back().get();
    if (font_stack_.empty())
        font_stack_.push_back(fonts_.back().get());

    return fonts_.back().get();
}

bool renderer2d::is_initialized()
{
    return is_initialized_;
}

void renderer2d::update_fonts_on_frame()
{
    assert(render_data_);
    assert(render_data_->font_texture);

    bool update_tex = atlas_update_queued_;
    atlas_update_queued_ = false;
    for (auto& font : fonts_)
        if (font->update_on_render())
            update_tex = true;

    if (update_tex) {
        render_data_->font_texture->update(
            font_atlas_->get_data32().data(), 
            static_cast<std::uint32_t>(
                font_atlas_->get_width() * sizeof(std::uint32_t))
        );
    }
}

void renderer2d::setup_render_state()
{
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

void renderer2d::backup_render_state()
{
    context_->backup_render_state();
}

void renderer2d::restore_render_state()
{
    context_->restore_render_state();
}

void renderer2d::reset_render_data()
{
    assert(render_data_->font_view);

    aa_scale_ = 1.f;

    vertices_.clear();
    indices_.clear();
    cmds_.clear();
    clip_rect_stack_.clear();
    texture_stack_.clear();

    add_draw_cmd();
    push_clip_rect(
        rect(
            0, 0,
            static_cast<std::int32_t>(display_size_.x),
            static_cast<std::int32_t>(display_size_.y)
        ),
        false
    );
    push_texture_id(
        render_data_->font_view.get()
    );
}

void renderer2d::render()
{
    assert(clip_rect_stack_.size() == 1u);
    assert(texture_stack_.size() == 1u);

    if (indices_.empty())
        return;

    // update buffers
    ensure_capacity(
        static_cast<std::uint32_t>(indices_.size()),
        static_cast<std::uint32_t>(vertices_.size())
    );

    render_data_->index_buffer->update(
        indices_.data(),
        indices_.size() * sizeof(index)
    );
    assert(!render_data_->index_buffer->has_error());

    render_data_->vertex_buffer->update(
        vertices_.data(),
        vertices_.size() * sizeof(vertex)
    );
    assert(!render_data_->vertex_buffer->has_error());

    // draw
    context_->set_vertex_buffer(render_data_->vertex_buffer.get());
    context_->set_index_buffer(render_data_->index_buffer.get());

    for (std::size_t i = 0u; i < cmds_.size(); i++) {
        const auto& cmd = cmds_[i];
        assert(cmd.texture != nullptr);

        if (cmd.clip_rect.left >= cmd.clip_rect.right ||
            cmd.clip_rect.top >= cmd.clip_rect.bottom) [[unlikely]]
            continue;

        const bool end = i == cmds_.size() - 1;
        const std::uint32_t index_end = end ?
            static_cast<std::uint32_t>(indices_.size()) : cmds_[i + 1].index_start;
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

void renderer2d::set_multisampled(bool multisample)
{
    multisample ?
        context_->set_rasterizerstate(render_data_->rasterizer_state_ms.get()) :
        context_->set_rasterizerstate(render_data_->rasterizer_state.get());
}

#include "shader.h"

void renderer2d::create_resources()
{
    // Create vertex shader
    vertex_attribute_desc vs_desc[] = {
        { "POSITION", vertex_attribute_format::f32f32,         offsetof(vertex, pos), false, 0 },
        { "TEXCOORD", vertex_attribute_format::f32f32,         offsetof(vertex, uv),  false, 0 },
        { "COLOR",    vertex_attribute_format::r8r8r8r8_unorm, offsetof(vertex, col), false, 0 },
    };

    std::unique_ptr<compiled_shader> vs_data = context_->compile_vertexshader(
        &vs_source[0], sizeof(vs_source));
    if (vs_data->has_error())
        throw error(error_code::vertex_shader_compile,
            vs_data->get_error(), vs_data->get_detail());

    std::unique_ptr<vertexshader> vs(context_->create_vertexshader(
        vs_data->data(), vs_data->size()));
    if (vs->has_error())
        throw error(error_code::vertex_shader_create,
            vs->get_error(), vs->get_detail());

    render_data_->input_layout = context_->create_inputlayout(vs_desc, _countof(vs_desc),
        vs_data->data(), vs_data->size());
    if (render_data_->input_layout->has_error())
        throw error(error_code::input_layout_create,
            render_data_->input_layout->get_error(), render_data_->input_layout->get_detail());

    std::unique_ptr<compiled_shader> ps_data = context_->compile_pixelshader(
        &ps_source[0], sizeof(ps_source));
    if (ps_data->has_error())
        throw error(error_code::vertex_shader_compile,
            ps_data->get_error(), ps_data->get_detail());

    std::unique_ptr<pixelshader> ps(context_->create_pixelshader(
        ps_data->data(), ps_data->size()));
    if (ps->has_error())
        throw error(error_code::pixel_shader_create,
            ps->get_error(), ps->get_detail());

    render_data_->shader = context_->create_shaderprogram(vs.get(), ps.get());
    if (render_data_->shader->has_error())
        throw error(error_code::shader_program_create, 
            render_data_->shader->get_error(), render_data_->shader->get_detail());

    // Create constant buffer
    buffer_desc cbdesc;
    cbdesc.size_bytes = sizeof(vec4);
    cbdesc.dynamic    = false;
    cbdesc.usage      = buffer_usage::uniform;

    render_data_->constant_buffer = context_->create_buffer(cbdesc);
    if (render_data_->constant_buffer->has_error())
        throw error(error_code::constant_buffer_create,
            render_data_->constant_buffer->get_error(), render_data_->constant_buffer->get_detail());

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
    if (render_data_->blend_state->has_error())
        throw error(error_code::blend_state_create,
            render_data_->blend_state->get_error(), render_data_->blend_state->get_detail());

    rasterizerstate_desc rdesc;
    rdesc.fill = fill_mode::solid;
    rdesc.cull = cull_mode::none;
    rdesc.scissor_enable = true;
    rdesc.depth_clip_enable = true;

    render_data_->rasterizer_state = context_->create_rasterizerstate(rdesc);
    if (render_data_->rasterizer_state->has_error())
        throw error(error_code::rasterizer_state_create,
            render_data_->rasterizer_state->get_error(), render_data_->rasterizer_state->get_detail());

    rdesc.multisample_enable = true;

    render_data_->rasterizer_state_ms = context_->create_rasterizerstate(rdesc);
    if (render_data_->rasterizer_state_ms->has_error())
        throw error(error_code::rasterizer_state_create,
            render_data_->rasterizer_state_ms->get_error(), render_data_->rasterizer_state_ms->get_detail());

    depthstencilstate_desc ddesc;
    ddesc.depth_enable = false;
    ddesc.depth_write  = true;
    ddesc.depth_func = comparison_func::always;
    ddesc.stencil_enable  = false;
    ddesc.front_face.func = comparison_func::always;
    ddesc.front_face.depth_fail_op =
        ddesc.front_face.fail_op   =
        ddesc.front_face.pass_op   = stencil_op::keep;
    ddesc.back_face = ddesc.front_face;

    render_data_->depth_stencil_state = context_->create_depthstencilstate(ddesc);
    if (render_data_->depth_stencil_state->has_error())
        throw error(error_code::depth_stencil_state_create,
            render_data_->depth_stencil_state->get_error(), render_data_->depth_stencil_state->get_detail());

    // Create texture sampler
    sampler_desc sdesc;
    sdesc.compare_func = sampler_compare_func::none;
    sdesc.address_u =
        sdesc.address_v =
        sdesc.address_w = sampler_address_mode::clamp_to_edge;
    sdesc.filter  = sampler_filter::linear;

    render_data_->sampler = context_->create_sampler(sdesc);
    if (render_data_->sampler->has_error())
        throw error(error_code::sampler_create,
            render_data_->sampler->get_error(), render_data_->sampler->get_detail());
}

void renderer2d::ensure_capacity(std::uint32_t num_indices, std::uint32_t num_vertices)
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
        d.ib_type = sizeof(index) == 4u ?
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

void renderer2d::font_update_thread()
{
    while (!destroyed_.load(std::memory_order_acquire)) {
        for (auto& font : fonts_) {
            font->update_worker();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

texture_handle renderer2d::font_texture() const noexcept
{
    assert(render_data_);
    assert(render_data_->font_view);
    return render_data_->font_view->native_texture_handle();
}

r2_end_
