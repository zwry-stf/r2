#pragma once
#include "drawlist2d.h"

#include <algorithm>


r2_begin_

inline void drawlist2d::prim_rect(const vec2& min, const vec2& max, color_u32 col)
{
    auto* idx = indices_.append(6u);
    idx[0] = vertex_ptr_ + 0u;
    idx[1] = vertex_ptr_ + 1u;
    idx[2] = vertex_ptr_ + 2u;
    idx[3] = vertex_ptr_ + 0u;
    idx[4] = vertex_ptr_ + 2u;
    idx[5] = vertex_ptr_ + 3u;

    const vec2& uv = shared_data_->uv_white_px;
    auto* vtx = vertices_.append(4u);
    vtx[0] = vertex(vec2{ min.x, min.y }, uv, col);
    vtx[1] = vertex(vec2{ min.x, max.y }, uv, col);
    vtx[2] = vertex(vec2{ max.x, max.y }, uv, col);
    vtx[3] = vertex(vec2{ max.x, min.y }, uv, col);

    vertex_ptr_ += 4u;
}

inline void drawlist2d::add_rect(const vec2& min, const vec2& max, color_u32 col, float line_width, float rounding,
                                 e_rounding_flags flags, float corner_step)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    const bool odd = (static_cast<int>(std::round(line_width)) & 1) != 0;
    const vec2 offset = vec2(odd ? 0.5f : 0.f);

    path_rect(
        min + offset,
        max - offset,
        rounding,
        flags,
        corner_step
    );
    path_stroke(col, line_width, true);
}

inline void drawlist2d::add_rect_inner(const vec2& min, const vec2& max, color_u32 col, float line_width, float rounding,
                                       e_rounding_flags flags, float corner_step)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    const vec2 offset = vec2(line_width * 0.5f);

    path_rect(
        min + offset,
        max - offset,
        rounding,
        flags,
        corner_step
    );
    path_stroke(col, line_width, true);
}

inline void drawlist2d::add_rect_inner_fast(const vec2& min, const vec2& max, color_u32 col, float line_width)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    // top
    const auto& uv = shared_data_->uv_white_px;

    {
        auto* idx = indices_.append(6u);
        idx[0] = vertex_ptr_ + 0u;
        idx[1] = vertex_ptr_ + 1u;
        idx[2] = vertex_ptr_ + 2u;
        idx[3] = vertex_ptr_ + 0u;
        idx[4] = vertex_ptr_ + 2u;
        idx[5] = vertex_ptr_ + 3u;

        auto* vtx = vertices_.append(4u);
        vtx[0] = vertex(min, uv, col);
        vtx[1] = vertex(min + vec2(line_width), uv, col);
        vtx[2] = vertex(vec2(max.x - line_width, min.y + line_width), uv, col);
        vtx[3] = vertex(vec2(max.x, min.y), uv, col);
    }

    vertex_ptr_ += 4u;

    {
        // bottom
        auto* idx = indices_.append(6u);
        idx[0] = vertex_ptr_ + 0u;
        idx[1] = vertex_ptr_ + 1u;
        idx[2] = vertex_ptr_ + 2u;
        idx[3] = vertex_ptr_ + 0u;
        idx[4] = vertex_ptr_ + 2u;
        idx[5] = vertex_ptr_ + 3u;

        auto* vtx = vertices_.append(4u);
        vtx[0] = vertex(vec2(min.x, max.y), uv, col);
        vtx[1] = vertex(max, uv, col);
        vtx[2] = vertex(vec2(max.x - line_width, max.y - line_width), uv, col);
        vtx[3] = vertex(vec2(min.x + line_width, max.y - line_width), uv, col);

        vertex_ptr_ += 4u;
    }

    {
        // left
        auto* idx = indices_.append(6u);
        idx[0] = vertex_ptr_ + 0u;
        idx[1] = vertex_ptr_ + 1u;
        idx[2] = vertex_ptr_ + 2u;
        idx[3] = vertex_ptr_ + 0u;
        idx[4] = vertex_ptr_ + 2u;
        idx[5] = vertex_ptr_ + 3u;

        auto* vtx = vertices_.append(4u);
        vtx[0] = vertex(min, uv, col);
        vtx[1] = vertex(vec2(min.x, max.y), uv, col);
        vtx[2] = vertex(vec2(min.x + line_width, max.y - line_width), uv, col);
        vtx[3] = vertex(vec2(min.x + line_width, min.y + line_width), uv, col);

        vertex_ptr_ += 4u;
    }

    {
        // right
        auto* idx = indices_.append(6u);
        idx[0] = vertex_ptr_ + 0u;
        idx[1] = vertex_ptr_ + 1u;
        idx[2] = vertex_ptr_ + 2u;
        idx[3] = vertex_ptr_ + 0u;
        idx[4] = vertex_ptr_ + 2u;
        idx[5] = vertex_ptr_ + 3u;

        auto* vtx = vertices_.append(4u);
        vtx[0] = vertex(vec2(max.x, min.y), uv, col);
        vtx[1] = vertex(vec2(max.x - line_width, min.y + line_width), uv, col);
        vtx[2] = vertex(vec2(max.x - line_width, max.y - line_width), uv, col);
        vtx[3] = vertex(max, uv, col);

        vertex_ptr_ += 4u;
    }
}

inline void drawlist2d::add_rect_filled_multicolor(const vec2& min, const vec2& max,
                                                   color_u32 col_tl, color_u32 col_tr, color_u32 col_br, color_u32 col_bl)
{
    auto* idx = indices_.append(6u);
    idx[0] = vertex_ptr_ + 0u;
    idx[1] = vertex_ptr_ + 1u;
    idx[2] = vertex_ptr_ + 2u;
    idx[3] = vertex_ptr_ + 0u;
    idx[4] = vertex_ptr_ + 2u;
    idx[5] = vertex_ptr_ + 3u;

    const auto& uv = shared_data_->uv_white_px;

    auto* vtx = vertices_.append(4u);
    vtx[0] = vertex(min, uv, col_tl);
    vtx[1] = vertex(vec2{ min.x, max.y }, uv, col_bl);
    vtx[2] = vertex(max, uv, col_br);
    vtx[3] = vertex(vec2{ max.x, min.y }, uv, col_tr);

    vertex_ptr_ += 4u;
}

