#pragma once
#include <backend/context.h>
#include <gl/glew.h>
#if defined(R2_PLATFORM_WINDOWS)
#include <Windows.h>
#endif


r2_begin_

enum class gl_context_error : std::int32_t {
    invalid_param,
    glew_init,
    backbuffer
};

class gl_context : public context {
private:
    const bool common_origin_;

    GLint minor_;
    GLint major_;

    GLenum current_topology_   = static_cast<GLenum>(-1);
    GLenum current_index_type_ = static_cast<GLenum>(-1);
    std::int32_t current_render_height_{ 0 };
    std::uint32_t render_width_{ 0u };
    std::uint32_t render_height_{ 0u };
#if defined(R2_PLATFORM_WINDOWS)
    HWND hwnd_;
#endif // R2_PLATFORM_WINDOWS

    std::unique_ptr<struct backup_render_data> backup_data_;

public:
#if defined(R2_PLATFORM_WINDOWS)
    gl_context(const context_init_data& init_data, bool common_origin);
#endif // R2_PLATFORM_WINDOWS

public:
    virtual void acquire_backbuffer() override;

    /// get
    // immediate
    virtual rect get_scissor_rect() const noexcept override;
    virtual primitive_topology get_primitive_topology() const noexcept override;
    virtual viewport get_viewport() const noexcept override;
    virtual void copy_subresource(framebuffer* dst, const framebuffer* src,
                                  const rect& src_rect, const rect& dst_rect) override;
    virtual void resolve_subresource(framebuffer* dst, const framebuffer* src, std::optional<texture_format> format,
                                     const rect& src_rect, const rect& dst_rect) override;

    /// create
    virtual std::unique_ptr<blendstate> create_blendstate(const blendstate_desc& desc) override;
    virtual std::unique_ptr<buffer> create_buffer(const buffer_desc& desc, const void* initial_data = nullptr) override;
    virtual std::unique_ptr<depthstencilstate> create_depthstencilstate(const depthstencilstate_desc& desc) override;
    virtual std::unique_ptr<rasterizerstate> create_rasterizerstate(const rasterizerstate_desc& desc) override;
    virtual std::unique_ptr<sampler> create_sampler(const sampler_desc& desc) override;
    virtual std::unique_ptr<compiled_shader> compile_vertexshader(const char* source, std::size_t length) override;
    virtual std::unique_ptr<vertexshader> create_vertexshader(compiled_shader* shader_data) override;
    virtual std::unique_ptr<vertexshader> create_vertexshader(const void* data, std::size_t size_bytes) override;
    virtual std::unique_ptr<compiled_shader> compile_pixelshader(const char* source, std::size_t length) override;
    virtual std::unique_ptr<pixelshader> create_pixelshader(compiled_shader* shader_data) override;
    virtual std::unique_ptr<pixelshader> create_pixelshader(const void* data, std::size_t size_bytes) override;
    virtual std::unique_ptr<shaderprogram> create_shaderprogram(vertexshader* vs, pixelshader* ps) override;
    virtual std::unique_ptr<inputlayout> create_inputlayout(const vertex_attribute_desc* desc, std::uint32_t count,
                                                            const void* vs_data, std::size_t vs_data_size) override;
    virtual std::unique_ptr<texture2d> create_texture2d(const texture_desc& desc, const void* initial_data = nullptr) override;
    virtual std::unique_ptr<textureview> create_textureview(texture2d* tex, const textureview_desc& desc) override;
    virtual std::unique_ptr<framebuffer> create_framebuffer(const framebuffer_desc& desc) override;

    /// bind
    virtual void set_blendstate(const blendstate* bs, const float(&factor)[4] = { 0.f, 0.f, 0.f, 0.f }, 
                                std::uint32_t sample_mask = 0xffffffffu) override;
    virtual void set_depthstencilstate(const depthstencilstate* ds, std::uint32_t stencil_ref = 0u) override;
    virtual void set_inputlayout(const inputlayout* il) override;
    virtual void set_rasterizerstate(const rasterizerstate* rs) override;
    virtual void set_shaderprogram(const shaderprogram* s) override;
    virtual void set_vertex_buffer(const buffer* vb, std::uint32_t slot = 0u) override;
    virtual void set_index_buffer(const buffer* ib) override;
    virtual void set_uniform_buffer(const buffer* ub, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) override;
    virtual void set_texture(const textureview* srv, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) override;
    virtual void set_texture_native(void* handle, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) override;
    virtual void set_sampler(const sampler* s, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) override;
    virtual void set_framebuffer(const framebuffer* fb) override;
    virtual void clear_framebuffer(const framebuffer* fb) override;

    // immediate
    virtual void draw(std::uint32_t count, std::uint32_t vertex_start = 0u) override;
    virtual void draw_indexed(std::uint32_t count, std::uint32_t index_start = 0u, std::uint32_t vertex_start = 0u) override;
    virtual void set_scissor_rect(const rect& rect) override;
    virtual void set_primitive_topology(primitive_topology t) override;
    virtual void set_viewport(const viewport& v) override;

    //
    virtual void backup_render_state() override;
    virtual void restore_render_state() override;
    virtual void setup_render_state() override;

public:
    [[nodiscard]] bool has_version(GLint major, GLint minor) const noexcept {
        return major_ > major || (major_ == major && minor_ >= minor);
    }
#if defined(R2_PLATFORM_WINDOWS)
    [[nodiscard]] HWND get_hwnd() const noexcept {
        return hwnd_;
    }
#endif // R2_PLATFORM_WINDOWS
};

r2_end_

#include "context.inline.inl"