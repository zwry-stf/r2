#pragma once
#include <backend/object.h>
#include <cstddef>


r2_begin_

enum class buffer_usage : std::uint8_t {
    vertex,
    index,
    uniform,
};

enum class index_buffer_type : std::uint8_t {
    u16,
    u32,
};

struct buffer_desc {
    std::uint32_t size_bytes = 0u;
    buffer_usage usage = buffer_usage::vertex;
    bool dynamic = false;

    union {
        uint32_t vb_stride{};
        index_buffer_type ib_type;
    };
};

class buffer : public object<buffer_desc> {
protected:
    using object<buffer_desc>::object;

public:
    virtual void update(const void* data, std::size_t size) = 0;

    [[nodiscard]] auto size() const noexcept {
        return desc().size_bytes;
    }
};

r2_end_