inline void drawlist2d::add_rect_filled(const vec2& min, const vec2& max, color_u32 col,
                                        float rounding, e_rounding_flags flags, float corner_step)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    if (rounding < 0.5f ||
        flags == e_rounding_flags::rounding_none) {
        prim_rect(min, max, col);
    }
    else {
        path_rect(min, max, rounding, flags, corner_step);
        path_fill_convex(col);
    }
}

inline void drawlist2d::add_rect_filled_faded(const vec2& min, const vec2& max, color_u32 col, color_u32 faded_col, float fade_start, float fade_end)
{
    const float delta = fade_end - fade_start;
    const bool draw_if_faded = (faded_col & color::alpha_mask) != 0u;
    const bool draw_no_fade = (col & color::alpha_mask) != 0u;
    assert(fade_start <= fade_end);

    if (!draw_if_faded &&
        !draw_no_fade) [[unlikely]]
        return;

    if (fade_end <= min.x) {
        if (draw_if_faded) {
            prim_rect(min, max, faded_col);
        }
    }
    else if (fade_start >= max.x) {
        if (draw_no_fade) {
            prim_rect(min, max, col);
        }
    }
    else {
        if (delta < 1e-3f) [[unlikely]] {
            if (draw_no_fade) {
                prim_rect(min, vec2(fade_start, max.y), col);
            }
            if (draw_if_faded) {
                prim_rect(vec2(fade_start, min.y), max, faded_col);
            }
            return;
        }

        const vec2& uv = shared_data_->uv_white_px;
        if (fade_start > min.x) {
            if (draw_no_fade) {
                prim_rect(min, vec2(fade_start, max.y), col);
            }

            if (fade_end < max.x) {
                auto* idx = indices_.append(6u);
                idx[0] = vertex_ptr_ + 0u;
                idx[1] = vertex_ptr_ + 1u;
                idx[2] = vertex_ptr_ + 2u;
                idx[3] = vertex_ptr_ + 0u;
                idx[4] = vertex_ptr_ + 2u;
                idx[5] = vertex_ptr_ + 3u;

                auto* vtx = vertices_.append(4u);
                vtx[0] = vertex(vec2{ fade_start, min.y }, uv, col);
                vtx[1] = vertex(vec2{ fade_end, min.y }, uv, faded_col);
                vtx[2] = vertex(vec2{ fade_end, max.y }, uv, faded_col);
                vtx[3] = vertex(vec2{ fade_start, max.y }, uv, col);

                vertex_ptr_ += 4u;

                if (draw_if_faded) {
                    prim_rect(vec2(fade_end, min.y), max, faded_col);
                }
            }
            else {
                auto* idx = indices_.append(6u);
                idx[0] = vertex_ptr_ + 0u;
                idx[1] = vertex_ptr_ + 1u;
                idx[2] = vertex_ptr_ + 2u;
                idx[3] = vertex_ptr_ + 0u;
                idx[4] = vertex_ptr_ + 2u;
                idx[5] = vertex_ptr_ + 3u;

                const float t = (max.x - fade_start) / delta;
                const color_u32 interp_col =
                    color(col).interp(color(faded_col), t);

                auto* vtx = vertices_.append(4u);
                vtx[0] = vertex(vec2{ fade_start, min.y }, uv, col);
                vtx[1] = vertex(vec2{ max.x, min.y }, uv, interp_col);
                vtx[2] = vertex(vec2{ max.x, max.y }, uv, interp_col);
                vtx[3] = vertex(vec2{ fade_start, max.y }, uv, col);

                vertex_ptr_ += 4u;
            }
        }
        else {
            const float t_left_raw = (min.x - fade_start) / delta;
            const float t_right_raw = (max.x - fade_start) / delta;

            const float t_left = std::clamp(t_left_raw, 0.f, 1.f);
            const float t_right = std::clamp(t_right_raw, 0.f, 1.f);

            const color_u32 col_left = color(col).interp(color(faded_col), t_left);

            if (fade_end < max.x) {
                auto* idx = indices_.append(6u);
                idx[0] = vertex_ptr_ + 0u;
                idx[1] = vertex_ptr_ + 1u;
                idx[2] = vertex_ptr_ + 2u;
                idx[3] = vertex_ptr_ + 0u;
                idx[4] = vertex_ptr_ + 2u;
                idx[5] = vertex_ptr_ + 3u;

                auto* vtx = vertices_.append(4u);
                vtx[0] = vertex(min, uv, col_left);
                vtx[1] = vertex(vec2{ fade_end, min.y }, uv, faded_col);
                vtx[2] = vertex(vec2{ fade_end, max.y }, uv, faded_col);
                vtx[3] = vertex(vec2{ min.x, max.y }, uv, col_left);

                vertex_ptr_ += 4u;

                prim_rect(vec2(fade_end, min.y), vec2(max.x, max.y), faded_col);
            }
            else {
                const color_u32 col_right = color(col).interp(color(faded_col), t_right);

                auto* idx = indices_.append(6u);
                idx[0] = vertex_ptr_ + 0u;
                idx[1] = vertex_ptr_ + 1u;
                idx[2] = vertex_ptr_ + 2u;
                idx[3] = vertex_ptr_ + 0u;
                idx[4] = vertex_ptr_ + 2u;
                idx[5] = vertex_ptr_ + 3u;

                auto* vtx = vertices_.append(4u);
                vtx[0] = vertex(min, uv, col_left);
                vtx[1] = vertex(vec2{ max.x, min.y }, uv, col_right);
                vtx[2] = vertex(vec2{ max.x, max.y }, uv, col_right);
                vtx[3] = vertex(vec2{ min.x, max.y }, uv, col_left);

                vertex_ptr_ += 4u;
            }
        }
    }
}

