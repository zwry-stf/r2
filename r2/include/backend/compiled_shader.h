#pragma once
#include <backend/object.h>
#include <cstddef>


r2_begin_

class compiled_shader : public object<void> {
protected:
    using object<void>::object;

public:
    virtual [[nodiscard]] const void* data() const noexcept = 0;
    virtual [[nodiscard]] std::size_t size() const noexcept = 0;
};

r2_end_