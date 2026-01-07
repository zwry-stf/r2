#pragma once
#include <algorithm>
#include "font/font.h"
#include "renderer.h"


static_assert(sizeof(r2::renderer2d) > 0u);

r2_begin_

inline draw_cmd& renderer2d::add_draw_cmd()
{
    assert(cmds_.empty() ||
        vertex_ptr_ == vertices_.size() - cmds_.back().vertex_start);
    vertex_ptr_ = 0u;

    auto& ret = cmds_.emplace_back();
    ret.index_start = static_cast<std::uint32_t>(indices_.size());
    ret.vertex_start = static_cast<std::uint32_t>(vertices_.size());
    ret.clip_rect = header_.clip_rect;
    ret.texture = header_.texture;
    return ret;
}

template<typename O>
inline void renderer2d::on_changed_header(const O& new_value, O draw_cmd::* field)
{
    assert(!cmds_.empty());

    if constexpr (std::is_same_v<O, decltype(draw_cmd::clip_rect)>) {
        header_.clip_rect = new_value;
    }
    else if constexpr (std::is_same_v<O, decltype(draw_cmd::texture)>) {
        header_.texture = new_value;
    }

    auto& curr_cmd = cmds_.back();

    if (curr_cmd.*field != new_value) {
        if (indices_.size() > curr_cmd.index_start) {
            add_draw_cmd();
        }
        else {
            curr_cmd.*field = new_value;
        }
    }
}

inline void renderer2d::set_clip_rect(const rect& r)
{
    on_changed_header(r, &draw_cmd::clip_rect);
}

inline void renderer2d::push_clip_rect(const vec2& min, const vec2& max, bool intersect_current)
{
    push_clip_rect({
        static_cast<std::int32_t>(min.x),
        static_cast<std::int32_t>(min.y),
        static_cast<std::int32_t>(max.x),
        static_cast<std::int32_t>(max.y),
        },
        intersect_current
    );
}

inline void renderer2d::push_clip_rect(const rect& r, bool intersect_current)
{
    rect rect = r;
    if (intersect_current) {
        if (header_.clip_rect.left > rect.left)
            rect.left = header_.clip_rect.left;
        if (header_.clip_rect.right < rect.right)
            rect.right = header_.clip_rect.right;
        if (header_.clip_rect.top > rect.top)
            rect.top = header_.clip_rect.top;
        if (header_.clip_rect.bottom < rect.bottom)
            rect.bottom = header_.clip_rect.bottom;
    }

    clip_rect_stack_.push_back(rect);
    set_clip_rect(rect);
}

inline void renderer2d::pop_clip_rect()
{
    assert(clip_rect_stack_.size() > 1u);

    clip_rect_stack_.pop_back();
    const auto& rect = clip_rect_stack_.back();

    on_changed_header(rect, &draw_cmd::clip_rect);
}

inline void renderer2d::set_current_texture(texture_handle texture)
{
    on_changed_header(texture, &draw_cmd::texture);
}

inline void renderer2d::push_texture_id(texture_handle texture)
{
    texture_stack_.push_back(texture);
    set_current_texture(texture);
}

inline void renderer2d::push_texture_id(textureview* texture)
{
    assert(texture != nullptr && 
           texture->desc().usage == view_usage::shader_resource);
    push_texture_id(texture->native_texture_handle());
}

inline void renderer2d::pop_texture_id()
{
    assert(texture_stack_.size() > 1u);

    texture_stack_.pop_back();
    set_current_texture(texture_stack_.back());
}

inline void renderer2d::set_current_font(font* font)
{
    current_font_ = font;
}

inline void renderer2d::push_font(font* font)
{
    font_stack_.push_back(font);
    set_current_font(font);
}

inline void renderer2d::pop_font()
{
    assert(font_stack_.size() > 1u);

    font_stack_.pop_back();
    set_current_font(font_stack_.back());
}