inline void drawlist2d::add_shadow_rect_filled(const vec2& min, const vec2& max, color_u32 col, float rounding,
                                               float shadow_size, e_rounding_flags flags, float corner_step)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    path_rect(min, max, rounding, flags, corner_step);
    add_shadow_convex(
        path_.data(),
        static_cast<std::uint32_t>(path_.size()),
        col,
        shadow_size,
        true
    );
    path_clear();
}

inline void drawlist2d::add_quad_filled(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4, color_u32 col)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    assert(path_.empty());

    path_add_point(p1);
    path_add_point(p2);
    path_add_point(p3);
    path_add_point(p4);

    path_fill_convex(col);
}

inline void drawlist2d::add_quad_filled_multicolor(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4,
                                                   color_u32 col1, color_u32 col2, color_u32 col3, color_u32 col4)
{
    auto* idx = indices_.append(6u);
    idx[0] = vertex_ptr_ + 0u;
    idx[1] = vertex_ptr_ + 1u;
    idx[2] = vertex_ptr_ + 2u;
    idx[3] = vertex_ptr_ + 0u;
    idx[4] = vertex_ptr_ + 2u;
    idx[5] = vertex_ptr_ + 3u;

    const auto& uv = shared_data_->uv_white_px;

    auto* vtx = vertices_.append(4u);
    vtx[0] = vertex(p1, uv, col1);
    vtx[1] = vertex(p2, uv, col2);
    vtx[2] = vertex(p3, uv, col3);
    vtx[3] = vertex(p4, uv, col4);

    vertex_ptr_ += 4u;
}

inline void drawlist2d::add_line(const vec2& start, const vec2& end, color_u32 col, float line_width)
{
    add_line_multicolor(start, end, col, col, line_width);
}

inline void drawlist2d::add_line(const point_3d& start, const point_3d& end, color_u32 col, float line_width)
{
    add_line_multicolor(start, end, col, col, line_width);
}

inline void drawlist2d::add_image(texture_handle texture, const vec2& min, const vec2& max, color_u32 col,
                                  const vec2& uv_min, const vec2& uv_max)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    push_texture_id(texture);

    auto* idx = indices_.append(6u);
    idx[0] = vertex_ptr_ + 0u;
    idx[1] = vertex_ptr_ + 1u;
    idx[2] = vertex_ptr_ + 2u;
    idx[3] = vertex_ptr_ + 0u;
    idx[4] = vertex_ptr_ + 2u;
    idx[5] = vertex_ptr_ + 3u;

    auto* vtx = vertices_.append(4u);
    vtx[0] = vertex(min, uv_min, col);
    vtx[1] = vertex(vec2{ min.x, max.y }, vec2{ uv_min.x, uv_max.y }, col);
    vtx[2] = vertex(max, uv_max, col);
    vtx[3] = vertex(vec2{ max.x, min.y }, vec2{ uv_max.x, uv_min.y }, col);

    vertex_ptr_ += 4u;

    pop_texture_id();
}

inline void drawlist2d::add_image_outline(texture_handle texture, const vec2& min, const vec2& max, color_u32 col, color_u32 outline_col,
                                          float outline_size, const vec2& uv_min, const vec2& uv_max)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    push_texture_id(texture);

    // top
    {
        auto* idx = indices_.append(6u);
        idx[0] = vertex_ptr_ + 0u;
        idx[1] = vertex_ptr_ + 1u;
        idx[2] = vertex_ptr_ + 2u;
        idx[3] = vertex_ptr_ + 0u;
        idx[4] = vertex_ptr_ + 2u;
        idx[5] = vertex_ptr_ + 3u;

        auto* vtx = vertices_.append(4u);
        vtx[0] = vertex(vec2{ min.x, min.y - outline_size }, uv_min, outline_col);
        vtx[1] = vertex(vec2{ min.x, max.y - outline_size }, vec2{ uv_min.x, uv_max.y }, outline_col);
        vtx[2] = vertex(vec2{ max.x, max.y - outline_size }, uv_max, outline_col);
        vtx[3] = vertex(vec2{ max.x, min.y - outline_size }, vec2{ uv_max.x, uv_min.y }, outline_col);
    }

    vertex_ptr_ += 4u;

    {
        // bottom
        auto* idx = indices_.append(6u);
        idx[0] = vertex_ptr_ + 0u;
        idx[1] = vertex_ptr_ + 1u;
        idx[2] = vertex_ptr_ + 2u;
        idx[3] = vertex_ptr_ + 0u;
        idx[4] = vertex_ptr_ + 2u;
        idx[5] = vertex_ptr_ + 3u;

        auto* vtx = vertices_.append(4u);
        vtx[0] = vertex(vec2{ min.x, min.y + outline_size }, uv_min, outline_col);
        vtx[1] = vertex(vec2{ min.x, max.y + outline_size }, vec2{ uv_min.x, uv_max.y }, outline_col);
        vtx[2] = vertex(vec2{ max.x, max.y + outline_size }, uv_max, outline_col);
        vtx[3] = vertex(vec2{ max.x, min.y + outline_size }, vec2{ uv_max.x, uv_min.y }, outline_col);

        vertex_ptr_ += 4u;
    }

    {
        // left
        auto* idx = indices_.append(6u);
        idx[0] = vertex_ptr_ + 0u;
        idx[1] = vertex_ptr_ + 1u;
        idx[2] = vertex_ptr_ + 2u;
        idx[3] = vertex_ptr_ + 0u;
        idx[4] = vertex_ptr_ + 2u;
        idx[5] = vertex_ptr_ + 3u;

        auto* vtx = vertices_.append(4u);
        vtx[0] = vertex(vec2{ min.x - outline_size, min.y }, uv_min, outline_col);
        vtx[1] = vertex(vec2{ min.x - outline_size, max.y }, vec2{ uv_min.x, uv_max.y }, outline_col);
        vtx[2] = vertex(vec2{ max.x - outline_size, max.y }, uv_max, outline_col);
        vtx[3] = vertex(vec2{ max.x - outline_size, min.y }, vec2{ uv_max.x, uv_min.y }, outline_col);

        vertex_ptr_ += 4u;
    }

    {
        // right
        auto* idx = indices_.append(6u);
        idx[0] = vertex_ptr_ + 0u;
        idx[1] = vertex_ptr_ + 1u;
        idx[2] = vertex_ptr_ + 2u;
        idx[3] = vertex_ptr_ + 0u;
        idx[4] = vertex_ptr_ + 2u;
        idx[5] = vertex_ptr_ + 3u;

        auto* vtx = vertices_.append(4u);
        vtx[0] = vertex(vec2{ min.x + outline_size, min.y }, uv_min, outline_col);
        vtx[1] = vertex(vec2{ min.x + outline_size, max.y }, vec2{ uv_min.x, uv_max.y }, outline_col);
        vtx[2] = vertex(vec2{ max.x + outline_size, max.y }, uv_max, outline_col);
        vtx[3] = vertex(vec2{ max.x + outline_size, min.y }, vec2{ uv_max.x, uv_min.y }, outline_col);

        vertex_ptr_ += 4u;
    }

    {
        // main
        auto* idx = indices_.append(6u);
        idx[0] = vertex_ptr_ + 0u;
        idx[1] = vertex_ptr_ + 1u;
        idx[2] = vertex_ptr_ + 2u;
        idx[3] = vertex_ptr_ + 0u;
        idx[4] = vertex_ptr_ + 2u;
        idx[5] = vertex_ptr_ + 3u;

        auto* vtx = vertices_.append(4u);
        vtx[0] = vertex(min, uv_min, col);
        vtx[1] = vertex(vec2{ min.x, max.y }, vec2{ uv_min.x, uv_max.y }, col);
        vtx[2] = vertex(max, uv_max, col);
        vtx[3] = vertex(vec2{ max.x, min.y }, vec2{ uv_max.x, uv_min.y }, col);

        vertex_ptr_ += 4u;
    }

    pop_texture_id();
}

