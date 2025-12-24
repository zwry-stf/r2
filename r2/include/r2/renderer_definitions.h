#pragma once
#include <backend/context.h>
#include <assert.h>
#include <cmath>
#include <vector>
#include <algorithm>


r2_begin_

// forward declarations
class font;

#ifndef v_always_inline
#if defined(_MSC_VER)
#define v_always_inline __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define v_always_inline inline __attribute__((always_inline))
#else
#define v_always_inline inline
#endif
#endif

typedef char32_t wchar;

class vec2 {
public:
    float x, y;

public:
    v_always_inline constexpr vec2() noexcept
        : x(0.f), y(0.f) { }
    v_always_inline constexpr vec2(float x, float y) noexcept
        : x(x), y(y) { }
    v_always_inline constexpr explicit vec2(float v) noexcept
        : x(v), y(v) { }

public:
    [[nodiscard]] v_always_inline vec2 operator+(const vec2& v) const noexcept {
        return vec2(x + v.x, y + v.y);
    }
    [[nodiscard]] v_always_inline vec2 operator-(const vec2& v) const noexcept {
        return vec2(x - v.x, y - v.y);
    }
    [[nodiscard]] v_always_inline vec2 operator*(const vec2& v) const noexcept {
        return vec2(x * v.x, y * v.y);
    }
    [[nodiscard]] v_always_inline vec2 operator/(const vec2& v) const noexcept {
        assert(v.x != 0.f && v.y != 0.f);
        return vec2(x / v.x, y / v.y);
    }
    v_always_inline vec2& operator+=(const vec2& v) noexcept {
        *this = *this + v;
        return *this;
    }
    v_always_inline vec2& operator-=(const vec2& v) noexcept {
        *this = *this - v;
        return *this;
    }
    v_always_inline vec2& operator*=(const vec2& v) noexcept {
        *this = *this * v;
        return *this;
    }
    v_always_inline vec2& operator/=(const vec2& v) noexcept {
        *this = *this / v;
        return *this;
    }
    [[nodiscard]] v_always_inline vec2 operator-() const noexcept {
        return vec2(-x, -y);
    }
    [[nodiscard]] v_always_inline bool operator==(const vec2& v) const noexcept {
        return x == v.x && y == v.y;
    }

public:
    [[nodiscard]] v_always_inline float length() const noexcept {
        return std::sqrt(x * x + y * y);
    }
    [[nodiscard]] v_always_inline vec2 perp() const noexcept {
        return vec2(-y, x);
    }
    [[nodiscard]] v_always_inline float dot(const vec2& b) const noexcept {
        return x * b.x + y * b.y;
    }
    [[nodiscard]] v_always_inline float cross(const vec2& b) const noexcept {
        return x * b.y - y * b.x;
    }
    [[nodiscard]] v_always_inline vec2 normalize() const noexcept {
        const float l = length();
        if (l < 1e-9f)
            return vec2();
        const float inv_len = 1.f / l;
        return *this * vec2(inv_len);
    }
    [[nodiscard]] v_always_inline vec2 normalize(float max_inv) const noexcept {
        const float l = length();
        if (l < 1e-9f)
            return vec2();
        float inv_len = 1.f / l;
        if (max_inv < inv_len)
            inv_len = max_inv;
        return *this * vec2(inv_len);
    }
};

typedef std::uint32_t color_u32;

class color {
private:
    inline static constexpr float kIntToFloat = 1.f / 255.f;

public:
    float r, g, b, a;

