#pragma once
#include <backend/object.h>
#include <memory>

// 
#include <backend/blendstate.h>
#include <backend/buffer.h>
#include <backend/depthstencilstate.h>
#include <backend/inputlayout.h>
#include <backend/pixelshader.h>
#include <backend/rasterizerstate.h>
#include <backend/sampler.h>
#include <backend/shaderprogram.h>
#include <backend/texture2d.h>
#include <backend/textureview.h>
#include <backend/vertexshader.h>
#include <backend/compiled_shader.h>
#include <backend/framebuffer.h>

#include "context.inline.inl"


r2_begin_

class context : public object<void> {
protected:
    context() = default;

public:
    static std::unique_ptr<context> make_context(const context_init_data& data, bool common_origin = true);

public:
    /// get
	// immediate
    virtual rect get_scissor_rect() const noexcept = 0;
    virtual primitive_topology get_primitive_topology() const noexcept = 0;
    virtual viewport get_viewport() const noexcept = 0;

    /// create
	virtual std::unique_ptr<blendstate> create_blendstate(const blendstate_desc& desc) = 0;
	virtual std::unique_ptr<buffer> create_buffer(const buffer_desc& desc, const void* initial_data = nullptr) = 0;
	virtual std::unique_ptr<depthstencilstate> create_depthstencilstate(const depthstencilstate_desc& desc) = 0;
	virtual std::unique_ptr<rasterizerstate> create_rasterizerstate(const rasterizerstate_desc& desc) = 0;
	virtual std::unique_ptr<sampler> create_sampler(const sampler_desc& desc) = 0;
	virtual std::unique_ptr<compiled_shader> compile_vertex_shader(const char* source, std::size_t length) = 0;
	virtual std::unique_ptr<vertexshader> create_vertexshader(compiled_shader* shader_data) = 0;
	virtual std::unique_ptr<vertexshader> create_vertexshader(const void* data, std::size_t size_bytes) = 0;
	virtual std::unique_ptr<compiled_shader> compile_pixel_shader(const char* source, std::size_t length) = 0;
	virtual std::unique_ptr<pixelshader> create_pixelshader(compiled_shader* shader_data) = 0;
	virtual std::unique_ptr<pixelshader> create_pixelshader(const void* data, std::size_t size_bytes) = 0;
	virtual std::unique_ptr<shaderprogram> create_shaderprogram(vertexshader* vs, pixelshader* ps) = 0;
	virtual std::unique_ptr<inputlayout> create_inputlayout(const vertex_attribute_desc* desc, std::uint32_t count,
		                                                    const void* vs_data, std::size_t vs_data_size) = 0;
	virtual std::unique_ptr<texture2d> create_texture2d(const texture_desc& desc, const void* initial_data = nullptr) = 0;
	virtual std::unique_ptr<textureview> create_textureview(texture2d* tex, const textureview_desc& desc) = 0;
	virtual std::unique_ptr<framebuffer> create_framebuffer(const framebuffer_desc& desc) = 0;

    /// bind
	virtual void set_vertex_buffer(buffer* vb, std::uint32_t slot = 0u) = 0;
	virtual void set_index_buffer(buffer* ib) = 0;
	virtual void set_uniform_buffer(buffer* ub, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) = 0;
	virtual void set_texture(textureview* srv, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) = 0;
	virtual void set_texture_native(void* handle, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) = 0;
	virtual void set_sampler(sampler* s, shader_bind_type stage = shader_bind_type::ps, std::uint32_t slot = 0u) = 0;
	virtual void set_framebuffer(framebuffer* fb) = 0;
	virtual void clear_framebuffer(framebuffer* fb) = 0;

    // immediate
    virtual void draw(std::uint32_t count, std::uint32_t vertex_start = 0u) = 0;
    virtual void draw_indexed(std::uint32_t count, std::uint32_t index_start = 0u, std::uint32_t vertex_start = 0u) = 0;
    virtual void set_scissor_rect(const rect& rect) = 0;
    virtual void set_primitive_topology(primitive_topology t) = 0;
    virtual void set_viewport(const viewport& v) = 0;

	//
	virtual void update_display_size(std::uint32_t width, std::uint32_t height) = 0;
	virtual void backup_render_state() = 0;
	virtual void restore_render_state() = 0;
	virtual void setup_render_state() = 0;
};

r2_end_