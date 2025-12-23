#pragma once
#include <backend/context.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>


r2_begin_

enum class d3d11_context_error : std::int32_t {
    invalid_param,
    device,
};

class d3d11_context : public context {
private:
    d3d_pointer<ID3D11Device> device_;
    d3d_pointer<ID3D11DeviceContext> context_;
    d3d_pointer<IDXGISwapChain> sc_;

public:
    d3d11_context(IDXGISwapChain* sc);

public:
    /// get
    // immediate
    virtual rect get_scissor_rect() const noexcept override;
    virtual primitive_topology get_primitive_topology() const noexcept override;
    virtual viewport get_viewport() const noexcept override;

    /// create
    virtual std::unique_ptr<blendstate> create_blendstate(const blendstate_desc& desc) override;
    virtual std::unique_ptr<buffer> create_buffer(const buffer_desc& desc, const void* initial_data = nullptr) override;
    virtual std::unique_ptr<depthstencilstate> create_depthstencilstate(const depthstencilstate_desc& desc) override;
    virtual std::unique_ptr<rasterizerstate> create_rasterizerstate(const rasterizerstate_desc& desc) override;
    virtual std::unique_ptr<sampler> create_sampler(const sampler_desc& desc) override;
    virtual std::unique_ptr<compiled_shader> compile_vertex_shader(const char* source, std::size_t length) override;
    virtual std::unique_ptr<vertexshader> create_vertexshader(compiled_shader* shader_data) override;
    virtual std::unique_ptr<vertexshader> create_vertexshader(const void* data, std::size_t size_bytes) override;
    virtual std::unique_ptr<compiled_shader> compile_pixel_shader(const char* source, std::size_t length) override;
    virtual std::unique_ptr<pixelshader> create_pixelshader(compiled_shader* shader_data) override;
    virtual std::unique_ptr<pixelshader> create_pixelshader(const void* data, std::size_t size_bytes) override;
    virtual std::unique_ptr<shaderprogram> create_shaderprogram(vertexshader* vs, pixelshader* ps) override;
    virtual std::unique_ptr<inputlayout> create_inputlayout(const vertex_attribute_desc* desc, std::uint32_t count,
                                                            const void* vs_data, std::size_t vs_data_size) override;
    virtual std::unique_ptr<texture2d> create_texture2d(const texture_desc& desc, const void* initial_data = nullptr) override;
    virtual std::optional<std::unique_ptr<texture2d>> acquire_backbuffer() override;
    virtual std::unique_ptr<textureview> create_textureview(texture2d* tex, const textureview_desc& desc) override;
    virtual std::unique_ptr<framebuffer> create_framebuffer(const framebuffer_desc& desc) override;

    /// bind
    virtual void set_vertex_buffer(buffer* vb, std::uint32_t slot = 0u) override;
    virtual void set_index_buffer(buffer* ib) override;
    virtual void set_uniform_buffer(buffer* ub, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) override;
    virtual void set_texture(textureview* srv, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) override;
    virtual void set_texture_native(void* handle, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) override;
    virtual void set_sampler(sampler* s, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) override;
    virtual void set_framebuffer(framebuffer* fb) override;
    virtual void clear_framebuffer(framebuffer* fb) override;

    // immediate
    virtual void draw(std::uint32_t count, std::uint32_t vertex_start = 0u) override;
    virtual void draw_indexed(std::uint32_t count, std::uint32_t index_start = 0u, std::uint32_t vertex_start = 0u) override;
    virtual void set_scissor_rect(const rect& rect) override;
    virtual void set_primitive_topology(primitive_topology t) override;
    virtual void set_viewport(const viewport& v) override;

    //
    virtual void update_display_size(std::uint32_t width, std::uint32_t height) override;
    virtual void backup_render_state() override;
    virtual void restore_render_state() override;
    virtual void setup_render_state() override;

public:
    [[nodiscard]] auto* get_device() const noexcept {
        return device_.get();
    }

    [[nodiscard]] auto* get_context() const noexcept {
        return context_.get();
    }

    [[nodiscard]] auto* get_swapchain() const noexcept {
        return sc_.get();
    }
};

r2_end_

#include "context.inline.inl"