    inline static constexpr color_u32 alpha_mask = 0xFF000000u;
public:
    v_always_inline constexpr color() noexcept
        : r(0.f), g(0.f), b(0.f), a(0.f) { }
    v_always_inline constexpr color(float r, float g, float b, float a) noexcept
        : r(r), g(g), b(b), a(a) { }
    v_always_inline constexpr color(float r, float g, float b) noexcept
        : color(r, g, b, 1.f) { }
    template <std::integral T>
    v_always_inline constexpr color(T r, T g, T b, T a) noexcept
        : r(static_cast<float>(r) * kIntToFloat),
          g(static_cast<float>(g) * kIntToFloat), 
          b(static_cast<float>(b) * kIntToFloat),
          a(static_cast<float>(a) * kIntToFloat) { }
    template <std::integral T>
    v_always_inline constexpr color(T r, T g, T b) noexcept
        : color(r, g, b, T(255)) { }
    v_always_inline constexpr explicit color(color_u32 u) noexcept
        : color(
            (u >> (0u * 8u)) & 0xFFu,
            (u >> (1u * 8u)) & 0xFFu,
            (u >> (2u * 8u)) & 0xFFu,
            (u >> (3u * 8u)) & 0xFFu
            ) { }

public:
    v_always_inline static color red()    noexcept { return color(1.f, 0.f, 0.f, 1.f); }
    v_always_inline static color green()  noexcept { return color(0.f, 1.f, 0.f, 1.f); }
    v_always_inline static color blue()   noexcept { return color(0.f, 0.f, 1.f, 1.f); }
    v_always_inline static color yellow() noexcept { return color(1.f, 1.f, 0.f, 1.f); }
    v_always_inline static color cyan()   noexcept { return color(0.f, 1.f, 1.f, 1.f); }
    v_always_inline static color purple() noexcept { return color(1.f, 0.f, 1.f, 1.f); }
    v_always_inline static color white()  noexcept { return color(1.f, 1.f, 1.f, 1.f); }
    v_always_inline static color black()  noexcept { return color(0.f, 0.f, 0.f, 1.f); }

public:
    [[nodiscard]] v_always_inline color operator+(const color& c) const noexcept {
        return color(r + c.r, g + c.g, b + c.b, a + c.a);
    }
    [[nodiscard]] v_always_inline color operator-(const color& c) const noexcept {
        return color(r - c.r, g - c.g, b - c.b, a - c.a);
    }
    [[nodiscard]] v_always_inline color operator*(const color& c) const noexcept {
        return color(r * c.r, g * c.g, b * c.b, a * c.a);
    }
    [[nodiscard]] v_always_inline color operator/(const color& c) const noexcept {
        return color(r / c.r, g / c.g, b / c.b, a / c.a);
    }

public:
    [[nodiscard]] v_always_inline color alpha(float m) const noexcept {
        return color(r, g, b, a * m);
    }
    [[nodiscard]] v_always_inline color transparent() const noexcept {
        return color(r, g, b, 0.f);
    }
    [[nodiscard]] v_always_inline color opague() const noexcept {
        return color(r, g, b, 1.f);
    }
    [[nodiscard]] v_always_inline color interp(const color& v, float t) const noexcept {
        return color(
            (v.r - r) * t + r,
            (v.g - g) * t + g, 
            (v.b - b) * t + b,
            (v.a - a) * t + a);
    }
    [[nodiscard]] v_always_inline operator color_u32() const noexcept {
        return static_cast<color_u32>(r * 255.f) << (0u * 8u) |
               static_cast<color_u32>(g * 255.f) << (1u * 8u) |
               static_cast<color_u32>(b * 255.f) << (2u * 8u) |
               static_cast<color_u32>(a * 255.f) << (3u * 8u);
    }
};