inline void drawlist2d::add_image_rounded(texture_handle texture, const vec2& min, const vec2& max, float rounding, color_u32 col,
                                          const vec2& uv_min, const vec2& uv_max)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]] {
        return;
    }

    push_texture_id(texture);

    const auto backup = vertex_ptr();
    add_rect_filled(min, max, col, rounding);

    shade_vertices_uv(
        backup,
        vertex_ptr(),
        min, max,
        uv_min, uv_max
    );

    pop_texture_id();
}

inline void drawlist2d::shade_vertices_uv(std::uint32_t vtx_start, std::uint32_t vtx_end, const vec2& min, const vec2& max,
                                          const vec2& uv_min, const vec2& uv_max)
{
    assert(vtx_start <= vtx_end);

    const vec2 d_pos = max - min;
    if (d_pos.x == 0.f || d_pos.y == 0.f)
        return;

    const vec2 inv_d_pos = vec2(1.f) / d_pos;
    const vec2 d_uv = uv_max - uv_min;

    assert(vtx_end <= vertices_.size());

    for (std::uint32_t i = vtx_start; i < vtx_end; i++) {
        auto& vtx = vertices_[i];

        vec2 d = (vtx.pos - min) * inv_d_pos;
        // clamp
        d.x = std::clamp(d.x, 0.f, 1.f);
        d.y = std::clamp(d.y, 0.f, 1.f);

        vtx.uv = uv_min + d_uv * d;
    }
}

inline void drawlist2d::shade_vertices_col(std::uint32_t vtx_start, std::uint32_t vtx_end, const vec2& min, const vec2& max,
                                           const color& col_tl, const color& col_tr, const color& col_br, const color& col_bl)
{
    assert(vtx_start <= vtx_end);

    const vec2 d_pos = max - min;
    if (d_pos.x == 0.f || d_pos.y == 0.f)
        return;

    const vec2 inv_d_pos = vec2(1.f) / d_pos;

    assert(vtx_end <= vertices_.size());

    for (std::uint32_t i = vtx_start; i < vtx_end; i++) {
        auto& vtx = vertices_[i];

        vec2 d = (vtx.pos - min) * inv_d_pos;

        d.x = std::clamp(d.x, 0.f, 1.f);
        d.y = std::clamp(d.y, 0.f, 1.f);

        const color a = col_tl.interp(col_tr, d.x);
        const color b = col_bl.interp(col_br, d.x);

        vtx.col = a.interp(b, d.y).alpha((vtx.col >> (3u * 8u)) & 0xFFu);
    }
}

inline void drawlist2d::shade_vertices_depth(std::uint32_t vtx_start, std::uint32_t vtx_end, const vec2& min, const vec2& max,
                                             float depth_tl, float depth_tr, float depth_br, float depth_bl)
{
    assert(vtx_start <= vtx_end);

    const vec2 d_pos = max - min;
    if (d_pos.x == 0.f || d_pos.y == 0.f)
        return;

    const vec2 inv_d_pos = vec2(1.f) / d_pos;

    assert(vtx_end <= vertices_.size());

    for (std::uint32_t i = vtx_start; i < vtx_end; i++) {
        auto& vtx = vertices_[i];

        vec2 d = (vtx.pos - min) * inv_d_pos;

        d.x = std::clamp(d.x, 0.f, 1.f);
        d.y = std::clamp(d.y, 0.f, 1.f);

        const float a = std::lerp(depth_tl, depth_tr, d.x);
        const float b = std::lerp(depth_bl, depth_br, d.x);

        vtx.depth = std::lerp(a, b, d.y);
    }
}

inline void drawlist2d::shade_vertices_depth(std::uint32_t vtx_start, std::uint32_t vtx_end, float depth)
{
    assert(vtx_start <= vtx_end);
    assert(vtx_end <= vertices_.size());

    for (std::uint32_t i = vtx_start; i < vtx_end; i++) {
        auto& vtx = vertices_[i];
        vtx.depth = depth;
    }
}

inline void drawlist2d::path_clear()
{
#ifdef _DEBUG
    assert_render_thread();
#endif // _DEBUG

    path_.clear();
}

inline void drawlist2d::path_clear3d()
{
#ifdef _DEBUG
    assert_render_thread();
#endif // _DEBUG

    path3d_.clear();
}

