#include <backend/opengl/context.h>
#include <assert.h>

#if defined(R2_PLATFORM_WINDOWS)
#include <Windows.h>
#endif

#include <gl/GL.h>
#include <gl/glew.h>
#include <gl/wglext.h>
#include <backend/opengl/object.h>

//
#include <backend/opengl/blendstate.h>
#include <backend/opengl/buffer.h>
#include <backend/opengl/depthstencilstate.h>
#include <backend/opengl/inputlayout.h>
#include <backend/opengl/pixelshader.h>
#include <backend/opengl/rasterizerstate.h>
#include <backend/opengl/sampler.h>
#include <backend/opengl/shaderprogram.h>
#include <backend/opengl/texture2d.h>
#include <backend/opengl/textureview.h>
#include <backend/opengl/vertexshader.h>
#include <backend/opengl/compiled_shader.h>
#include <backend/opengl/framebuffer.h>


r2_begin_

struct backup_render_data {
    bool captured = false;

    GLint      last_program;
    GLint      last_active_texture;
    GLint      last_texture;
    GLint      last_sampler;
    GLint      last_array_buffer;
    GLint      last_vertex_array;
    GLint      last_polygon_mode[2];
    GLint      last_viewport[4];
    GLint      last_scissor_box[4];
    GLfloat    last_depth_range[2];

    GLint      last_blend_src_rgb;
    GLint      last_blend_dst_rgb;
    GLint      last_blend_src_alpha;
    GLint      last_blend_dst_alpha;
    GLint      last_blend_eq_rgb;
    GLint      last_blend_eq_alpha;
    GLfloat    last_blend_color[4];
    GLuint     last_sample_mask;

    GLint      last_fbo;
    GLint      last_draw_fbo;
    GLint      last_read_fbo;

    GLboolean  last_enable_blend;
    GLboolean  last_enable_cull_face;
    GLboolean  last_enable_depth_test;
    GLboolean  last_enable_stencil_test;
    GLboolean  last_enable_scissor_test;
    GLboolean  last_enable_color_op;
    GLboolean  last_depth_clamp;
    GLboolean  last_multisample;
    GLboolean  last_line_smooth;

    GLint      last_stencil_func_front;
    GLint      last_stencil_ref_front;
    GLuint     last_stencil_valuemask_front;
    GLint      last_stencil_sfail_front;
    GLint      last_stencil_dpfail_front;
    GLint      last_stencil_dppass_front;
    GLuint     last_stencil_writemask_front;

    GLint      last_stencil_func_back;
    GLint      last_stencil_ref_back;
    GLuint     last_stencil_valuemask_back;
    GLint      last_stencil_sfail_back;
    GLint      last_stencil_dpfail_back;
    GLint      last_stencil_dppass_back;
    GLuint     last_stencil_writemask_back;

    GLboolean  last_poly_offset_fill;
    GLint      last_cull_face_mode;
    GLint      last_front_face;
    GLfloat    last_poly_offset_factor;
    GLfloat    last_poly_offset_units;

    GLint      last_depth_func;

    GLboolean  last_color_mask[4];
    GLboolean  last_depth_mask;

    GLuint     last_uniform_buffer_0;
};

gl_context::gl_context(const platform_init_data& pinit, bool common_origin)
    : r2::context(pinit),
      common_origin_(common_origin)
{
    glewExperimental = true;
    GLenum res = glewInit();
    if (res != GLEW_OK) {
        set_error(
            std::to_underlying(gl_context_error::glew_init), 
            static_cast<std::int32_t>(res)
        );
        return;
    }

    // version
    glGetIntegerv(GL_MAJOR_VERSION, &major_);
    glGetIntegerv(GL_MINOR_VERSION, &minor_);

    backup_data_ = std::make_unique<backup_render_data>();
}

/// get

