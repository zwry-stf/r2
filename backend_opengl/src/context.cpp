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

gl_context::gl_context(bool common_origin)
    : common_origin_(common_origin)
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
}

/// get

rect gl_context::get_scissor_rect() const noexcept
{
    GLint box[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_SCISSOR_BOX, box);

    const GLint x = box[0];
    const GLint y = box[1];
    const GLint w = box[2];
    const GLint h = box[3];

    rect r{};
    r.left = static_cast<std::int32_t>(x);
    r.right = static_cast<std::int32_t>(x + w);

    if (common_origin_) {
        const std::int32_t bottom = current_render_height_ - static_cast<std::int32_t>(y);
        r.bottom = bottom;
        r.top = bottom - static_cast<std::int32_t>(h);
    }
    else {
        r.top = static_cast<std::int32_t>(y);
        r.bottom = static_cast<std::int32_t>(y + h);
    }

    return r;
}

primitive_topology gl_context::get_primitive_topology() const noexcept
{
    switch (current_topology_) {
    case GL_TRIANGLES: return primitive_topology::triangle_list;
    case GL_LINES:     return primitive_topology::line_list;
    case GL_POINTS:    return primitive_topology::point_list;
    default:           return primitive_topology::triangle_list;
    }
}

viewport gl_context::get_viewport() const noexcept
{
    GLint vp[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, vp);

    GLfloat depth_range[2] = { 0.0f, 1.0f };
    glGetFloatv(GL_DEPTH_RANGE, depth_range);

    viewport v{};
    v.top_left_x = static_cast<float>(vp[0]);
    v.width      = static_cast<float>(vp[2]);
    v.height     = static_cast<float>(vp[3]);

    v.top_left_y = common_origin_ ? static_cast<float>(
        static_cast<GLint>(current_render_height_) - (vp[1] + vp[3])) :
        static_cast<float>(vp[1]);

    v.min_depth = static_cast<float>(depth_range[0]);
    v.max_depth = static_cast<float>(depth_range[1]);

    return v;
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

std::unique_ptr<compiled_shader> gl_context::compile_vertex_shader(const char* source, std::size_t length)
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

std::unique_ptr<compiled_shader> gl_context::compile_pixel_shader(const char* source, std::size_t length)
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

std::optional<std::unique_ptr<texture2d>> gl_context::acquire_backbuffer()
{
    return std::make_unique<gl_texture2d>(this, nullptr);
}

std::unique_ptr<textureview> gl_context::create_textureview(texture2d* tex, const textureview_desc& desc)
{
    return std::make_unique<gl_textureview>(this, to_native(tex), desc);
}

std::unique_ptr<framebuffer> gl_context::create_framebuffer(const framebuffer_desc& desc)
{
    return std::make_unique<gl_framebuffer>(this, desc);
}

/// bind

void gl_context::set_vertex_buffer(buffer* vb, std::uint32_t slot)
{
    GLuint buf = 0u;
    if (vb != nullptr) {
        auto* gvb = to_native(vb);
        buf = gvb->buffer();
    }

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

void gl_context::set_index_buffer(buffer* ib)
{
    GLuint buf = 0u;
    if (ib != nullptr) {
        auto* gib = to_native(ib);
        buf = gib->buffer();
        assert(gib->target() == GL_ELEMENT_ARRAY_BUFFER);
    }

    gl_call(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf));

    current_index_type_ = (ib->desc().ib_type == index_buffer_type::u16) ? 
        GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
}

void gl_context::set_uniform_buffer(buffer* ub, shader_bind_type stage, std::uint32_t slot)
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

void gl_context::set_texture(textureview* srv, shader_bind_type stage, std::uint32_t slot)
{
    (void)stage;

    GLuint tex = 0u;
    GLenum target = GL_TEXTURE_2D;
    if (srv != nullptr) {
        auto* gsrv = to_native(srv);
        tex = gsrv->texture();
        target = gsrv->target();
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

void gl_context::set_sampler(sampler* s, shader_bind_type stage, std::uint32_t slot)
{
    (void)stage;

    GLuint samp = 0u;
    if (s != nullptr) {
        samp = to_native(s)->sampler();
    }

    gl_call(glBindSampler(static_cast<GLuint>(slot), samp));
}

void gl_context::set_framebuffer(framebuffer* fb)
{
    GLuint fbo = 0;
    if (fb != nullptr) {
        fbo = to_native(fb->desc().color_attachment.view)->fbo();
    }

    gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    if (fbo == 0) { // backbuffer
        current_render_height_ = static_cast<std::int32_t>(render_height_);
    }

    glViewport(0,
        static_cast<GLint>(current_render_height_) - static_cast<GLint>(render_height_),
        static_cast<GLint>(render_width_),
        static_cast<GLint>(render_height_)
    );

    if (fbo == 0) { // backbuffer
        return;
    }

    auto* grtv = to_native(fb->desc().color_attachment.view);
    auto* res = grtv->resource();
    const auto& td = res->desc();
    current_render_height_ = static_cast<std::int32_t>(td.height);
}

void gl_context::clear_framebuffer(framebuffer* fb)
{
    auto fbo = to_native(fb->desc().color_attachment.view)->fbo();

    if (fbo == 0) {
        const GLsizei width = static_cast<GLsizei>(render_width_);
        const GLsizei height = static_cast<GLsizei>(render_height_);
        glViewport(0, 0, width, height);
        glScissor(0, 0, width, height);
    }
    else {
        auto& desc = to_native(fb->desc().color_attachment.view)->resource()->desc();

        glViewport(0, 0,
            static_cast<GLsizei>(desc.width),
            static_cast<GLsizei>(desc.height)
        );
        glScissor(0, 0,
            static_cast<GLsizei>(desc.width),
            static_cast<GLsizei>(desc.height)
        );
    }

    GLint prev_fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);

    gl_call(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

    constexpr GLfloat ccolor[4] = { 0.f, 0.f, 0.f, 0.f };
    glClearBufferfv(GL_COLOR, 0, &ccolor[0]);

    glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
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

    const GLint x = static_cast<GLint>(r.left);
    const GLsizei w = static_cast<GLsizei>(r.right - r.left);
    const GLsizei h = static_cast<GLsizei>(r.bottom - r.top);

    const GLint y = common_origin_ ?
        static_cast<GLint>(current_render_height_ - r.bottom) :
        static_cast<GLint>(r.top);

    glScissor(x, y, w, h);
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
    const GLint x = static_cast<GLint>(v.top_left_x);
    const GLint y = common_origin_ ? 
        static_cast<GLint>(current_render_height_ - (v.top_left_y + v.height)) :
        static_cast<GLint>(v.top_left_y);

    glViewport(x, y,
        static_cast<GLsizei>(v.width),
        static_cast<GLsizei>(v.height)
    );

    glDepthRangef(
        static_cast<GLclampf>(v.min_depth),
        static_cast<GLclampf>(v.max_depth)
    );
}

void gl_context::update_display_size(std::uint32_t width, std::uint32_t height)
{
    render_width_ = width;
    render_height_ = height;
}

void gl_context::backup_render_state()
{
}

void gl_context::restore_render_state()
{
}

void gl_context::setup_render_state()
{
    glDisable(GL_COLOR_LOGIC_OP);
}

r2_end_