inline void renderer2d::aa_side(const vec2& start, const vec2& end, std::uint32_t vtx_start, std::uint32_t vtx_end, color_u32 col)
{
    const vec2 d = (end - start);
    const float length = d.length();
    if (length < 1e-3f)
        return;

    const vec2 dir = d * vec2(1.f / length);
    float diag_factor = (std::min)(std::abs(dir.x), std::abs(dir.y)) * 1.41421356f; // *sqrt(2)
    diag_factor = std::clamp(diag_factor, 0.0f, 1.0f);
    constexpr float p = 0.7f;
    diag_factor = std::pow(diag_factor, p);

    const float aa_scale = aa_scale_ * diag_factor;
    if (aa_scale > 1e-3f) {
        const vec2 aa = dir.perp() * vec2(aa_scale);

        indices_.push_back(vtx_start);
        indices_.push_back(vertex_ptr_ + 0u);
        indices_.push_back(vertex_ptr_ + 1u);
        indices_.push_back(vtx_start);
        indices_.push_back(vertex_ptr_ + 1u);
        indices_.push_back(vtx_end);

        vertex_ptr_ += 2u;

        vertices_.emplace_back(start + aa, shared_data_.uv_white_px, col);
        vertices_.emplace_back(end + aa, shared_data_.uv_white_px, col);
    }
}

inline void renderer2d::add_line(const vec2& start, const vec2& end, color_u32 col, float line_width)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    assert(path_.empty());

    const vec2 d = (end - start);
    const float length = d.length();
    if (length < 1e-6f)
        return;

    const vec2 dir = d * vec2(1.f / length);
    const vec2 n = dir.perp() * vec2(line_width * 0.5f);

    // main quad
    indices_.push_back(vertex_ptr_ + 0u);
    indices_.push_back(vertex_ptr_ + 1u);
    indices_.push_back(vertex_ptr_ + 2u);
    indices_.push_back(vertex_ptr_ + 0u);
    indices_.push_back(vertex_ptr_ + 2u);
    indices_.push_back(vertex_ptr_ + 3u);

    vertices_.emplace_back(start - n, shared_data_.uv_white_px, col);
    vertices_.emplace_back(start + n, shared_data_.uv_white_px, col);
    vertices_.emplace_back(end + n, shared_data_.uv_white_px, col);
    vertices_.emplace_back(end - n, shared_data_.uv_white_px, col);

    const auto backup = vertex_ptr_;
    vertex_ptr_ += 4u;

    if (!flags_.anti_aliased_lines) [[unlikely]]
        return;

    const color_u32 col_no_alpha = col & ~color::alpha_mask;

    aa_side(start + n, end + n, backup + 1u, backup + 2u, col_no_alpha);
    aa_side(end - n, start - n, backup + 3u, backup + 0u, col_no_alpha);
}

inline void renderer2d::prim_rect(const vec2& min, const vec2& max, color_u32 col)
{
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 1u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 3u);

    const vec2& uv = shared_data_.uv_white_px;
    vertices_.emplace_back(vec2{ min.x, min.y }, uv, col);
    vertices_.emplace_back(vec2{ min.x, max.y }, uv, col);
    vertices_.emplace_back(vec2{ max.x, max.y }, uv, col);
    vertices_.emplace_back(vec2{ max.x, min.y }, uv, col);

    vertex_ptr_ += 4u;
}

inline void renderer2d::add_rect(const vec2& min, const vec2& max, color_u32 col, float line_width, float rounding,
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

inline void renderer2d::add_rect_inner(const vec2& min, const vec2& max, color_u32 col, float line_width, float rounding,
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

inline void renderer2d::add_rect_inner_fast(const vec2& min, const vec2& max, color_u32 col, float line_width)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    // top
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 1u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 3u);

    const auto& uv = shared_data_.uv_white_px;

    vertices_.emplace_back(min, uv, col);
    vertices_.emplace_back(min + vec2(line_width), uv, col);
    vertices_.emplace_back(vec2(max.x - line_width, min.y + line_width), uv, col);
    vertices_.emplace_back(vec2(max.x, min.y), uv, col);

    vertex_ptr_ += 4u;

    // bottom
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 1u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 3u);

    vertices_.emplace_back(vec2(min.x, max.y), uv, col);
    vertices_.emplace_back(max, uv, col);
    vertices_.emplace_back(vec2(max.x - line_width, max.y - line_width), uv, col);
    vertices_.emplace_back(vec2(min.x + line_width, max.y - line_width), uv, col);

    vertex_ptr_ += 4u;

    // left
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 1u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 3u);

    vertices_.emplace_back(min, uv, col);
    vertices_.emplace_back(vec2(min.x, max.y), uv, col);
    vertices_.emplace_back(vec2(min.x + line_width, max.y - line_width), uv, col);
    vertices_.emplace_back(vec2(min.x + line_width, min.y + line_width), uv, col);

    vertex_ptr_ += 4u;

    // right
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 1u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 3u);

    vertices_.emplace_back(vec2(max.x, min.y), uv, col);
    vertices_.emplace_back(vec2(max.x - line_width, min.y + line_width), uv, col);
    vertices_.emplace_back(vec2(max.x - line_width, max.y - line_width), uv, col);
    vertices_.emplace_back(max, uv, col);

    vertex_ptr_ += 4u;
}