inline void drawlist2d::path_add_point(const vec2& p)
{
#ifdef _DEBUG
    assert_render_thread();
#endif // _DEBUG

    path_.emplace_back(p);
}

inline void drawlist2d::path_add_point(const vec2& p, float depth)
{
#ifdef _DEBUG
    assert_render_thread();
#endif // _DEBUG

    path3d_.emplace_back(p, depth);
}

inline void drawlist2d::path_add_point(const point_3d& p)
{
#ifdef _DEBUG
    assert_render_thread();
#endif // _DEBUG

    path3d_.emplace_back(p);
}

template <int a_min_of_12, int a_max_of_12>
inline void drawlist2d::path_arc_to(const vec2& center, float radius, float step)
{
    static_assert(a_min_of_12 < a_max_of_12);
    static_assert(a_min_of_12 >= 0 && a_min_of_12 < 12);
    static_assert(a_max_of_12 > 0 && a_max_of_12 <= 12);
    assert(radius >= 0.5f);
    assert(step > 0.0f);

    constexpr float kStart = (static_cast<float>(a_min_of_12) / 12.0f) * math::g_2_pi;
    constexpr float kEnd   = (static_cast<float>(a_max_of_12) / 12.0f) * math::g_2_pi;

    const float span = kEnd - kStart;
    const int n = (std::max)(1, static_cast<int>(std::ceil(span / step)));
    const float delta = span / static_cast<float>(n);

    float s = std::sin(kStart);
    float c = std::cos(kStart);
    const float sd = std::sin(delta);
    const float cd = std::cos(delta);

    for (int j = 0; j <= n; ++j) {
        path_.emplace_back(center.x + s * radius, center.y + c * radius);

        if (j != n) {
            const float s_next = s * cd + c * sd;
            const float c_next = c * cd - s * sd;
            s = s_next;
            c = c_next;
        }
    }
}

inline void drawlist2d::path_rect(const vec2& min, const vec2& max, float rounding, e_rounding_flags flags, float corner_step)
{
    float width = max.x - min.x;
    float height = max.y - min.y;

    rounding = (std::min)(rounding, width * (flags & e_rounding_flags::rounding_top ||
        flags & e_rounding_flags::rounding_bottom ? 0.5f : 1.f) - 1.f);
    rounding = (std::min)(rounding, height * (flags & e_rounding_flags::rounding_left ||
        flags & e_rounding_flags::rounding_right ? 0.5f : 1.f) - 1.f);

    if (rounding < 0.5f ||
        flags == e_rounding_flags::rounding_none) {
        path_add_point(min);
        path_add_point(vec2(max.x, min.y));
        path_add_point(max);
        path_add_point(vec2(min.x, max.y));
    }
    else {
        const float rounding_tl = flags & e_rounding_flags::rounding_topleft ? rounding : 0.f;
        const float rounding_tr = flags & e_rounding_flags::rounding_topright ? rounding : 0.f;
        const float rounding_bl = flags & e_rounding_flags::rounding_bottomleft ? rounding : 0.f;
        const float rounding_br = flags & e_rounding_flags::rounding_bottomright ? rounding : 0.f;
        const float corner_size = rounding * math::g_pi_div_2;
        const float step = corner_step / corner_size * 2.f;

        assert(rounding_tl + rounding_tr <= max.x - min.x);
        assert(rounding_bl + rounding_br <= max.x - min.x);
        assert(rounding_tl + rounding_bl <= max.y - min.y);
        assert(rounding_tr + rounding_br <= max.y - min.y);

        [[likely]] if (rounding_tl > 0.5f) {
            path_arc_to<6, 9>(vec2{ min.x + rounding_tl, min.y + rounding_tl }, rounding_tl, step);
        } else {
            path_.emplace_back(min.x, min.y);
        }

        [[likely]] if (rounding_bl > 0.5f) {
            path_arc_to<9, 12>(vec2{ min.x + rounding_bl, max.y - rounding_bl }, rounding_bl, step);
        } else {
            path_.emplace_back(min.x, max.y);
        }

        [[likely]] if (rounding_br > 0.5f) {
            path_arc_to<0, 3>(vec2{ max.x - rounding_br, max.y - rounding_br }, rounding_br, step);
        } else {
            path_.emplace_back(max.x, max.y);
        }

        [[likely]] if (rounding_tr > 0.5f) {
            path_arc_to<3, 6>(vec2{ max.x - rounding_tr, min.y + rounding_tr }, rounding_tr, step);
        } else {
            path_.emplace_back(max.x, min.y);
        }
    }
}

inline void drawlist2d::path_fill_convex(color_u32 col)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]] {
        path_clear();
        return;
    }

    add_convex_filled(
        path_.data(),
        static_cast<std::uint32_t>(path_.size()),
        col
    );

    path_clear();
}

inline void drawlist2d::path_stroke(color_u32 col, float line_width, bool closed)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]] {
        path_clear();
        return;
    }

    add_lines(
        path_.data(),
        static_cast<std::uint32_t>(path_.size()),
        col,
        line_width,
        closed
    );

    path_clear();
}

