#include <backend/opengl/compiled_shader.h>


r2_begin_

gl_compiled_shader::gl_compiled_shader(gl_context* ctx, const char* source, std::size_t length)
	: r2::compiled_shader(),
	  gl_object(ctx),
	  source_(source),
	  source_length_(length)
{
}

gl_compiled_shader::~gl_compiled_shader() = default;

const void* gl_compiled_shader::data() const noexcept
{
	return reinterpret_cast<const void*>(source_);
}

std::size_t gl_compiled_shader::size() const noexcept
{
	return source_length_;
}

r2_end_