class vec4 {
public:
    float x, y, z, w;

public:
    v_always_inline constexpr vec4() noexcept
        : x(0.f), y(0.f), z(0.f), w(0.f) { }
    v_always_inline constexpr vec4(float x, float y, float z, float w) noexcept
        : x(x), y(y), z(z), w(w) { }
    v_always_inline constexpr explicit vec4(float v) noexcept
        : x(v), y(v), z(v), w(v) { }

public:
    [[nodiscard]] v_always_inline vec4 operator+(const vec4& v) const noexcept {
        return vec4(x + v.x, y + v.y, z + v.z, w + v.w);
    }
    [[nodiscard]] v_always_inline vec4 operator-(const vec4& v) const noexcept {
        return vec4(x - v.x, y - v.y, z - v.z, w - v.w);
    }
    [[nodiscard]] v_always_inline vec4 operator*(const vec4& v) const noexcept {
        return vec4(x * v.x, y * v.y, z * v.z, w * v.w);
    }
    [[nodiscard]] v_always_inline vec4 operator/(const vec4& v) const noexcept {
        assert(v.x != 0.f && v.y != 0.f && v.z != 0.f && v.w != 0.f);
        return vec4(x / v.x, y / v.y, z / v.z, w / v.w);
    }
    v_always_inline vec4& operator+=(const vec4& v) noexcept {
        *this = *this + v;
        return *this;
    }
    v_always_inline vec4& operator-=(const vec4& v) noexcept {
        *this = *this - v;
        return *this;
    }
    v_always_inline vec4& operator*=(const vec4& v) noexcept {
        *this = *this * v;
        return *this;
    }
    v_always_inline vec4& operator/=(const vec4& v) noexcept {
        *this = *this / v;
        return *this;
    }
    [[nodiscard]] v_always_inline vec4 operator-() const noexcept {
        return vec4(-x, -y, -z, -w);
    }
    [[nodiscard]] v_always_inline bool operator==(const vec4& v) const noexcept {
        return x == v.x && y == v.y && z == v.z && w == v.w;
    }

public:
    [[nodiscard]] v_always_inline float length() const noexcept {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }
    [[nodiscard]] v_always_inline float dot() const noexcept {
        return x * x + y * y + z * z + w * w;
    }
    [[nodiscard]] v_always_inline vec4 normalize() const noexcept {
        const float l = length();
        if (l < 1e-9f)
            return vec4();
        const float inv_len = 1.f / l;
        return *this * vec4(inv_len);
    }
};

struct vertex {
    vec2 pos; // position
    vec2 uv;  // tex coord
    color_u32 col; // color
};

typedef std::uint32_t index;

typedef void* texture_handle;

struct draw_cmd {
    rect clip_rect{};
    texture_handle texture{};
    std::uint32_t index_start;
    std::uint32_t vertex_start;
};

struct cmd_header {
    rect clip_rect;
    texture_handle texture;
};

namespace math
{
    inline constexpr float g_pi        = 3.14159265358979323846f;
    inline constexpr float g_2_pi      = g_pi * 2.f;
    inline constexpr float g_pi_div_2  = g_pi / 2.f;
    inline constexpr float g_1_div_2pi = 1.f / g_2_pi;
}

struct shared_data {
    vec2 uv_white_px;
    vec4 shadow_uvs;
    std::vector<vec2> temp_buffer;
};

struct font_cfg {
    std::uint32_t size{ 18u };
    std::uint32_t oversample_h{ 1u };
    std::uint32_t oversample_v{ 1u };
    std::int32_t offset_x{ 0 };
    std::int32_t offset_y{ 0 };
    std::uint32_t glow_radius{ 0u };
    float glow_strength{ 2. };
};

struct renderer_flags {
    std::int8_t anti_aliased_lines : 1 { 1 };
    std::int8_t anti_aliased_lines_use_tex : 1 { 1 };
    std::int8_t anti_aliased_fill  : 1 { 1 };
};

enum class e_rounding_flags : std::uint8_t {
    rounding_none        = 0u,
    rounding_topleft     = (1u << 0u),
    rounding_topright    = (1u << 1u),
    rounding_bottomleft  = (1u << 2u),
    rounding_bottomright = (1u << 3u),
    rounding_top = rounding_topleft | rounding_topright,
    rounding_bottom = rounding_bottomleft | rounding_bottomright,
    rounding_left = rounding_topleft | rounding_bottomleft,
    rounding_right = rounding_topright | rounding_bottomright,
    rounding_all = rounding_topleft | rounding_topright | 
                   rounding_bottomleft | rounding_bottomright
};

v_always_inline e_rounding_flags operator|(const e_rounding_flags& a, const e_rounding_flags& b) {
    return static_cast<e_rounding_flags>(
        std::to_underlying(a) | std::to_underlying(b)
        );
}

v_always_inline bool operator&(const e_rounding_flags& a, const e_rounding_flags& b) {
    return (std::to_underlying(a) & std::to_underlying(b)) != 0u;
}

r2_end_