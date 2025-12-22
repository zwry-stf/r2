#pragma once
#include <backend/compiled_shader.h>
#include <backend/d3d11/object.h>
#include <backend/d3d11/d3d_pointer.h>
#include <d3dcompiler.h>


r2_begin_

class d3d11_compiled_shader : public compiled_shader,
							  protected d3d11_object {
private:
	d3d_pointer<ID3DBlob> shader_blob_;

public:
	d3d11_compiled_shader(d3d11_context* ctx, const char* source, std::size_t length, const char* version);
	~d3d11_compiled_shader();

public:
	virtual [[nodiscard]] const void* data() const noexcept override;
	virtual [[nodiscard]] std::size_t size() const noexcept override;

	[[nodiscard]] auto* shader_blob() const noexcept {
		return shader_blob_.get();
	}
};

r2_end_