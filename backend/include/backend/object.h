#pragma once
#include <backend/def.h>
#include <cstdint>


r2_begin_

class object_base {
private:
    std::int32_t error_;
    std::int32_t detail_;

public:
    constexpr object_base() noexcept
        : error_(-1),
          detail_(0) { }

    virtual ~object_base() = default;

    object_base(const object_base&) = delete;
    object_base& operator=(const object_base&) = delete;
    object_base(object_base&&) = delete;
    object_base& operator=(object_base&&) = delete;

protected:
    void set_error(std::int32_t error, std::int32_t detail = 0) noexcept {
        error_ = error;
        detail_ = detail;
    }

    void set_error(std::int32_t error, long detail) noexcept {
        set_error(error, static_cast<std::int32_t>(detail));
    }
    
public:
    [[nodiscard]] bool has_error() const noexcept {
        return error_ != -1;
    }

    // 
    [[nodiscard]] std::int32_t get_error() const noexcept {
        return error_;
    }

    // gives platform specific details to error messages, can be 0
    [[nodiscard]] std::int32_t get_detail() const noexcept {
        return detail_;
    }
};

template <typename T>
class object : public object_base {
protected:
    const T desc_;

public:
    constexpr explicit object(const T& desc) noexcept
        : object_base(),
          desc_(desc) { }

public:
    [[nodiscard]] const auto& desc() const noexcept {
        return desc_;
    }
};

template <>
class object<void> : public object_base {
public:
    constexpr object() noexcept
        : object_base() { }
};

r2_end_