template <float CharOffset, unicode::string_like String>
inline void drawlist2d::add_text(const vec2& pos, color_u32 col, const String& text, bool blurred)
{
#ifdef _DEBUG
    assert_render_thread();
#endif // _DEBUG
    assert(current_font_ != nullptr);

    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    const float line_height = static_cast<float>(current_font_->cfg().size);
    const std::uint32_t length = static_cast<std::uint32_t>(text.length());

    std::uint32_t s = 0u;
    float x = pos.x;
    float y = pos.y;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            if (cp == U'\n') {
                x = pos.x;
                y += line_height;
                continue;
            }
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = blurred ?
            current_font_->find_glyph_blurred(cp) : current_font_->find_glyph(cp);
        if (glyph == nullptr)
            continue;

        if (glyph->visible) {
            const float x0 = x + glyph->x0;
            const float x1 = x + glyph->x1;
            const float y0 = y + glyph->y0;
            const float y1 = y + glyph->y1;

            if (x0 <= header_.clip_rect.right &&
                x1 >= header_.clip_rect.left &&
                y0 <= header_.clip_rect.bottom &&
                y1 >= header_.clip_rect.top) {
                auto* idx = indices_.append(6u);
                idx[0] = vertex_ptr_ + 0u;
                idx[1] = vertex_ptr_ + 1u;
                idx[2] = vertex_ptr_ + 2u;
                idx[3] = vertex_ptr_ + 0u;
                idx[4] = vertex_ptr_ + 2u;
                idx[5] = vertex_ptr_ + 3u;

                auto* vtx = vertices_.append(4u);
                vtx[0] = vertex(vec2{ x0, y0 }, glyph->uv_min, col);
                vtx[1] = vertex(vec2{ x0, y1 }, vec2{ glyph->uv_min.x, glyph->uv_max.y }, col);
                vtx[2] = vertex(vec2{ x1, y1 }, glyph->uv_max, col);
                vtx[3] = vertex(vec2{ x1, y0 }, vec2{ glyph->uv_max.x, glyph->uv_min.y }, col);

                vertex_ptr_ += 4u;
            }
        }

        x += glyph->advance_x;
        if constexpr (CharOffset != 0.f) {
            x += CharOffset;
        }
    }
}

template <float CharOffset, unicode::string_like String>
inline void drawlist2d::add_text_faded(const vec2& pos, color_u32 col, color_u32 faded_col,
                                       float fade_start, float fade_end, const String& text, bool blurred)
{
    assert(fade_start <= fade_end);
    if (col == faded_col) [[unlikely]]
        return add_text(pos, col, text, blurred);

    assert(current_font_ != nullptr);

    const bool draw_no_fade = (col & color::alpha_mask) != 0u;
    const bool draw_if_faded = (faded_col & color::alpha_mask) != 0u;
    if (!draw_no_fade &&
        !draw_if_faded) [[unlikely]]
        return;

    const float line_height = static_cast<float>(current_font_->cfg().size);
    const std::uint32_t length = static_cast<std::uint32_t>(text.length());

    const bool do_fade = (fade_end > fade_start);
    if (!do_fade &&
        !draw_no_fade &&
        !draw_if_faded) [[unlikely]]
        return;

    const float denom = fade_end - fade_start;
    const float inv_denom = denom > 0.f ? 1.f / denom : 0.f;

    std::uint32_t s = 0u;
    float x = pos.x;
    float y = pos.y;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            if (cp == U'\n') {
                x = pos.x;
                y += line_height;
                continue;
            }
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = blurred ?
            current_font_->find_glyph_blurred(cp) : current_font_->find_glyph(cp);
        if (glyph == nullptr)
            continue;

        if (glyph->visible) {
            const float x0 = x + glyph->x0;
            const float x1 = x + glyph->x1;
            const float y0 = y + glyph->y0;
            const float y1 = y + glyph->y1;

            if (x0 <= header_.clip_rect.right &&
                x1 >= header_.clip_rect.left &&
                y0 <= header_.clip_rect.bottom &&
                y1 >= header_.clip_rect.top) {
                color_u32 c_left = col;
                color_u32 c_right = col;
                const vec2 uv_min(glyph->uv_min);
                const vec2 uv_max(glyph->uv_max);
                float mid_left_pos = x0;
                float mid_right_pos = x1;
                float mid_left = uv_min.x;
                float mid_right = uv_max.x;

                if (do_fade) {
                    if (x0 >= fade_end) {
                        if (!draw_if_faded) {
                            x += glyph->advance_x;
                            continue;
                        }
                        c_left = faded_col;
                        c_right = faded_col;
                    }
                    else {
                        if (x0 < fade_start && fade_start < x1) {
                            const float d = (fade_start - x0) / (x1 - x0);
                            mid_left = uv_min.x + (uv_max.x - uv_min.x) * d;
                            mid_left_pos = x0 + (x1 - x0) * d;

                            if (draw_no_fade) {
                                auto* idx = indices_.append(6u);
                                idx[0] = vertex_ptr_ + 0u;
                                idx[1] = vertex_ptr_ + 1u;
                                idx[2] = vertex_ptr_ + 2u;
                                idx[3] = vertex_ptr_ + 0u;
                                idx[4] = vertex_ptr_ + 2u;
                                idx[5] = vertex_ptr_ + 3u;

                                auto* vtx = vertices_.append(4u);
                                vtx[0] = vertex(vec2{ x0, y0 }, vec2{ uv_min.x, uv_min.y }, col);
                                vtx[1] = vertex(vec2{ x0, y1 }, vec2{ uv_min.x, uv_max.y }, col);
                                vtx[2] = vertex(vec2{ mid_left_pos, y1 }, vec2{ mid_left, uv_max.y }, col);
                                vtx[3] = vertex(vec2{ mid_left_pos, y0 }, vec2{ mid_left, uv_min.y }, col);

                                vertex_ptr_ += 4u;
                            }
                        }

                        if (fade_end < x1) {
                            assert(x0 < fade_end);

                            const float d = (fade_end - x0) / (x1 - x0);
                            mid_right = uv_min.x + (uv_max.x - uv_min.x) * d;
                            mid_right_pos = x0 + (x1 - x0) * d;
                            if (draw_if_faded) {
                                auto* idx = indices_.append(6u);
                                idx[0] = vertex_ptr_ + 0u;
                                idx[1] = vertex_ptr_ + 1u;
                                idx[2] = vertex_ptr_ + 2u;
                                idx[3] = vertex_ptr_ + 0u;
                                idx[4] = vertex_ptr_ + 2u;
                                idx[5] = vertex_ptr_ + 3u;

                                auto* vtx = vertices_.append(4u);
                                vtx[0] = vertex(vec2{ mid_right_pos, y0 }, vec2{ mid_right, uv_min.y }, faded_col);
                                vtx[1] = vertex(vec2{ mid_right_pos, y1 }, vec2{ mid_right, uv_max.y }, faded_col);
                                vtx[2] = vertex(vec2{ x1, y1 }, vec2{ uv_max.x, uv_max.y }, faded_col);
                                vtx[3] = vertex(vec2{ x1, y0 }, vec2{ uv_max.x, uv_min.y }, faded_col);

                                vertex_ptr_ += 4u;
                            }
                        }

                        const float t_left = (mid_left_pos - fade_start) * inv_denom;
                        c_left = color(col).interp(color(faded_col), std::clamp(t_left, 0.f, 1.f));

                        const float t_right = (mid_right_pos - fade_start) * inv_denom;
                        c_right = color(col).interp(color(faded_col), std::clamp(t_right, 0.f, 1.f));
                    }
                }
                else {
                    if (x0 >= fade_start) {
                        if (!draw_if_faded) {
                            x += glyph->advance_x;
                            continue;
                        }
                        c_left = faded_col;
                        c_right = faded_col;
                    }
                    else if (fade_start < x1) {
                        const float d = (fade_start - x0) / (x1 - x0);
                        const float mid_pos = x0 + (x1 - x0) * d;
                        const float mid_uv = uv_min.x + (uv_max.x - uv_min.x) * d;

                        if (draw_no_fade) {
                            auto* idx = indices_.append(6u);
                            idx[0] = vertex_ptr_ + 0u;
                            idx[1] = vertex_ptr_ + 1u;
                            idx[2] = vertex_ptr_ + 2u;
                            idx[3] = vertex_ptr_ + 0u;
                            idx[4] = vertex_ptr_ + 2u;
                            idx[5] = vertex_ptr_ + 3u;

                            auto* vtx = vertices_.append(4u);
                            vtx[0] = vertex(vec2{ x0, y0 }, vec2{ uv_min.x, uv_min.y }, col);
                            vtx[1] = vertex(vec2{ x0, y1 }, vec2{ uv_min.x, uv_max.y }, col);
                            vtx[2] = vertex(vec2{ mid_pos, y1 }, vec2{ mid_uv, uv_max.y }, col);
                            vtx[3] = vertex(vec2{ mid_pos, y0 }, vec2{ mid_uv, uv_min.y }, col);

                            vertex_ptr_ += 4u;
                        }

                        mid_left_pos = mid_pos;
                        mid_left = mid_uv;

                        c_left = faded_col;
                        c_right = faded_col;
                    }
                }

                // middle/main quad
                auto* idx = indices_.append(6u);
                idx[0] = vertex_ptr_ + 0u;
                idx[1] = vertex_ptr_ + 1u;
                idx[2] = vertex_ptr_ + 2u;
                idx[3] = vertex_ptr_ + 0u;
                idx[4] = vertex_ptr_ + 2u;
                idx[5] = vertex_ptr_ + 3u;

                auto* vtx = vertices_.append(4u);
                vtx[0] = vertex(vec2{ mid_left_pos,  y0 }, vec2{ mid_left,  uv_min.y }, c_left);
                vtx[1] = vertex(vec2{ mid_left_pos,  y1 }, vec2{ mid_left,  uv_max.y }, c_left);
                vtx[2] = vertex(vec2{ mid_right_pos, y1 }, vec2{ mid_right, uv_max.y }, c_right);
                vtx[3] = vertex(vec2{ mid_right_pos, y0 }, vec2{ mid_right, uv_min.y }, c_right);

                vertex_ptr_ += 4u;
            }
        }

        x += glyph->advance_x;
        if constexpr (CharOffset != 0.f) {
            x += CharOffset;
        }
    }
}