void gl_context::copy_subresource(framebuffer* dst, const framebuffer* src,
                                  const rect& src_rect, const rect& dst_rect)
{
    assert(dst_rect.right - dst_rect.left == src_rect.right - src_rect.left);
    assert(dst_rect.bottom - dst_rect.top == src_rect.bottom - src_rect.top);

    const auto* gl_src = to_native(src);
    auto* gl_dst = to_native(dst);
    assert(gl_src != nullptr);
    assert(gl_dst != nullptr);

    const auto* gl_view_src = to_native(gl_src->desc().color_attachment.view);
    auto* gl_view_dst = to_native(gl_dst->desc().color_attachment.view);
    assert(gl_view_src != nullptr);
    assert(gl_view_dst != nullptr);

    if (gl_view_src->texture() == 0u ||
        !has_version(4, 3)) {
        resolve_subresource(
            dst, src, 
            std::nullopt, /* format */
            src_rect, dst_rect
        );
    }
    else {
        const auto* ddesc = &gl_view_dst->resource()->desc();
        const auto* sdesc = &gl_view_src->resource()->desc();

        assert(gl_src->fbo() != gl_dst->fbo());

        if (gl_dst->fbo() == 0) {
            ddesc = &get_backbuffer()->desc();
        }
        else if (gl_src->fbo() == 0) {
            sdesc = &get_backbuffer()->desc();
        }

        const GLint src_level = 0;
        const GLint dst_level = 0;

        const GLint src_x = static_cast<GLint>(src_rect.left);
        const GLint src_y = static_cast<GLint>(sdesc->height - src_rect.bottom); // note bottom
        const GLint src_z = 0;

        const GLint dst_x = static_cast<GLint>(dst_rect.left);
        const GLint dst_y = static_cast<GLint>(ddesc->height - dst_rect.bottom); // note bottom
        const GLint dst_z = 0;

        const GLsizei w = static_cast<GLsizei>(src_rect.right - src_rect.left);
        const GLsizei h = static_cast<GLsizei>(src_rect.bottom - src_rect.top);
        const GLsizei d = 1;

        gl_call(glCopyImageSubData(
            gl_view_src->texture(), GL_TEXTURE_2D, src_level,
            src_x, src_y, src_z,
            gl_view_dst->texture(), GL_TEXTURE_2D, dst_level,
            dst_x, dst_y, dst_z,
            w, h, d)
        );
    }
}

void gl_context::resolve_subresource(framebuffer* dst, const framebuffer* src, std::optional<texture_format> format,
                                     const rect& src_rect, const rect& dst_rect)
{
    assert(dst_rect.right - dst_rect.left == src_rect.right - src_rect.left);
    assert(dst_rect.bottom - dst_rect.top == src_rect.bottom - src_rect.top);

    (void)format;

    const auto* gl_src = to_native(src);
    auto* gl_dst = to_native(dst);
    assert(gl_src != nullptr);
    assert(gl_dst != nullptr);

    assert(gl_src->fbo() != gl_dst->fbo());

    const auto* gl_view_src = to_native(gl_src->desc().color_attachment.view);
    auto* gl_view_dst = to_native(gl_dst->desc().color_attachment.view);
    assert(gl_view_src != nullptr);
    assert(gl_view_dst != nullptr);

    const auto* ddesc = &gl_view_dst->resource()->desc();
    const auto* sdesc = &gl_view_src->resource()->desc();

    GLboolean scissors_was_enabled;
    glGetBooleanv(GL_SCISSOR_TEST, &scissors_was_enabled);
    glDisable(GL_SCISSOR_TEST);

    gl_call(glBindFramebuffer(GL_READ_FRAMEBUFFER, gl_src->fbo()));
    gl_call(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl_dst->fbo()));

    if (gl_dst->fbo() == 0) {
        gl_call(glDrawBuffer(GL_BACK));
        ddesc = &get_backbuffer()->desc();
    }
    else {
        gl_call(glDrawBuffer(GL_COLOR_ATTACHMENT0));
    }
    if (gl_src->fbo() == 0) {
        gl_call(glReadBuffer(GL_BACK));
        sdesc = &get_backbuffer()->desc();
    }
    else {
        gl_call(glReadBuffer(GL_COLOR_ATTACHMENT0));
    }

    gl_call(glBlitFramebuffer(
        src_rect.left, sdesc->height - src_rect.top, src_rect.right, sdesc->height - src_rect.bottom,
        dst_rect.left, ddesc->height - dst_rect.top, dst_rect.right, ddesc->height - dst_rect.bottom,
        GL_COLOR_BUFFER_BIT, GL_NEAREST)
    );

    if (scissors_was_enabled)
        glEnable(GL_SCISSOR_TEST);
}

