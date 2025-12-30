#pragma once
#include <d3d11.h>
#include <type_traits>


template <class _Class>
    requires (std::is_base_of_v<IUnknown, _Class>)
class d3d_pointer
{
private:
    _Class* value_;

public:
    constexpr d3d_pointer() noexcept
        : value_(nullptr) { }

    explicit d3d_pointer(_Class* v) noexcept
        : value_(v) { }

    d3d_pointer(const d3d_pointer&) = delete;
    d3d_pointer& operator=(const d3d_pointer&) = delete;

    d3d_pointer(d3d_pointer<_Class>&& v) noexcept
        : value_(v.value_) {
        v.value_ = nullptr;
    }
    d3d_pointer& operator=(d3d_pointer<_Class>&& v) noexcept {
        if (&v != this) {
            reset();
            value_ = v.value_;
            v.value_ = nullptr;
        }
        return *this;
    }

    ~d3d_pointer() noexcept {
        reset();
    }

public:
    _Class* release() noexcept {
        _Class* ret = value_;
        value_ = nullptr;
        return ret;
    }

    void reset() noexcept {
        if (value_ != nullptr) {
            value_->Release();
            value_ = nullptr;
        }
    }

    void reset(_Class* v) noexcept {
        if (v != value_) {
            reset();
            value_ = v;
        }
    }

    [[nodiscard]] _Class* get() const noexcept {
        return value_;
    }

    // will reset first
    [[nodiscard]] _Class** address_of() noexcept {
        reset();
        return &value_;
    }

    [[nodiscard]] _Class* const* address_of() const noexcept {
        return &value_;
    }

public:
    [[nodiscard]] _Class* operator->() const noexcept {
        return get();
    }

    [[nodiscard]] operator _Class* () const noexcept {
        return value_;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return value_ != nullptr;
    }
};