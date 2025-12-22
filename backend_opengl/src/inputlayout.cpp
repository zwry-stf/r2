#include <backend/opengl/inputlayout.h>
#include <backend/opengl/context.h>
#include <assert.h>
#include <utility>


r2_begin_

gl_attr_info gl_inputlayout::to_gl_attr_info(vertex_attribute_format fmt) noexcept
{
    switch (fmt) {
    // 32-bit signed
    case vertex_attribute_format::i32:             return { 1, GL_INT,           GL_FALSE, true };
    case vertex_attribute_format::i32i32:          return { 2, GL_INT,           GL_FALSE, true };
    case vertex_attribute_format::i32i32i32:       return { 3, GL_INT,           GL_FALSE, true };
    case vertex_attribute_format::i32i32i32i32:    return { 4, GL_INT,           GL_FALSE, true };

    // 32-bit unsigned
    case vertex_attribute_format::u32:             return { 1, GL_UNSIGNED_INT,  GL_FALSE, true };
    case vertex_attribute_format::u32u32:          return { 2, GL_UNSIGNED_INT,  GL_FALSE, true };
    case vertex_attribute_format::u32u32u32:       return { 3, GL_UNSIGNED_INT,  GL_FALSE, true };
    case vertex_attribute_format::u32u32u32u32:    return { 4, GL_UNSIGNED_INT,  GL_FALSE, true };

    // 32-bit float
    case vertex_attribute_format::f32:             return { 1, GL_FLOAT,         GL_FALSE, false };
    case vertex_attribute_format::f32f32:          return { 2, GL_FLOAT,         GL_FALSE, false };
    case vertex_attribute_format::f32f32f32:       return { 3, GL_FLOAT,         GL_FALSE, false };
    case vertex_attribute_format::f32f32f32f32:    return { 4, GL_FLOAT,         GL_FALSE, false };

    // 16-bit signed
    case vertex_attribute_format::i16:             return { 1, GL_SHORT,         GL_FALSE, true };
    case vertex_attribute_format::i16i16:          return { 2, GL_SHORT,         GL_FALSE, true };
    case vertex_attribute_format::i16i16i16i16:    return { 4, GL_SHORT,         GL_FALSE, true };

    // 16-bit unsigned
    case vertex_attribute_format::u16:             return { 1, GL_UNSIGNED_SHORT,GL_FALSE, true };
    case vertex_attribute_format::u16u16:          return { 2, GL_UNSIGNED_SHORT,GL_FALSE, true };
    case vertex_attribute_format::u16u16u16u16:    return { 4, GL_UNSIGNED_SHORT,GL_FALSE, true };

    // 16-bit float
    case vertex_attribute_format::f16:             return { 1, GL_HALF_FLOAT,    GL_FALSE, false };
    case vertex_attribute_format::f16f16:          return { 2, GL_HALF_FLOAT,    GL_FALSE, false };
    case vertex_attribute_format::f16f16f16f16:    return { 4, GL_HALF_FLOAT,    GL_FALSE, false };

    // 8-bit signed
    case vertex_attribute_format::i8:              return { 1, GL_BYTE,          GL_FALSE, true };
    case vertex_attribute_format::i8i8:            return { 2, GL_BYTE,          GL_FALSE, true };
    case vertex_attribute_format::i8i8i8i8:        return { 4, GL_BYTE,          GL_FALSE, true };

    // 8-bit unsigned
    case vertex_attribute_format::u8:              return { 1, GL_UNSIGNED_BYTE, GL_FALSE, true };
    case vertex_attribute_format::u8u8:            return { 2, GL_UNSIGNED_BYTE, GL_FALSE, true };
    case vertex_attribute_format::u8u8u8u8:        return { 4, GL_UNSIGNED_BYTE, GL_FALSE, true };

    // normalized 8-bit
    case vertex_attribute_format::r8_unorm:        return { 1, GL_UNSIGNED_BYTE, GL_TRUE,  false };
    case vertex_attribute_format::r8r8_unorm:      return { 2, GL_UNSIGNED_BYTE, GL_TRUE,  false };
    case vertex_attribute_format::r8r8r8r8_unorm:  return { 4, GL_UNSIGNED_BYTE, GL_TRUE,  false };
    default:
        assert(false);
        return {};
    }
}