/// create

std::unique_ptr<blendstate> gl_context::create_blendstate(const blendstate_desc& desc)
{
    return std::make_unique<gl_blendstate>(this, desc);
}

std::unique_ptr<buffer> gl_context::create_buffer(const buffer_desc& desc, const void* initial_data)
{
    return std::make_unique<gl_buffer>(this, desc, initial_data);
}

std::unique_ptr<depthstencilstate> gl_context::create_depthstencilstate(const depthstencilstate_desc& desc)
{
    return std::make_unique<gl_depthstencilstate>(this, desc);
}

std::unique_ptr<rasterizerstate> gl_context::create_rasterizerstate(const rasterizerstate_desc& desc)
{
    return std::make_unique<gl_rasterizerstate>(this, desc);
}

std::unique_ptr<sampler> gl_context::create_sampler(const sampler_desc& desc)
{
    return std::make_unique<gl_sampler>(this, desc);
}

std::unique_ptr<compiled_shader> gl_context::compile_vertexshader(const char* source, std::size_t length)
{
    return std::make_unique<gl_compiled_shader>(this, source, length);
}

std::unique_ptr<vertexshader> gl_context::create_vertexshader(compiled_shader* shader_data)
{
    return std::make_unique<gl_vertexshader>(this, to_native(shader_data));
}

std::unique_ptr<vertexshader> gl_context::create_vertexshader(const void* data, std::size_t size_bytes)
{
    gl_compiled_shader cs(
        this,
        reinterpret_cast<const char*>(data),
        size_bytes
    );
    return create_vertexshader(&cs);
}

std::unique_ptr<compiled_shader> gl_context::compile_pixelshader(const char* source, std::size_t length)
{
    return std::make_unique<gl_compiled_shader>(this, source, length);
}

std::unique_ptr<pixelshader> gl_context::create_pixelshader(compiled_shader* shader_data)
{
    return std::make_unique<gl_pixelshader>(this, to_native(shader_data));
}

std::unique_ptr<pixelshader> gl_context::create_pixelshader(const void* data, std::size_t size_bytes)
{
    gl_compiled_shader cs(
        this,
        reinterpret_cast<const char*>(data),
        size_bytes
    );
    return create_pixelshader(&cs);
}

std::unique_ptr<shaderprogram> gl_context::create_shaderprogram(vertexshader* vs, pixelshader* ps)
{
    return std::make_unique<gl_shaderprogram>(this,
        reinterpret_cast<gl_vertexshader*>(vs),
        reinterpret_cast<gl_pixelshader*>(ps)
    );
}

std::unique_ptr<inputlayout> gl_context::create_inputlayout(const vertex_attribute_desc* desc, std::uint32_t count,
    const void* vs_data, std::size_t vs_data_size)
{
    return std::make_unique<gl_inputlayout>(this, desc, count,
        reinterpret_cast<const std::uint8_t*>(vs_data), vs_data_size
    );
}

std::unique_ptr<texture2d> gl_context::create_texture2d(const texture_desc& desc, const void* initial_data)
{
    return std::make_unique<gl_texture2d>(this, desc, initial_data);
}

void gl_context::acquire_backbuffer()
{
    backbuffer_ = gl_texture2d::from_backbuffer(this);
    if (backbuffer_->has_error()) {
        set_error(
            std::to_underlying(gl_context_error::backbuffer),
            backbuffer_->get_detail()
        );
    }

    render_width_ = backbuffer_->desc().width;
    render_height_ = backbuffer_->desc().height;
}