inline void renderer2d::add_rect_filled_multicolor(const vec2& min, const vec2& max,
                                                   color_u32 col_tl, color_u32 col_tr, color_u32 col_br, color_u32 col_bl)
{
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 1u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 3u);

    const auto& uv = shared_data_.uv_white_px;

    vertices_.emplace_back(min, uv, col_tl);
    vertices_.emplace_back(vec2{ min.x, max.y }, uv, col_bl);
    vertices_.emplace_back(max, uv, col_br);
    vertices_.emplace_back(vec2{ max.x, min.y }, uv, col_tr);

    vertex_ptr_ += 4u;
}

inline void renderer2d::add_rect_filled(const vec2& min, const vec2& max, color_u32 col,
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

inline void renderer2d::add_rect_filled_faded(const vec2& min, const vec2& max, color_u32 col, color_u32 faded_col, float fade_start, float fade_end)
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

        const vec2& uv = shared_data_.uv_white_px;
        if (fade_start > min.x) {
            if (draw_no_fade) {
                prim_rect(min, vec2(fade_start, max.y), col);
            }

            if (fade_end < max.x) {
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 1u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 3u);

                vertices_.emplace_back(vec2{ fade_start, min.y }, uv, col);
                vertices_.emplace_back(vec2{ fade_end, min.y }, uv, faded_col);
                vertices_.emplace_back(vec2{ fade_end, max.y }, uv, faded_col);
                vertices_.emplace_back(vec2{ fade_start, max.y }, uv, col);

                vertex_ptr_ += 4u;

                if (draw_if_faded) {
                    prim_rect(vec2(fade_end, min.y), max, faded_col);
                }
            }
            else {
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 1u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 3u);

                const float t = (max.x - fade_start) / delta;
                const color_u32 interp_col =
                    color(col).interp(color(faded_col), t);

                vertices_.emplace_back(vec2{ fade_start, min.y }, uv, col);
                vertices_.emplace_back(vec2{ max.x, min.y }, uv, interp_col);
                vertices_.emplace_back(vec2{ max.x, max.y }, uv, interp_col);
                vertices_.emplace_back(vec2{ fade_start, max.y }, uv, col);

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
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 1u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 3u);

                vertices_.emplace_back(min, uv, col_left);
                vertices_.emplace_back(vec2{ fade_end, min.y }, uv, faded_col);
                vertices_.emplace_back(vec2{ fade_end, max.y }, uv, faded_col);
                vertices_.emplace_back(vec2{ min.x, max.y }, uv, col_left);

                vertex_ptr_ += 4u;

                prim_rect(vec2(fade_end, min.y), vec2(max.x, max.y), faded_col);
            }
            else {
                const color_u32 col_right = color(col).interp(color(faded_col), t_right);

                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 1u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 3u);

                vertices_.emplace_back(min, uv, col_left);
                vertices_.emplace_back(vec2{ max.x, min.y }, uv, col_right);
                vertices_.emplace_back(vec2{ max.x, max.y }, uv, col_right);
                vertices_.emplace_back(vec2{ min.x, max.y }, uv, col_left);

                vertex_ptr_ += 4u;
            }
        }
    }
}

inline void renderer2d::add_shadow_rect_filled(const vec2& min, const vec2& max, color_u32 col, float rounding,
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

inline void renderer2d::add_quad_filled(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4, color_u32 col)
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

inline void renderer2d::add_quad_filled_multicolor(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4, 
                                                   color_u32 col1, color_u32 col2, color_u32 col3, color_u32 col4)
{
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 1u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 3u);

    const auto& uv = shared_data_.uv_white_px;

    vertices_.emplace_back(p1, uv, col1);
    vertices_.emplace_back(p2, uv, col2);
    vertices_.emplace_back(p3, uv, col3);
    vertices_.emplace_back(p4, uv, col4);

    vertex_ptr_ += 4u;
}