gl_inputlayout::gl_inputlayout(gl_context* ctx, const vertex_attribute_desc* desc, std::uint32_t count,
                               const std::uint8_t* vs_data, std::size_t vs_data_size)
    : r2::inputlayout(),
      gl_object(ctx)
{
    assert(desc != nullptr && count > 0u);

    (void)vs_data;
    (void)vs_data_size;

    clear_gl_errors();

    glGenVertexArrays(1, &vao_);
    if (drain_gl_errors() != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_inputlayout_error::layout_generation)
        );
        return;
    }

    glBindVertexArray(vao_);

    const GLuint kVertexBinding = 0;
    const GLuint kInstanceBinding = 1;

    if (ctx->has_version(4, 3)) {
        for (std::uint32_t i = 0u; i < count; ++i) {
            const auto& a = desc[i];
            const GLuint attrib_index = i;
            const gl_attr_info info = to_gl_attr_info(a.format);
            const GLuint binding = a.per_instance ? kInstanceBinding : kVertexBinding;

            glEnableVertexAttribArray(attrib_index);

            if (info.integer && !info.normalized) {
                glVertexAttribIFormat(attrib_index, info.size,
                    info.type, a.aligned_byte_offset);
            }
            else {
                glVertexAttribFormat(attrib_index, info.size,
                    info.type, info.normalized, a.aligned_byte_offset);
            }

            glVertexAttribBinding(attrib_index, binding);

            if (a.per_instance) {
                glVertexBindingDivisor(binding, a.instance_data_step_rate);
            }
        }
    }
    else {
        auto type_size = [](GLenum t) -> GLuint
            {
                switch (t)
                {
                case GL_BYTE:
                case GL_UNSIGNED_BYTE:
                    return 1u;
                case GL_SHORT:
                case GL_UNSIGNED_SHORT:
                    return 2u;
                case GL_INT:
                case GL_UNSIGNED_INT:
                case GL_FLOAT:
                    return 4u;
                case GL_HALF_FLOAT:
                    return 2u;
                default:
                    return 4u;
                }
            };

        GLuint stride = 0u;
        for (std::uint32_t i = 0u; i < count; ++i) {
            const auto& a = desc[i];
            const gl_attr_info info = to_gl_attr_info(a.format);
            GLuint bytes = info.size * type_size(info.type);
            stride = (stride > (a.aligned_byte_offset + bytes)) ? stride : (a.aligned_byte_offset + bytes);
        }

        for (std::uint32_t i = 0u; i < count; ++i) {
            const auto& a = desc[i];
            const GLuint attrib_index = i;
            const gl_attr_info info = to_gl_attr_info(a.format);
            const GLvoid* pointer = reinterpret_cast<const GLvoid*>(static_cast<uintptr_t>(a.aligned_byte_offset));

            glEnableVertexAttribArray(attrib_index);

            if (info.integer && !info.normalized) {
                glVertexAttribIPointer(attrib_index, info.size, info.type, stride, pointer);
            }
            else {
                glVertexAttribPointer(attrib_index, info.size, info.type, info.normalized, stride, pointer);
            }

            if (a.per_instance) {
                glVertexAttribDivisor(attrib_index, a.instance_data_step_rate);
            }
        }
    }

    GLenum gl_err = drain_gl_errors();

    glBindVertexArray(0);

    if (gl_err != GL_NO_ERROR) {
        set_error(
            std::to_underlying(gl_inputlayout_error::layout_creation),
            static_cast<std::int32_t>(gl_err)
        );
    }
}

gl_inputlayout::~gl_inputlayout()
{
    if (vao_ != 0u) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0u;
    }
}

void gl_inputlayout::bind() const
{
    gl_call(glBindVertexArray(vao_));
}

r2_end_