std::unique_ptr<textureview> gl_context::create_textureview(texture2d* tex, const textureview_desc& desc)
{
    return std::make_unique<gl_textureview>(this, to_native(tex), desc);
}

std::unique_ptr<framebuffer> gl_context::create_framebuffer(const framebuffer_desc& desc)
{
    return std::make_unique<gl_framebuffer>(this, desc);
}

void gl_context::set_blendstate(const blendstate* bs, const float(&factor)[4], std::uint32_t sample_mask)
{
    if (bs != nullptr) {
        to_native(bs)->bind(factor, sample_mask);
    }
    else {
        glDisable(GL_BLEND);
    }
}

void gl_context::set_depthstencilstate(const depthstencilstate* ds, std::uint32_t stencil_ref)
{
    if (ds != nullptr) {
        to_native(ds)->bind(stencil_ref);
    }
    else {
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);
    }
}

void gl_context::set_inputlayout(const inputlayout* il)
{
    if (il != nullptr) {
        gl_call(glBindVertexArray(
            to_native(il)->vao()
        ));
    }
    else {
        gl_call(glBindVertexArray(0u));
    }
}

void gl_context::set_rasterizerstate(const rasterizerstate* rs)
{
    if (rs != nullptr) {
        to_native(rs)->bind();
    }
    else {
        //
    }
}

void gl_context::set_shaderprogram(const shaderprogram* s)
{
    const auto program = s == nullptr ? 0u : to_native(s)->program();
    gl_call(glUseProgram(program));
}

/// bind

void gl_context::set_vertex_buffer(const buffer* vb, std::uint32_t slot)
{
    const auto buf = vb == nullptr ? 0u : to_native(vb)->buffer();

    if (has_version(4, 3)) {
        gl_call(glBindVertexBuffer(
            static_cast<GLuint>(slot),
            buf,
            0u,
            vb == nullptr ? 0 : static_cast<GLsizei>(vb->desc().vb_stride)
        ));
    }
    else {
        gl_call(glBindBuffer(GL_ARRAY_BUFFER, buf));
    }
}

void gl_context::set_index_buffer(const buffer* ib)
{
    GLuint buf = 0u;
    if (ib != nullptr) {
        auto* gib = to_native(ib);
        buf = gib->buffer();
        assert(gib->target() == GL_ELEMENT_ARRAY_BUFFER);
    }

    gl_call(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf));

    if (ib == nullptr) {
        current_index_type_ = static_cast<GLenum>(-1);
        return;
    }

    current_index_type_ = (ib->desc().ib_type == index_buffer_type::u16) ? 
        GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}

void gl_context::set_uniform_buffer(const buffer* ub, shader_bind_type stage, std::uint32_t slot)
{
    GLuint buf = 0u;
    if (ub != nullptr) {
        auto* gub = to_native(ub);
        buf = gub->buffer();
        assert(gub->target() == GL_UNIFORM_BUFFER);
    }

    (void)stage;
    gl_call(glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(slot), buf));
}

void gl_context::set_texture(const textureview* srv, shader_bind_type stage, std::uint32_t slot)
{
    (void)stage;

    GLuint tex = 0u;
    GLenum target = GL_TEXTURE_2D;
    if (srv != nullptr) {
        auto* gsrv = to_native(srv);
        tex = gsrv->texture();
        target = gsrv->view_target();
    }

    gl_call(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot)));
    gl_call(glBindTexture(target, tex));
}

void gl_context::set_texture_native(void* handle, shader_bind_type stage, std::uint32_t slot)
{
    (void)stage;

    GLuint tex = static_cast<GLuint>(reinterpret_cast<std::uintptr_t>(handle));
    GLenum target = GL_TEXTURE_2D;

    gl_call(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot)));
    gl_call(glBindTexture(target, tex));
}