inline void renderer2d::add_image(texture_handle texture, const vec2& min, const vec2& max, color_u32 col,
                                  const vec2& uv_min, const vec2& uv_max)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    push_texture_id(texture);

    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 1u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 0u);
    indices_.emplace_back(vertex_ptr_ + 2u);
    indices_.emplace_back(vertex_ptr_ + 3u);

    vertices_.emplace_back(min, uv_min, col);
    vertices_.emplace_back(vec2{ min.x, max.y }, vec2{ uv_min.x, uv_max.y }, col);
    vertices_.emplace_back(max, uv_max, col);
    vertices_.emplace_back(vec2{ max.x, min.y }, vec2{ uv_max.x, uv_min.y }, col);

    vertex_ptr_ += 4u;

    pop_texture_id();
}

inline void renderer2d::add_image_rounded(texture_handle texture, const vec2& min, const vec2& max, float rounding, color_u32 col,
                                          const vec2& uv_min, const vec2& uv_max)
{
    if ((col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    push_texture_id(texture);

    const auto backup = vertex_ptr_;
    add_rect_filled(min, max, col, rounding);

    shade_vertices_uv(
        backup,
        vertex_ptr_,
        min, max,
        uv_min, uv_max
    );

    pop_texture_id();
}

inline void renderer2d::shade_vertices_uv(std::uint32_t vtx_start, std::uint32_t vtx_end, const vec2& min, const vec2& max,
                                          const vec2& uv_min, const vec2& uv_max)
{
    assert(vtx_start <= vtx_end);

    const vec2 d_pos = max - min;
    if (d_pos.x == 0.f || d_pos.y == 0.f)
        return;

    const vec2 inv_d_pos = vec2(1.f) / d_pos;
    const vec2 d_uv = uv_max - uv_min;

    const auto& curr_cmd = cmds_.back();
    assert(curr_cmd.vertex_start + vtx_end <= vertices_.size());

    for (std::uint32_t i = curr_cmd.vertex_start + vtx_start;
         i < curr_cmd.vertex_start + vtx_end; i++) {
        auto& vtx = vertices_[i];

        vec2 d = (vtx.pos - min) * inv_d_pos;
        // clamp
        d.x = std::clamp(d.x, 0.f, 1.f);
        d.y = std::clamp(d.y, 0.f, 1.f);

        vtx.uv = uv_min + d_uv * d;
    }
}

inline void renderer2d::shade_vertices_col(std::uint32_t vtx_start, std::uint32_t vtx_end, const vec2& min, const vec2& max, 
                                           const color& col_tl, const color& col_tr, const color& col_br, const color& col_bl)
{
    assert(vtx_start <= vtx_end);

    const vec2 d_pos = max - min;
    if (d_pos.x == 0.f || d_pos.y == 0.f)
        return;

    const vec2 inv_d_pos = vec2(1.f) / d_pos;

    const auto& curr_cmd = cmds_.back();
    assert(curr_cmd.vertex_start + vtx_end <= vertices_.size());

    for (std::uint32_t i = curr_cmd.vertex_start + vtx_start;
         i < curr_cmd.vertex_start + vtx_end; i++) {
        auto& vtx = vertices_[i];

        vec2 d = (vtx.pos - min) * inv_d_pos;
        // clamp
        d.x = std::clamp(d.x, 0.f, 1.f);
        d.y = std::clamp(d.y, 0.f, 1.f);

        const color a = col_tl.interp(col_tr, d.x);
        const color b = col_bl.interp(col_br, d.x);

        vtx.col = a.interp(b, d.y).alpha((vtx.col >> (3u * 8u)) & 0xFFu);
    }
}

inline void renderer2d::path_clear()
{
    path_.clear();
}

inline void renderer2d::path_add_point(const vec2& p)
{
    path_.emplace_back(p);
}

template <int a_min_of_12, int a_max_of_12>
inline void renderer2d::path_arc_to(const vec2& center, float radius, float step)
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

inline void renderer2d::path_rect(const vec2& min, const vec2& max, float rounding, e_rounding_flags flags, float corner_step)
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

inline void renderer2d::path_fill_convex(color_u32 col)
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

inline void renderer2d::path_stroke(color_u32 col, float line_width, bool closed)
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

template <unicode::string_like String>
inline void renderer2d::add_text(const vec2& pos, color_u32 col, const String& text, bool blurred)
{
    assert(current_font_ != nullptr);

    if ((col & ~color::alpha_mask) == 0u) [[unlikely]]
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
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 1u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 3u);

                vertices_.emplace_back(vec2{ x0, y0 }, glyph->uv_min, col);
                vertices_.emplace_back(vec2{ x0, y1 }, vec2{ glyph->uv_min.x, glyph->uv_max.y }, col);
                vertices_.emplace_back(vec2{ x1, y1 }, glyph->uv_max, col);
                vertices_.emplace_back(vec2{ x1, y0 }, vec2{ glyph->uv_max.x, glyph->uv_min.y }, col);

                vertex_ptr_ += 4u;
            }
        }

        x += glyph->advance_x;
    }
}

