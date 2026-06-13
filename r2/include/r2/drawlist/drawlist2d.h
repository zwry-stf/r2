#pragma once
#include "drawlist_base.h"
#include <concepts>


r2_begin_

class drawlist2d : public drawlist_base {
public:
    using drawlist_base::drawlist_base;

public:
    void reset_states();

public:
    /// render
    void prim_rect(const vec2& min, const vec2& max, color_u32 col);
    void add_rect(const vec2& min, const vec2& max, color_u32 col, 
                  float line_width, float rounding = 0.f,
                  e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void add_rect_inner(const vec2& min, const vec2& max, color_u32 col,
                        float line_width, float rounding = 0.f,
                        e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void add_rect_inner_fast(const vec2& min, const vec2& max, color_u32 col, float line_width);
    void add_rect_filled(const vec2& min, const vec2& max, color_u32 col, float rounding = 0.f,
                         e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void add_rect_filled_multicolor(const vec2& min, const vec2& max,
                                    color_u32 col_tl, color_u32 col_tr, color_u32 col_br, color_u32 col_bl);
    void add_rect_filled_faded(const vec2& min, const vec2& max, color_u32 col, color_u32 faded_col, 
                               float fade_start, float fade_end);
    void add_shadow_rect_filled(const vec2& min, const vec2& max, color_u32 col, float rounding = 0.f, float shadow_size = 50.f,
                                e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void add_quad_filled(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4, color_u32 col);
    void add_quad_filled_multicolor(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4, 
                                    color_u32 col1, color_u32 col2, color_u32 col3, color_u32 col4);
    void add_line(const vec2& start, const vec2& end, color_u32 col, float line_width);
    void add_line_multicolor(const vec2& start, const vec2& end, color_u32 col_start, 
                             color_u32 col_end, float line_width);
    void add_line(const point_3d& start, const point_3d& end, color_u32 col, float line_width);
    void add_line_multicolor(const point_3d& start, const point_3d& end, color_u32 col_start,
                             color_u32 col_end, float line_width);

    /// primitives
    void add_convex_filled(const vec2* points, std::uint32_t num_points, color_u32 col);
    template <std::integral T>
    void add_convex_filled(const vec2* points, T num_points, color_u32 col);
    void add_shadow_convex(const vec2* points, std::uint32_t num_points, color_u32 col,
                           float shadow_size, bool filled = true);
    template <std::integral T>
    void add_shadow_convex(const vec2* points, T num_points, color_u32 col,
                           float shadow_size, bool filled = true);
    void add_lines(const vec2* points, std::uint32_t num_points, color_u32 col,
                   float line_width, bool closed = false);
    template <std::integral T>
    void add_lines(const vec2* points, T num_points, color_u32 col,
                   float line_width, bool closed = false);

    /// primitives 3d
    void add_convex_filled(const point_3d* points, std::uint32_t num_points, color_u32 col);
    template <std::integral T>
    void add_convex_filled(const point_3d* points, T num_points, color_u32 col);
    void add_shadow_convex(const point_3d* points, std::uint32_t num_points, color_u32 col,
                           float shadow_size, bool filled = true);
    template <std::integral T>
    void add_shadow_convex(const point_3d* points, T num_points, color_u32 col,
                           float shadow_size, bool filled = true);
    void add_lines(const point_3d* points, std::uint32_t num_points, color_u32 col,
                   float line_width, bool closed = false);
    template <std::integral T>
    void add_lines(const point_3d* points, T num_points, color_u32 col,
                   float line_width, bool closed = false);
    
    /// images
    void add_image(texture_handle texture, const vec2& min, const vec2& max, color_u32 col = color::white(),
                   const vec2& uv_min = vec2(0.f), const vec2& uv_max = vec2(1.f));
    void add_image_outline(texture_handle texture, const vec2& min, const vec2& max, color_u32 col = color::white(),
                           color_u32 outline_col = color::black(), float outline_size = 1.f,
                           const vec2& uv_min = vec2(0.f), const vec2& uv_max = vec2(1.f));
    void add_image_rounded(texture_handle texture, const vec2& min, const vec2& max, float rounding,
                           color_u32 col = color::white(),
                           const vec2& uv_min = vec2(0.f), const vec2& uv_max = vec2(1.f));

    /// vertex shading
    void shade_vertices_uv(std::uint32_t vtx_start, std::uint32_t vtx_end, const vec2& min, const vec2& max, 
                           const vec2& uv_min, const vec2& uv_max);
    void shade_vertices_col(std::uint32_t vtx_start, std::uint32_t vtx_end, const vec2& min, const vec2& max, 
                            const color& col_tl, const color& col_tr, const color& col_br, const color& col_bl);
    void shade_vertices_depth(std::uint32_t vtx_start, std::uint32_t vtx_end, const vec2& min, const vec2& max, 
                              float depth_tl, float depth_tr, float depth_br, float depth_bl);
    void shade_vertices_depth(std::uint32_t vtx_start, std::uint32_t vtx_end, float depth);

    /// path
    void path_clear();
    void path_clear3d();
    void path_add_point(const vec2& p);
    void path_add_point(const vec2& p, float depth);
    void path_add_point(const point_3d& p);
    template <int a_min_of_12, int a_max_of_12>
    void path_arc_to(const vec2& center, float radius, float step);
    void path_rect(const vec2& min, const vec2& max, float rounding,
                   e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void path_fill_convex(color_u32 col);
    void path_stroke(color_u32 col, float line_width, bool closed = false);
    
    /// text
    template <float CharOffset = 0.f, unicode::string_like String>
    void add_text(const vec2& pos, color_u32 col, const String& text, bool blurred = false);
    template <float CharOffset = 0.f, unicode::string_like String>
    void add_text_faded(const vec2& pos, color_u32 col, color_u32 faded_col, 
                        float fade_start, float fade_end, const String& text, bool blurred = false);
    template <float CharOffset = 0.f, unicode::string_like String>
    void add_text_outlined(const vec2& pos, color_u32 col, const String& text, 
                           const color_u32 outline_col = r2::color::black(), float outline_width = 1.f, bool blurred = false);
};

r2_end_

#include "drawlist2d.inline.inl"