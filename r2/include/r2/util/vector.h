#pragma once
#include <backend/def.h>

#include <type_traits>
#include <concepts>
#include <cassert>
#include <cstring>
#include <utility>


#ifndef v_always_inline
#if defined(_MSC_VER)
#define v_always_inline __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define v_always_inline inline __attribute__((always_inline))
#else
#define v_always_inline inline
#endif
#endif

r2_begin_

template <typename T, typename S = std::uint32_t>
class vector {
public:
    using value_type = T;
    using size_type = S;

private:
    inline static constexpr bool k_is_trivially_copyable = std::is_trivially_copyable_v<T>;
    inline static constexpr bool k_is_trivially_constructible = std::is_trivially_constructible_v<T>;
    inline static constexpr bool k_is_trivially_destructible = std::is_trivially_destructible_v<T>;

private:
    value_type* data_;
    size_type size_{};
    size_type capacity_{};

public:
    constexpr vector() noexcept = default;
    ~vector() noexcept(noexcept(clear())) {
        clear();
        if (capacity_ != 0) {
            ::operator delete(
                data_,
                capacity_ * sizeof(T),
                std::align_val_t{ alignof(T) }
            );
        }
    }
    vector(const vector<T, S>& o)
        : vector() {
        if (!o.empty()) {
            reallocate(o.size());
            static_assert(std::is_copy_constructible_v<T>);
            for (size_type i = 0; i < o.size(); i++) {
                new (&data_[i]) T(o[i]);
            }
        }
        size_ = o.size();
    }
    vector& operator=(const vector<T, S>& o) {
        if (this != &o) {
            if (o.empty()) {
                clear();
                return *this;
            }

            if (capacity_ < o.size()) {
                vector tmp(o);
                *this = std::move(tmp);
                return *this;
            }

            clear();
            static_assert(std::is_copy_constructible_v<T>);
            for (size_type i = 0; i < o.size(); ++i) {
                new (&data_[i]) T(o[i]);
            }
            size_ = o.size();
        }
        return *this;
    }
    vector(vector<T, S>&& o) noexcept
        : data_(o.data_),
          size_(o.size_),
          capacity_(o.capacity_) {
        o.size_ = 0;
        o.capacity_ = 0;
    }
    vector& operator=(vector<T, S>&& o) noexcept {
        if (this != &o) {
            if (capacity_ != 0) {
                clear();
                ::operator delete(
                    data_,
                    capacity_ * sizeof(T),
                    std::align_val_t{ alignof(T) }
                );
            }
            size_ = o.size_;
            capacity_ = o.capacity_;
            data_ = o.data_;

            o.size_ = 0;
            o.capacity_ = 0;
        }

        return *this;
    }

public:
    v_always_inline void clear() noexcept(std::is_nothrow_destructible_v<T>) {
        if (!k_is_trivially_destructible) {
            for (size_type i = 0; i < size_; i++) {
                data_[i].~T();
            }
        }
        size_ = 0;
    }
    // bytes will *NOT* be 0 initialized for trivial types
    void resize(size_type new_size) {
        if (new_size <= size_) {
            if constexpr (!k_is_trivially_destructible) {
                for (size_type i = new_size; i < size_; i++) {
                    data_[i].~T();
                }
            }
        }
        else {
            if (new_size > capacity_) {
                reallocate(new_size);
            }
            static_assert(std::default_initializable<T>);
            if constexpr (!k_is_trivially_constructible) {
                for (size_type i = size_; i < new_size; i++) {
                    new (&data_[i]) T();
                }
            }
        }

        size_ = new_size;
    }
    void reserve(size_type new_size) {
        if (new_size <= capacity_) {
            return;
        }

        reallocate(new_size);
    }
    v_always_inline void push_back(const T& value) {
        if (size_ >= capacity_) {
            reallocate(calculate_growth(capacity_));
        }
        static_assert(std::is_copy_constructible_v<T>);
        new (&data_[size_]) T(value);
        size_++;
    }
    v_always_inline void push_back(T&& value) {
        if (size_ >= capacity_) {
            reallocate(calculate_growth(capacity_));
        }
        static_assert(std::is_move_constructible_v<T>);
        new (&data_[size_]) T(std::move(value));
        size_++;
    }
    v_always_inline void pop_back() {
        assert(!empty());
        if constexpr (!k_is_trivially_destructible) {
            back().~T();
        }
        size_--;
    }
    template <typename... Args>
        requires (std::is_constructible_v<T, Args...>)
    v_always_inline T& emplace_back(Args&&... args) {
        if (size_ >= capacity_) {
            reallocate(calculate_growth(capacity_));
        }
        new (&data_[size_]) T(std::forward<Args>(args)...);
        return data_[size_++];
    }
    [[nodiscard]] T* append(size_type count) {
        const auto offset = size_;
        resize(size_ + count);
        return data_ + offset;
    }

public:
    [[nodiscard]] v_always_inline bool empty() const noexcept {
        return size_ == 0;
    }
    [[nodiscard]] v_always_inline size_type size() const noexcept {
        return size_;
    }
    [[nodiscard]] v_always_inline const T* data() const noexcept {
        return data_;
    }
    [[nodiscard]] v_always_inline T* data() noexcept {
        return data_;
    }
    [[nodiscard]] v_always_inline const T* begin() const noexcept {
        return data_;
    }
    [[nodiscard]] v_always_inline T* begin() noexcept {
        return data_;
    }
    [[nodiscard]] v_always_inline const T* end() const noexcept {
        return data_ + size_;
    }
    [[nodiscard]] v_always_inline T* end() noexcept {
        return data_ + size_;
    }

public:
    template <std::integral I>
    [[nodiscard]] v_always_inline const T& at(I index) const noexcept {
        if constexpr (std::signed_integral<I>) {
            assert(index >= I{ 0 });
        }
        assert(static_cast<std::uint64_t>(index) < static_cast<std::uint64_t>(size_));
        return data_[index];
    }
    template <std::integral I>
    [[nodiscard]] v_always_inline T& at(I index) noexcept {
        if constexpr (std::signed_integral<I>) {
            assert(index >= I{ 0 });
        }
        assert(static_cast<std::uint64_t>(index) < static_cast<std::uint64_t>(size_));
        return data_[index];
    }
    [[nodiscard]] v_always_inline const T& back() const noexcept {
        assert(!empty());
        return data_[size_ - 1];
    }
    [[nodiscard]] v_always_inline T& back() noexcept {
        assert(!empty());
        return data_[size_ - 1];
    }
    [[nodiscard]] v_always_inline const T& front() const noexcept {
        assert(!empty());
        return data_[0];
    }
    [[nodiscard]] v_always_inline T& front() noexcept {
        assert(!empty());
        return data_[0];
    }