template<unicode::string_like String>
inline void renderer2d::add_text_faded(const vec2& pos, color_u32 col, color_u32 faded_col,
                                       float fade_start, float fade_end, const String& text, bool blurred)
{
    if (col == faded_col) [[unlikely]]
        return add_text<String>(pos, col, text, blurred);

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
        !draw_no_fade) [[unlikely]]
        return;

    const float denom = (fade_end - fade_start);
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
                                indices_.emplace_back(vertex_ptr_ + 0u);
                                indices_.emplace_back(vertex_ptr_ + 1u);
                                indices_.emplace_back(vertex_ptr_ + 2u);
                                indices_.emplace_back(vertex_ptr_ + 0u);
                                indices_.emplace_back(vertex_ptr_ + 2u);
                                indices_.emplace_back(vertex_ptr_ + 3u);

                                vertices_.emplace_back(vec2{ x0, y0 }, vec2{ uv_min.x, uv_min.y }, col);
                                vertices_.emplace_back(vec2{ x0, y1 }, vec2{ uv_min.x, uv_max.y }, col);
                                vertices_.emplace_back(vec2{ mid_left_pos, y1 }, vec2{ mid_left, uv_max.y }, col);
                                vertices_.emplace_back(vec2{ mid_left_pos, y0 }, vec2{ mid_left, uv_min.y }, col);

                                vertex_ptr_ += 4u;
                            }
                        }

                        if (fade_end < x1) {
                            assert(x0 < fade_end);

                            const float d = (fade_end - x0) / (x1 - x0);
                            mid_right = uv_min.x + (uv_max.x - uv_min.x) * d;
                            mid_right_pos = x0 + (x1 - x0) * d;
                            if (draw_if_faded) {
                                indices_.emplace_back(vertex_ptr_ + 0u);
                                indices_.emplace_back(vertex_ptr_ + 1u);
                                indices_.emplace_back(vertex_ptr_ + 2u);
                                indices_.emplace_back(vertex_ptr_ + 0u);
                                indices_.emplace_back(vertex_ptr_ + 2u);
                                indices_.emplace_back(vertex_ptr_ + 3u);

                                vertices_.emplace_back(vec2{ mid_right_pos, y0 }, vec2{ mid_right, uv_min.y }, faded_col);
                                vertices_.emplace_back(vec2{ mid_right_pos, y1 }, vec2{ mid_right, uv_max.y }, faded_col);
                                vertices_.emplace_back(vec2{ x1, y1 }, vec2{ uv_max.x, uv_max.y }, faded_col);
                                vertices_.emplace_back(vec2{ x1, y0 }, vec2{ uv_max.x, uv_min.y }, faded_col);

                                vertex_ptr_ += 4u;
                            }
                        }

                        const float t_left = (mid_left_pos - fade_start) * inv_denom;
                        c_left = color(col).interp(color(faded_col), std::clamp(t_left, 0.f, 1.f));

                        const float t_right = (mid_right_pos - fade_start) * inv_denom;
                        c_right = color(col).interp(color(faded_col), std::clamp(t_right, 0.f, 1.f));
                    }
                }

                // middle/main quad
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 1u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                indices_.emplace_back(vertex_ptr_ + 3u);

                vertices_.emplace_back(vec2{ mid_left_pos,  y0 }, vec2{ mid_left,  uv_min.y }, c_left);
                vertices_.emplace_back(vec2{ mid_left_pos,  y1 }, vec2{ mid_left,  uv_max.y }, c_left);
                vertices_.emplace_back(vec2{ mid_right_pos, y1 }, vec2{ mid_right, uv_max.y }, c_right);
                vertices_.emplace_back(vec2{ mid_right_pos, y0 }, vec2{ mid_right, uv_min.y }, c_right);

                vertex_ptr_ += 4u;
            }
        }

        x += glyph->advance_x;
    }
}

template<unicode::string_like String, std::integral T>
inline float renderer2d::get_text_width(const String& text, T offset, std::optional<T> count)
{
    std::uint32_t length = static_cast<std::uint32_t>(text.length());
    if (count.has_value() &&
        static_cast<std::uint32_t>(*count) < length) {
        length = static_cast<std::uint32_t>(*count);
    }

    float ret = 0.f;
    std::uint32_t s = offset;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            assert(cp != U'\n');
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph(cp);
        if (glyph == nullptr)
            continue;

        ret += glyph->advance_x;
    }

    return ret;
}

