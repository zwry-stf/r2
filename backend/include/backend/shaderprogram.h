#pragma once
#include <backend/object.h>


r2_begin_

class shaderprogram : public object<void> {
protected:
    using object<void>::object;

public:
    virtual void bind() const = 0;
};

r2_end_