#pragma once


#if not defined(r2_begin_)
#error "don't include this file directly..."
#endif

#if defined(R2_BACKEND_D3D11)
struct IDXGISwapChain;
#endif // R2_BACKEND_D3D11

#if defined(R2_PLATFORM_WINDOWS)
struct HWND__;
#endif // R2_PLATFORM_WINDOWS;

r2_begin_

struct backend_init_data {
#if defined(R2_BACKEND_D3D11)
    IDXGISwapChain* sc;
    explicit backend_init_data(IDXGISwapChain* sc)
        : sc(sc) { }
#elif defined(R2_BACKEND_OPENGL) // R2_BACKEND_D3D11
    backend_init_data() = default;
#endif // R2_BACKEND_OPENGL
};

struct platform_init_data {
#if defined(R2_PLATFORM_WINDOWS)
    HWND__* hwnd;
    explicit platform_init_data(HWND__* hwnd)
        : hwnd(hwnd) { }
#endif // R2_PLATFORM_WINDOWS
};


enum class shader_bind_type : std::uint8_t {
    ps,
    vs,
    cs
};

enum class primitive_topology : std::uint8_t {
    unknown,
    triangle_list,
    line_list,
    point_list,
    triangle_strip,
};


struct rect {
    std::int32_t left{};
    std::int32_t top{};
    std::int32_t right{};
    std::int32_t bottom{};

public:
    [[nodiscard]] bool operator==(const rect& o) const noexcept {
        return left == o.left &&
               top == o.top &&
               right == o.right &&
               bottom == o.bottom;
    }
};

struct viewport {
    float top_left_x = 0.f;
    float top_left_y = 0.f;
    float width;
    float height;
    float min_depth = 0.f;
    float max_depth = 1.f;
};

r2_end_