#pragma once
#include <backend/compiled_shader.h>
#include <backend/opengl/object.h>


r2_begin_

class gl_compiled_shader : public compiled_shader,
							  protected gl_object {
private:
	const char* const source_;
	const std::size_t source_length_;

public:
	gl_compiled_shader(gl_context* ctx, const char* source, std::size_t length);
	~gl_compiled_shader();

public:
	virtual [[nodiscard]] const void* data() const noexcept override;
	virtual [[nodiscard]] std::size_t size() const noexcept override;

	[[nodiscard]] const auto* source() const noexcept {
		return source_;
	}
	[[nodiscard]] auto source_length() const noexcept {
		return source_length_;
	}
};

r2_end_