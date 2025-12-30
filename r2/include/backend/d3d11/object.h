#pragma once
#include <backend/def.h>


r2_begin_

class d3d11_context;

class d3d11_object {
private:
    d3d11_context* const context_;

public:
    constexpr explicit d3d11_object(d3d11_context* ctx) noexcept
        : context_(ctx) { }

    virtual ~d3d11_object() = default;

    d3d11_object(const d3d11_object&) = delete;
    d3d11_object& operator=(const d3d11_object&) = delete;
    d3d11_object(d3d11_object&&) = delete;
    d3d11_object& operator=(d3d11_object&&) = delete;

protected:
    [[nodiscard]] d3d11_context* context() const noexcept {
        return context_;
    }
};

r2_end_