void gl_context::set_sampler(const sampler* s, shader_bind_type stage, std::uint32_t slot)
{
    (void)stage;

    GLuint samp = 0u;
    if (s != nullptr) {
        samp = to_native(s)->sampler();
    }

    gl_call(glBindSampler(static_cast<GLuint>(slot), samp));
}

void gl_context::set_framebuffer(const framebuffer* fb)
{
    const auto fbo = fb == nullptr ? 0u : to_native(fb)->fbo();

    gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    const texture_desc* td;
    if (fbo == 0) {
        td = &get_backbuffer()->desc();
    }
    else {
        auto* grtv = to_native(fb->desc().color_attachment.view);
        auto* res = grtv->resource();
        td = &res->desc();
    }
    current_render_height_ = td->height;

    glViewport(
        0,
        static_cast<GLint>(current_render_height_) - static_cast<GLint>(render_height_),
        static_cast<GLint>(render_width_),
        static_cast<GLint>(render_height_)
    );
}

void gl_context::clear_framebuffer(const framebuffer* fb)
{
    assert(fb != nullptr);
    auto fbo = to_native(fb)->fbo();

    const texture_desc* d;

    if (fbo == 0) {
        d = &get_backbuffer()->desc();
    }
    else {
        d = &to_native(fb->desc().color_attachment.view)->resource()->desc();

    }

    glViewport(
        0, 0,
        static_cast<GLsizei>(d->width),
        static_cast<GLsizei>(d->height)
    );
    glScissor(
        0, 0,
        static_cast<GLsizei>(d->width),
        static_cast<GLsizei>(d->height)
    );

    GLint prev_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

    gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    constexpr GLfloat ccolor[4] = { 0.f, 0.f, 0.f, 0.f };
    glClearBufferfv(GL_COLOR, 0, &ccolor[0]);

    glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
}

void gl_context::apply_viewport_if_dirty()
{
    if (!raster_.viewport_set)
        return;

    const auto& v = raster_.viewport;
    const GLint x = (GLint)v.top_left_x;

    GLint y;
    if (common_origin_) {
        y = (GLint)(current_render_height_ - (v.top_left_y + v.height));
    }
    else {
        y = (GLint)v.top_left_y;
    }

    glViewport(x, y, (GLsizei)v.width, (GLsizei)v.height);
    glDepthRangef((GLclampf)v.min_depth, (GLclampf)v.max_depth);

    raster_.viewport_set = false;
}

void gl_context::apply_scissor_if_dirty()
{
    if (!raster_.scissor_set)
        return;

    const auto& r = raster_.scissor;
    const GLint x = (GLint)r.left;
    const GLsizei w = (GLsizei)(r.right - r.left);
    const GLsizei h = (GLsizei)(r.bottom - r.top);

    GLint y;
    if (common_origin_) {
        y = (GLint)(current_render_height_ - r.bottom);
    }
    else {
        y = (GLint)r.top;
    }

    glScissor(x, y, w, h);
    raster_.scissor_set = false;
}

void gl_context::draw(std::uint32_t count, std::uint32_t vertex_start)
{
    assert(current_topology_ != static_cast<GLenum>(-1));

    gl_call(glDrawArrays(
        current_topology_,
        static_cast<GLint>(vertex_start),
        static_cast<GLsizei>(count)
    ));
}

void gl_context::draw_indexed(std::uint32_t count, std::uint32_t index_start, std::uint32_t vertex_start)
{
    assert(current_index_type_ != static_cast<GLenum>(-1));
    assert(current_topology_   != static_cast<GLenum>(-1));

    const std::uint32_t index_stride = (current_index_type_ == GL_UNSIGNED_SHORT) ?
                                        2u : 4u;
    const uintptr_t start_bytes = uintptr_t(index_start) * index_stride;
    void* idx_ptr = reinterpret_cast<void*>(start_bytes);

    if (vertex_start != 0) {
        gl_call(glDrawElementsBaseVertex(
            current_topology_,
            count,
            current_index_type_,
            idx_ptr,
            static_cast<GLint>(vertex_start)
        ));
    }
    else {
        gl_call(glDrawElements(
            current_topology_,
            count,
            current_index_type_,
            idx_ptr
        ));
    }
}

