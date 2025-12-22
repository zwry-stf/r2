#pragma once


#if not defined(r2_begin_)
#error "don't include this file directly..."
#endif

#if defined(R2_BACKEND_D3D11)
struct IDXGISwapChain;
#endif // R2_BACKEND_D3D11

r2_begin_

struct context_init_data {
#if defined(R2_BACKEND_D3D11)
    IDXGISwapChain* sc;
#endif // R2_BACKEND_D3D11
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