    template <std::integral I>
    [[nodiscard]] v_always_inline const T& operator[](I index) const noexcept {
        return at(index);
    }
    template <std::integral I>
    [[nodiscard]] v_always_inline T& operator[](I index) noexcept {
        return at(index);
    }

private:
    [[nodiscard]] v_always_inline static size_type calculate_growth(size_type current) noexcept {
        return current < 8 ? 8 : (current * 3 / 2);
    }
    void reallocate(size_type new_size) {
        assert(new_size > capacity_);
        assert(capacity_ != 0 || size_ == 0);

        T* new_data = reinterpret_cast<T*>(
            ::operator new(new_size * sizeof(T), std::align_val_t{ alignof(T) })
        );

        size_type constructed = 0;

        if constexpr (k_is_trivially_copyable) {
            std::memcpy(new_data, data_, size_ * sizeof(T));
            constructed = size_;
        }
        else {
            static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>);

            try {
                for (; constructed < size_; ++constructed) {
                    if constexpr (std::is_move_constructible_v<T>) {
                        new (&new_data[constructed]) T(std::move(data_[constructed]));
                    }
                    else {
                        new (&new_data[constructed]) T(data_[constructed]);
                    }
                }
            }
            catch (...) {
                for (size_type j = 0; j < constructed; ++j) {
                    new_data[j].~T();
                }
                ::operator delete(
                    new_data, 
                    new_size * sizeof(T), 
                    std::align_val_t{ alignof(T) }
                );
                throw;
            }
        }

        T* const old_data = data_;
        const size_type old_cap = capacity_;

        if constexpr (!k_is_trivially_destructible) {
            for (size_type i = 0; i < size_; ++i) {
                old_data[i].~T();
            }
        }

        if (old_cap != 0) {
            ::operator delete(
                old_data, 
                old_cap * sizeof(T), 
                std::align_val_t{ alignof(T) }
            );
        }

        data_ = new_data;
        capacity_ = new_size;
    }
};

r2_end_