void gl_context::set_scissor_rect(const rect& r)
{
    assert(r.left <= r.right);
    assert(r.top <= r.bottom);

    raster_.scissor = r;
    raster_.scissor_set = true;
}

void gl_context::set_primitive_topology(primitive_topology t)
{
    switch (t) {
    case primitive_topology::triangle_list: 
        current_topology_ = GL_TRIANGLES;
        break;
    case primitive_topology::line_list:
        current_topology_ = GL_LINES;
        break;
    case primitive_topology::point_list:
        current_topology_ = GL_POINTS;
        break;
    }
}

void gl_context::set_viewport(const viewport& v)
{
    raster_.viewport = v;
    raster_.viewport_set = true;
}

void gl_context::backup_render_state()
{
    assert(!backup_data_->captured);

    glGetIntegerv(GL_CURRENT_PROGRAM, &backup_data_->last_program);

    // active texture & texture binding
    glGetIntegerv(GL_ACTIVE_TEXTURE, &backup_data_->last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &backup_data_->last_texture);
#ifdef GL_SAMPLER_BINDING
    glGetIntegerv(GL_SAMPLER_BINDING, &backup_data_->last_sampler);
#else
    backup_data_->last_sampler = 0;
#endif

    // buffers / VAO
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &backup_data_->last_array_buffer);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &backup_data_->last_vertex_array);

    // viewport / scissor / polygon mode
    glGetIntegerv(GL_VIEWPORT, backup_data_->last_viewport);
    glGetIntegerv(GL_SCISSOR_BOX, backup_data_->last_scissor_box);
    glGetIntegerv(GL_POLYGON_MODE, backup_data_->last_polygon_mode);
    glGetFloatv(GL_DEPTH_RANGE, backup_data_->last_depth_range);

    // enables
    backup_data_->last_enable_blend = glIsEnabled(GL_BLEND);
    backup_data_->last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    backup_data_->last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    backup_data_->last_enable_stencil_test = glIsEnabled(GL_STENCIL_TEST);
    backup_data_->last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    backup_data_->last_enable_color_op = glIsEnabled(GL_COLOR_LOGIC_OP);
    backup_data_->last_depth_clamp = glIsEnabled(GL_DEPTH_CLAMP);
    backup_data_->last_multisample = glIsEnabled(GL_MULTISAMPLE);
    backup_data_->last_line_smooth = glIsEnabled(GL_LINE_SMOOTH);

    // rasterizer
    glGetIntegerv(GL_CULL_FACE_MODE, &backup_data_->last_cull_face_mode);
    glGetIntegerv(GL_FRONT_FACE, &backup_data_->last_front_face);

    backup_data_->last_poly_offset_fill = glIsEnabled(GL_POLYGON_OFFSET_FILL);
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &backup_data_->last_poly_offset_factor);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &backup_data_->last_poly_offset_units);

    glGetIntegerv(GL_DEPTH_FUNC, &backup_data_->last_depth_func);

    // stencil front
    glGetIntegerv(GL_STENCIL_FUNC, &backup_data_->last_stencil_func_front);
    glGetIntegerv(GL_STENCIL_REF, &backup_data_->last_stencil_ref_front);
    {
        GLint mask = 0;
        glGetIntegerv(GL_STENCIL_VALUE_MASK, &mask);
        backup_data_->last_stencil_valuemask_front = static_cast<GLuint>(mask);
    }
    glGetIntegerv(GL_STENCIL_FAIL, &backup_data_->last_stencil_sfail_front);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_FAIL, &backup_data_->last_stencil_dpfail_front);
    glGetIntegerv(GL_STENCIL_PASS_DEPTH_PASS, &backup_data_->last_stencil_dppass_front);
    {
        GLint mask = 0;
        glGetIntegerv(GL_STENCIL_WRITEMASK, &mask);
        backup_data_->last_stencil_writemask_front = static_cast<GLuint>(mask);
    }

    // stencil back
    glGetIntegerv(GL_STENCIL_BACK_FUNC, &backup_data_->last_stencil_func_back);
    glGetIntegerv(GL_STENCIL_BACK_REF, &backup_data_->last_stencil_ref_back);
    {
        GLint mask = 0;
        glGetIntegerv(GL_STENCIL_BACK_VALUE_MASK, &mask);
        backup_data_->last_stencil_valuemask_back = static_cast<GLuint>(mask);
    }
    glGetIntegerv(GL_STENCIL_BACK_FAIL, &backup_data_->last_stencil_sfail_back);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_FAIL, &backup_data_->last_stencil_dpfail_back);
    glGetIntegerv(GL_STENCIL_BACK_PASS_DEPTH_PASS, &backup_data_->last_stencil_dppass_back);
    {
        GLint mask = 0;
        glGetIntegerv(GL_STENCIL_BACK_WRITEMASK, &mask);
        backup_data_->last_stencil_writemask_back = static_cast<GLuint>(mask);
    }

    // blend state
    glGetIntegerv(GL_BLEND_SRC_RGB, &backup_data_->last_blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &backup_data_->last_blend_dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &backup_data_->last_blend_src_alpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &backup_data_->last_blend_dst_alpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &backup_data_->last_blend_eq_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &backup_data_->last_blend_eq_alpha);
    glGetFloatv(GL_BLEND_COLOR, backup_data_->last_blend_color);