template<float CharOffset, unicode::string_like String>
inline void drawlist2d::add_text_outlined(const vec2& pos, color_u32 col, const String& text,
                                          const color_u32 outline_col, float outline_width, bool blurred)
{
    assert(current_font_ != nullptr);

    const bool draw_no_outline = (col & color::alpha_mask) != 0u;
    const bool draw_outline = (outline_col & color::alpha_mask) != 0u;
    if (!draw_no_outline &&
        !draw_outline) [[unlikely]]
        return;

    const float line_height = static_cast<float>(current_font_->cfg().size);
    const std::uint32_t length = static_cast<std::uint32_t>(text.length());

    std::uint32_t s = 0u;
    float x = pos.x;
    float y = pos.y;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            if (cp == U'\n') {
                x = pos.x;
                y += line_height;
                continue;
            }
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = blurred ?
            current_font_->find_glyph_blurred(cp) : current_font_->find_glyph(cp);
        if (glyph == nullptr)
            continue;

        if (glyph->visible) {
            const float x0 = x + glyph->x0;
            const float x1 = x + glyph->x1;
            const float y0 = y + glyph->y0;
            const float y1 = y + glyph->y1;

            if (x0 - outline_width <= header_.clip_rect.right &&
                x1 + outline_width >= header_.clip_rect.left &&
                y0 - outline_width <= header_.clip_rect.bottom &&
                y1 + outline_width >= header_.clip_rect.top) {

                if (draw_outline) {
                    // left
                    auto* idx = indices_.append(6u);
                    idx[0] = vertex_ptr_ + 0u;
                    idx[1] = vertex_ptr_ + 1u;
                    idx[2] = vertex_ptr_ + 2u;
                    idx[3] = vertex_ptr_ + 0u;
                    idx[4] = vertex_ptr_ + 2u;
                    idx[5] = vertex_ptr_ + 3u;

                    auto* vtx = vertices_.append(4u);
                    vtx[0] = vertex(vec2{ x0 - outline_width, y0 }, glyph->uv_min, outline_col);
                    vtx[1] = vertex(vec2{ x0 - outline_width, y1 }, vec2{ glyph->uv_min.x, glyph->uv_max.y }, outline_col);
                    vtx[2] = vertex(vec2{ x1 - outline_width, y1 }, glyph->uv_max, outline_col);
                    vtx[3] = vertex(vec2{ x1 - outline_width, y0 }, vec2{ glyph->uv_max.x, glyph->uv_min.y }, outline_col);

                    vertex_ptr_ += 4u;

                    {
                        // right
                        auto* idx = indices_.append(6u);
                        idx[0] = vertex_ptr_ + 0u;
                        idx[1] = vertex_ptr_ + 1u;
                        idx[2] = vertex_ptr_ + 2u;
                        idx[3] = vertex_ptr_ + 0u;
                        idx[4] = vertex_ptr_ + 2u;
                        idx[5] = vertex_ptr_ + 3u;

                        auto* vtx = vertices_.append(4u);
                        vtx[0] = vertex(vec2{ x0 + outline_width, y0 }, glyph->uv_min, outline_col);
                        vtx[1] = vertex(vec2{ x0 + outline_width, y1 }, vec2{ glyph->uv_min.x, glyph->uv_max.y }, outline_col);
                        vtx[2] = vertex(vec2{ x1 + outline_width, y1 }, glyph->uv_max, outline_col);
                        vtx[3] = vertex(vec2{ x1 + outline_width, y0 }, vec2{ glyph->uv_max.x, glyph->uv_min.y }, outline_col);

                        vertex_ptr_ += 4u;
                    }

                    {
                        // top
                        auto* idx = indices_.append(6u);
                        idx[0] = vertex_ptr_ + 0u;
                        idx[1] = vertex_ptr_ + 1u;
                        idx[2] = vertex_ptr_ + 2u;
                        idx[3] = vertex_ptr_ + 0u;
                        idx[4] = vertex_ptr_ + 2u;
                        idx[5] = vertex_ptr_ + 3u;

                        auto* vtx = vertices_.append(4u);
                        vtx[0] = vertex(vec2{ x0, y0 - outline_width }, glyph->uv_min, outline_col);
                        vtx[1] = vertex(vec2{ x0, y1 - outline_width }, vec2{ glyph->uv_min.x, glyph->uv_max.y }, outline_col);
                        vtx[2] = vertex(vec2{ x1, y1 - outline_width }, glyph->uv_max, outline_col);
                        vtx[3] = vertex(vec2{ x1, y0 - outline_width }, vec2{ glyph->uv_max.x, glyph->uv_min.y }, outline_col);

                        vertex_ptr_ += 4u;
                    }

                    {
                        // bottom
                        auto* idx = indices_.append(6u);
                        idx[0] = vertex_ptr_ + 0u;
                        idx[1] = vertex_ptr_ + 1u;
                        idx[2] = vertex_ptr_ + 2u;
                        idx[3] = vertex_ptr_ + 0u;
                        idx[4] = vertex_ptr_ + 2u;
                        idx[5] = vertex_ptr_ + 3u;

                        auto* vtx = vertices_.append(4u);
                        vtx[0] = vertex(vec2{ x0, y0 + outline_width }, glyph->uv_min, outline_col);
                        vtx[1] = vertex(vec2{ x0, y1 + outline_width }, vec2{ glyph->uv_min.x, glyph->uv_max.y }, outline_col);
                        vtx[2] = vertex(vec2{ x1, y1 + outline_width }, glyph->uv_max, outline_col);
                        vtx[3] = vertex(vec2{ x1, y0 + outline_width }, vec2{ glyph->uv_max.x, glyph->uv_min.y }, outline_col);

                        vertex_ptr_ += 4u;
                    }
                }

                if (draw_no_outline) {
                    auto* idx = indices_.append(6u);
                    idx[0] = vertex_ptr_ + 0u;
                    idx[1] = vertex_ptr_ + 1u;
                    idx[2] = vertex_ptr_ + 2u;
                    idx[3] = vertex_ptr_ + 0u;
                    idx[4] = vertex_ptr_ + 2u;
                    idx[5] = vertex_ptr_ + 3u;

                    auto* vtx = vertices_.append(4u);
                    vtx[0] = vertex(vec2{ x0, y0 }, glyph->uv_min, col);
                    vtx[1] = vertex(vec2{ x0, y1 }, vec2{ glyph->uv_min.x, glyph->uv_max.y }, col);
                    vtx[2] = vertex(vec2{ x1, y1 }, glyph->uv_max, col);
                    vtx[3] = vertex(vec2{ x1, y0 }, vec2{ glyph->uv_max.x, glyph->uv_min.y }, col);

                    vertex_ptr_ += 4u;
                }
            }
        }

        x += glyph->advance_x;
        if constexpr (CharOffset != 0.f) {
            x += CharOffset;
        }
    }
}

