#pragma once
#include <backend/object.h>


r2_begin_

enum class comparison_func : std::uint8_t {
    never,
    less,
    equal,
    less_equal,
    greater,
    not_equal,
    greater_equal,
    always,
};

enum class stencil_op : std::uint8_t {
    keep,
    zero,
    replace,
    incr_sat,
    decr_sat,
    invert,
    incr,
    decr,
};

struct depthstencil_op_desc {
    stencil_op      fail_op       = stencil_op::keep;
    stencil_op      depth_fail_op = stencil_op::keep;
    stencil_op      pass_op       = stencil_op::keep;
    comparison_func func          = comparison_func::always;
};

struct depthstencilstate_desc {
    bool             depth_enable = true;
    bool             depth_write  = true;
    comparison_func  depth_func   = comparison_func::less_equal;

    bool         stencil_enable     = false;
    std::uint8_t stencil_read_mask  = 0xFF;
    std::uint8_t stencil_write_mask = 0xFF;

    depthstencil_op_desc front_face;
    depthstencil_op_desc back_face;
};

class depthstencilstate : public object<depthstencilstate_desc> {
protected:
    using object<depthstencilstate_desc>::object;

public:
    virtual void bind(std::uint32_t stencil_ref = 0u) = 0;
};

r2_end_