#ifdef GL_SAMPLE_MASK_VALUE
    {
        GLint sampleMask = 0;
        glGetIntegeri_v(GL_SAMPLE_MASK_VALUE, 0, &sampleMask);
        backup_data_->last_sample_mask = static_cast<GLuint>(sampleMask);
    }
#endif

    // write masks
    glGetBooleanv(GL_COLOR_WRITEMASK, backup_data_->last_color_mask);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &backup_data_->last_depth_mask);

    // fbo
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &backup_data_->last_fbo);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &backup_data_->last_draw_fbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &backup_data_->last_read_fbo);

    // uniform buffer binding 0
    if (has_version(3, 1)) {
        GLint buf0 = 0;
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &buf0);
        backup_data_->last_uniform_buffer_0 = static_cast<GLuint>(buf0);
    }

    backup_data_->captured = true;
}

void gl_context::restore_render_state()
{
    assert(backup_data_->captured);
    backup_data_->captured = false;

    // viewport + scissor
    glViewport(
        backup_data_->last_viewport[0],
        backup_data_->last_viewport[1],
        backup_data_->last_viewport[2],
        backup_data_->last_viewport[3]
    );
    glScissor(
        backup_data_->last_scissor_box[0],
        backup_data_->last_scissor_box[1],
        backup_data_->last_scissor_box[2],
        backup_data_->last_scissor_box[3]
    );

    glDepthRangef(backup_data_->last_depth_range[0], backup_data_->last_depth_range[1]);

    // enables
    if (backup_data_->last_enable_blend)        glEnable(GL_BLEND);          else glDisable(GL_BLEND);
    if (backup_data_->last_enable_cull_face)    glEnable(GL_CULL_FACE);      else glDisable(GL_CULL_FACE);
    if (backup_data_->last_enable_depth_test)   glEnable(GL_DEPTH_TEST);     else glDisable(GL_DEPTH_TEST);
    if (backup_data_->last_enable_stencil_test) glEnable(GL_STENCIL_TEST);   else glDisable(GL_STENCIL_TEST);
    if (backup_data_->last_enable_scissor_test) glEnable(GL_SCISSOR_TEST);   else glDisable(GL_SCISSOR_TEST);
    if (backup_data_->last_enable_color_op)     glEnable(GL_COLOR_LOGIC_OP); else glDisable(GL_COLOR_LOGIC_OP);

    // blend state
    glBlendEquationSeparate(
        backup_data_->last_blend_eq_rgb,
        backup_data_->last_blend_eq_alpha
    );
    glBlendFuncSeparate(
        backup_data_->last_blend_src_rgb,
        backup_data_->last_blend_dst_rgb,
        backup_data_->last_blend_src_alpha,
        backup_data_->last_blend_dst_alpha
    );
    glBlendColor(
        backup_data_->last_blend_color[0],
        backup_data_->last_blend_color[1],
        backup_data_->last_blend_color[2],
        backup_data_->last_blend_color[3]
    );

#ifdef GL_SAMPLE_MASK_VALUE
    glSampleMaski(0, backup_data_->last_sample_mask);
#endif

    // polygon mode
    gl_call(glPolygonMode(GL_FRONT_AND_BACK, backup_data_->last_polygon_mode[0]));

    // program
    gl_call(glUseProgram(backup_data_->last_program));

    // fb0
    glBindFramebuffer(GL_FRAMEBUFFER,        backup_data_->last_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,   backup_data_->last_draw_fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,   backup_data_->last_read_fbo);

    // UBO binding 0
    if (has_version(3, 1)) {
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, backup_data_->last_uniform_buffer_0);
    }

    // VAO + VBO
    glBindVertexArray(backup_data_->last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, backup_data_->last_array_buffer);

    // texture / sampler / active tex
    // glActiveTexture(backup_data_->last_active_texture); // causes visual bugs
    glBindTexture(GL_TEXTURE_2D, backup_data_->last_texture);