template<std::integral T>
void drawlist2d::add_convex_filled(const vec2* points, T num_points, color_u32 col)
{
    assert(num_points >= 0);

    add_convex_filled(
        points,
        static_cast<std::uint32_t>(num_points),
        col
    );
}

template<std::integral T>
void drawlist2d::add_shadow_convex(const vec2* points, T num_points, color_u32 col, float shadow_size, bool filled)
{
    assert(num_points >= 0);

    add_shadow_convex(
        points,
        static_cast<std::uint32_t>(num_points),
        col,
        shadow_size,
        filled
    );
}

template<std::integral T>
void drawlist2d::add_lines(const vec2* points, T num_points, color_u32 col, float line_width, bool closed)
{
    assert(num_points >= 0);

    add_lines(
        points,
        static_cast<std::uint32_t>(num_points),
        col,
        line_width,
        closed
    );
}

template<std::integral T>
inline void drawlist2d::add_convex_filled(const point_3d* points, T num_points, color_u32 col)
{
    assert(num_points >= 0);

    add_convex_filled(
        points,
        static_cast<std::uint32_t>(num_points),
        col
    );
}

template<std::integral T>
inline void drawlist2d::add_shadow_convex(const point_3d* points, T num_points, color_u32 col, float shadow_size, bool filled)
{
    assert(num_points >= 0);

    add_shadow_convex(
        points,
        static_cast<std::uint32_t>(num_points),
        col,
        shadow_size,
        filled
    );
}

template<std::integral T>
inline void drawlist2d::add_lines(const point_3d* points, T num_points, color_u32 col, float line_width, bool closed)
{
    assert(num_points >= 0);

    add_lines(
        points,
        static_cast<std::uint32_t>(num_points),
        col,
        line_width,
        closed
    );
}

r2_end_