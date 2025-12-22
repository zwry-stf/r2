#pragma once
#include <backend/object.h>


r2_begin_

enum class vertex_attribute_format : std::uint8_t {
    unknown = 0,
    i32,
    i32i32,
    i32i32i32,
    i32i32i32i32,
    u32,
    u32u32,
    u32u32u32,
    u32u32u32u32,
    f32,
    f32f32,
    f32f32f32,
    f32f32f32f32,
    i16,
    i16i16,
    i16i16i16i16,
    u16,
    u16u16,
    u16u16u16u16,
    f16,
    f16f16,
    f16f16f16f16,
    i8,
    i8i8,
    i8i8i8i8,
    u8,
    u8u8,
    u8u8u8u8,
    r8_unorm,
    r8r8_unorm,
    r8r8r8r8_unorm,
};

struct vertex_attribute_desc {
    const char*             semantic_name;
    vertex_attribute_format format;
    std::uint32_t           aligned_byte_offset;
    bool                    per_instance = false;
    std::uint32_t           instance_data_step_rate = 0u;
};

class inputlayout : public object<void> {
protected:
    using object<void>::object;

public:
    virtual void bind() const = 0;
};

r2_end_