#ifdef GL_SAMPLER_BINDING
    glBindSampler(0, backup_data_->last_sampler);
#endif

    // rasterizer / depth
    glCullFace(backup_data_->last_cull_face_mode);
    glFrontFace(backup_data_->last_front_face);

    if (backup_data_->last_poly_offset_fill) glEnable(GL_POLYGON_OFFSET_FILL);
    else                                     glDisable(GL_POLYGON_OFFSET_FILL);

    glPolygonOffset(
        backup_data_->last_poly_offset_factor,
        backup_data_->last_poly_offset_units
    );

    if (backup_data_->last_depth_clamp) glEnable(GL_DEPTH_CLAMP);
    else                                glDisable(GL_DEPTH_CLAMP);

    if (backup_data_->last_multisample) glEnable(GL_MULTISAMPLE);
    else                                glDisable(GL_MULTISAMPLE);

    if (backup_data_->last_line_smooth) glEnable(GL_LINE_SMOOTH);
    else                                glDisable(GL_LINE_SMOOTH);

    glDepthFunc(backup_data_->last_depth_func);

    glStencilFuncSeparate(
        GL_FRONT,
        backup_data_->last_stencil_func_front,
        backup_data_->last_stencil_ref_front,
        backup_data_->last_stencil_valuemask_front
    );
    glStencilOpSeparate(
        GL_FRONT,
        backup_data_->last_stencil_sfail_front,
        backup_data_->last_stencil_dpfail_front,
        backup_data_->last_stencil_dppass_front
    );
    glStencilMaskSeparate(GL_FRONT, backup_data_->last_stencil_writemask_front);

    glStencilFuncSeparate(
        GL_BACK,
        backup_data_->last_stencil_func_back,
        backup_data_->last_stencil_ref_back,
        backup_data_->last_stencil_valuemask_back
    );
    glStencilOpSeparate(
        GL_BACK,
        backup_data_->last_stencil_sfail_back,
        backup_data_->last_stencil_dpfail_back,
        backup_data_->last_stencil_dppass_back
    );
    glStencilMaskSeparate(GL_BACK, backup_data_->last_stencil_writemask_back);

    // write masks
    glColorMask(
        backup_data_->last_color_mask[0],
        backup_data_->last_color_mask[1],
        backup_data_->last_color_mask[2],
        backup_data_->last_color_mask[3]
    );
    gl_call(glDepthMask(backup_data_->last_depth_mask));
}

void gl_context::setup_render_state()
{
    glDisable(GL_COLOR_LOGIC_OP);
}

r2_end_