template<unicode::string_like String, std::integral T>
inline vec2 renderer2d::get_text_size(const String& text, T offset, std::optional<T> count)
{
    std::uint32_t length = static_cast<std::uint32_t>(text.length());
    if (count.has_value() &&
        static_cast<std::uint32_t>(*count) < length) {
        length = static_cast<std::uint32_t>(*count);
    }
    const float line_height = static_cast<float>(current_font_->cfg().size);

    vec2 ret;
    std::uint32_t s = offset;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            if (cp == U'\n') {
                ret.y += line_height;
                continue;
            }
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph(cp);
        if (glyph == nullptr)
            continue;

        ret.x = (std::max)(ret.x, glyph->advance_x);
    }

    return ret;
}

template<unicode::string_like String, std::integral T>
inline bool renderer2d::get_text_width_strict(const String& text, float& out, T offset, std::optional<T> count)
{
    std::uint32_t length = static_cast<std::uint32_t>(text.length());
    if (count.has_value() &&
        static_cast<std::uint32_t>(*count) < length) {
        length = static_cast<std::uint32_t>(*count);
    }

    out = 0.f;
    std::uint32_t s = offset;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            assert(cp != U'\n');
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph_no_fallback(cp);
        if (glyph == nullptr)
            return false;

        out += glyph->advance_x;
    }

    return true;
}

template<unicode::string_like String, std::integral T>
inline bool renderer2d::get_text_size_strict(const String& text, vec2& out, T offset, std::optional<T> count)
{
    std::uint32_t length = static_cast<std::uint32_t>(text.length());
    if (count.has_value() &&
        static_cast<std::uint32_t>(*count) < length) {
        length = static_cast<std::uint32_t>(*count);
    }
    const float line_height = static_cast<float>(current_font_->cfg().size);

    out = vec2(0.f);
    std::uint32_t s = offset;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            if (cp == U'\n') {
                out.y += line_height;
                continue;
            }
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph_no_fallback(cp);
        if (glyph == nullptr)
            return false;

        out.x = (std::max)(out.x, glyph->advance_x);
    }

    return true;
}

template<bool center, unicode::string_like String>
inline std::uint32_t renderer2d::get_char_at_pos(const String& text, float pos)
{
    if (pos <= 0.f)
        return 0u;

    const std::uint32_t length = static_cast<std::uint32_t>(text.length());

    std::uint32_t s = 0u;
    float x = 0.f;
    float prev_width = 0.f;
    while (s < length) {
        const std::uint32_t start = s;
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            assert(cp != U'\n');
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph_no_fallback(cp);
        if (glyph == nullptr)
            continue;

        x += glyph->advance_x;
        if constexpr (center) {
            const float curr_center = x - glyph->advance_x * 0.5f;
            const float prev_center = (x - prev_width * 0.5f - glyph->advance_x);
            if (pos >= prev_center && pos < curr_center) {
                return start;
            }
        }
        else {
            if (pos >= x &&
                pos < x + glyph->advance_x) {
                return start;
            }
        }
        prev_width = glyph->advance_x;
    }

    return length;
}

template<bool center, unicode::string_like String>
inline bool renderer2d::get_char_at_pos_strict(const String& text, float pos, std::uint32_t& index)
{
    if (pos <= 0.f) {
        index = 0u;
        return true;
    }

    const std::uint32_t length = static_cast<std::uint32_t>(text.length());

    std::uint32_t s = 0u;
    float x = 0.f;
    float prev_width = 0.f;
    while (s < length) {
        const std::uint32_t start = s;
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            assert(cp != U'\n');
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph_no_fallback(cp);
        if (glyph == nullptr)
            return false;

        if constexpr (center) {
            const float curr_center = x - glyph->advance_x * 0.5f;
            const float prev_center = (x - prev_width * 0.5f - glyph->advance_x);
            if (pos >= prev_center && pos < curr_center) {
                index = start;
                return true;
            }
        }
        else {
            if (pos >= x &&
                pos < x + glyph->advance_x) {
                index = start;
                return true;
            }
        }
        x += glyph->advance_x;
        prev_width = glyph->advance_x;
    }

    index = length;

    return true;
}

r2_end_