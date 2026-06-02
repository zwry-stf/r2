#include <r2/drawlist/drawlist2d.h>
#include <r2/renderer.h>
#include <r2/font/font_atlas.h>
#include <r2/render_data.h>

#include <algorithm>


r2_begin_

void drawlist2d::reset_states()
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif
    assert(renderer_->render_data_->font_view && "fonts not build");
    assert(!renderer_->fonts_.empty() && "no fonts added");

    vertices_.clear();
    indices_.clear();
    cmds_.clear();
    clip_rect_stack_.clear();
    texture_stack_.clear();
    font_stack_.clear();

    add_draw_cmd();
    push_clip_rect(
        rect(
            0, 0,
            static_cast<std::int32_t>(renderer_->display_size_.x),
            static_cast<std::int32_t>(renderer_->display_size_.y)
        ),
        false
    );
    push_texture_id(
        renderer_->render_data_->font_view.get()
    );
    {
        std::lock_guard<std::mutex> lock(renderer_->font_mutex_);
        push_font(renderer_->fonts_.front());
    }
}


/// primitives

void drawlist2d::add_convex_filled(const vec2* points, std::uint32_t num_points, color_u32 col)
{
    if (num_points < 3u ||
        (col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    const float aa_scale = renderer_->aa_scale_;
    const vec2 uv = shared_data_->uv_white_px;

    if (renderer_->flags().anti_aliased_fill) {
        const std::uint32_t vtx_inner_idx = vertex_ptr_;
        const std::uint32_t vtx_outer_idx = vertex_ptr_ + 1u;
        for (std::uint32_t i = 2u; i < num_points; i++) {
            indices_.emplace_back(vtx_inner_idx);
            indices_.emplace_back(vtx_inner_idx + ((i - 1u) << 1u));
            indices_.emplace_back(vtx_inner_idx + (i << 1u));
        }

        shared_data_->temp_buffer.clear();
        shared_data_->temp_buffer.resize(num_points);
        for (std::uint32_t i0 = num_points - 1u, i1 = 0u; i1 < num_points; i0 = i1++) {
            const vec2& p0 = points[i0];
            const vec2& p1 = points[i1];
            vec2 d = (p0 - p1).normalize();
            shared_data_->temp_buffer[i0].x = d.y;
            shared_data_->temp_buffer[i0].y = -d.x;
        }

        const color_u32 col_no_alpha = col & ~color::alpha_mask;
        for (std::uint32_t i0 = num_points - 1u, i1 = 0u; i1 < num_points; i0 = i1++) {
            const vec2& n0 = shared_data_->temp_buffer[i0];
            const vec2& n1 = shared_data_->temp_buffer[i1];

            vec2 dm = ((n0 + n1) * vec2(0.5f)).normalize(100.f);

            dm.x *= aa_scale * 0.5f;
            dm.y *= aa_scale * 0.5f;

            vertices_.emplace_back(
                vec2{ points[i1].x - dm.x, points[i1].y - dm.y },
                uv,
                col
            );

            vertices_.emplace_back(
                vec2{ points[i1].x + dm.x, points[i1].y + dm.y },
                uv,
                col_no_alpha
            );

            vertex_ptr_ += 2u;

            indices_.emplace_back(vtx_inner_idx + (i1 << 1u));
            indices_.emplace_back(vtx_inner_idx + (i0 << 1u));
            indices_.emplace_back(vtx_outer_idx + (i0 << 1u));
            indices_.emplace_back(vtx_outer_idx + (i0 << 1u));
            indices_.emplace_back(vtx_outer_idx + (i1 << 1u));
            indices_.emplace_back(vtx_inner_idx + (i1 << 1u));
        }
    }
    else {
        for (std::uint32_t i = 0u; i < num_points; i++) {
            vertices_.emplace_back(
                points[i],
                uv,
                col
            );
        }
        for (std::uint32_t i = 2u; i < num_points; i++) {
            indices_.emplace_back(vertex_ptr_);
            indices_.emplace_back(vertex_ptr_ + i - 1u);
            indices_.emplace_back(vertex_ptr_ + i);
        }
        vertex_ptr_ += num_points;
    }
}

void drawlist2d::add_shadow_convex(const vec2* points, std::uint32_t num_points, color_u32 col, float shadow_size, bool filled)
{
    if (num_points < 3u ||
        (col & color::alpha_mask) == 0u)
        return;

    const int vertex_winding = (
        ((points[0].x * (points[1].y - points[2].y)) + 
            (points[1].x * (points[2].y - points[0].y)) + 
            (points[2].x * (points[0].y - points[1].y))) < 0.0f) ? -1 : 1;
    const bool use_inset_distance = renderer_->flags().anti_aliased_fill && !filled;
    const vec2 inset_distance = vec2(0.5f);

    const vec4 shadow_uvs = shared_data_->shadow_uvs;

    const vec2 tex_size = vec2(
        static_cast<float>(renderer_->font_atlas_->get_width()),
        static_cast<float>(renderer_->font_atlas_->get_height())
    );
    const vec2 inv_tex_size = vec2(1.f) / tex_size;

    const vec2 uv_min_in = vec2(shadow_uvs.x, shadow_uvs.y);
    const vec2 uv_max_in = vec2(shadow_uvs.z, shadow_uvs.w);

    const vec2 solid_uv = uv_max_in;
    const vec2 edge_uv = vec2(uv_min_in.x, uv_max_in.y);

    const vec2 solid_to_edge_delta_texels = (edge_uv - solid_uv) * tex_size;

    const std::uint32_t num_edges = num_points;

    shared_data_->temp_buffer.resize(num_edges);
    shared_data_->temp_buffer2.resize(num_edges);
    auto* edge_size_scales = shared_data_->temp_buffer2.data();
    vec2* edge_normals = shared_data_->temp_buffer.data();

    for (std::uint32_t edge_index = 0u; edge_index < num_edges; edge_index++) {
        vec2 edge_start = points[edge_index];
        vec2 edge_end = points[(edge_index + 1) % num_edges];
        vec2 edge_normal = vec2(edge_end.y - edge_start.y, -(edge_end.x - edge_start.x)).normalize();
        edge_normals[edge_index] = edge_normal * vec2(static_cast<float>(vertex_winding));
    }

    {
        vec2 prev_edge_normal = edge_normals[num_edges - 1u];
        for (std::uint32_t edge_index = 0u; edge_index < num_edges; edge_index++) {
            vec2 edge_normal = edge_normals[edge_index];
            float cos_angle_coverage = edge_normal.dot(prev_edge_normal);

            if (cos_angle_coverage < 0.999999f) {
                float angle_coverage = std::acos(cos_angle_coverage);
                if (cos_angle_coverage <= 0.f)
                    angle_coverage *= 0.5f;
                edge_size_scales[edge_index] = 1.f / std::cos(angle_coverage * 0.5f);
            }
            else {
                edge_size_scales[edge_index] = 1.f;
            }

            prev_edge_normal = edge_normal;
        }
    }

    vec2 prev_edge_normal = edge_normals[num_edges - 1u];
    vec2 edge_start = points[0];

    if (use_inset_distance)
        edge_start -= (edge_normals[0] + prev_edge_normal).normalize() * inset_distance;

    for (std::uint32_t edge_index = 0u; edge_index < num_edges; edge_index++) {
        vec2 edge_end = points[(edge_index + 1u) % num_edges];
        vec2 edge_normal = edge_normals[edge_index];
        const float size_scale_start = edge_size_scales[edge_index];
        const float size_scale_end = edge_size_scales[(edge_index + 1) % num_edges];

        if (use_inset_distance)
            edge_end -= (edge_normals[(edge_index + 1u) % num_edges] + 
                edge_normal).normalize() * inset_distance;

        float cos_angle_coverage = edge_normal.dot(prev_edge_normal);
        if (cos_angle_coverage < 0.999999f) {
            std::uint32_t num_steps = (cos_angle_coverage <= 0.0f) ? 2u : 1u;

            for (std::uint32_t step = 0u; step < num_steps; step++) {
                if (num_steps > 1u) {
                    if (step == 0u)
                        edge_normal = (edge_normal + prev_edge_normal).normalize();
                    else
                        edge_normal = edge_normals[edge_index];

                    cos_angle_coverage = edge_normal.dot(prev_edge_normal);
                }

                const float angle_coverage = std::acos(cos_angle_coverage);
                const float sin_angle_coverage = std::sin(angle_coverage);

                const vec2 edge_delta = solid_to_edge_delta_texels * vec2(size_scale_start);

                const vec2 rotated_edge_delta = vec2(
                    (edge_delta.x * cos_angle_coverage) +
                        (edge_delta.y * sin_angle_coverage), 
                    (edge_delta.x * sin_angle_coverage) +
                        (edge_delta.y * cos_angle_coverage)
                );

                const vec2 edge_delta_uv = edge_delta * inv_tex_size;
                const vec2 rotated_edge_delta_uv = rotated_edge_delta * inv_tex_size;

                const vec2 expanded_edge_uv = solid_uv + edge_delta_uv;
                const vec2 other_edge_uv = solid_uv + rotated_edge_delta_uv;

                const vec2 expanded_thickness = vec2(shadow_size * size_scale_start);

                const vec2 outer_edge_start = edge_start + (prev_edge_normal * expanded_thickness);
                const vec2 outer_edge_end = edge_start + (edge_normal * expanded_thickness);

                vertices_.emplace_back(edge_start, solid_uv, col);
                vertices_.emplace_back(outer_edge_end, expanded_edge_uv, col);
                vertices_.emplace_back(outer_edge_start, other_edge_uv, col);

                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 1u);
                indices_.emplace_back(vertex_ptr_ + 2u);

                vertex_ptr_ += 3u;

                prev_edge_normal = edge_normal;
            }
        }

        const float edge_length = (edge_end - edge_start).length();
        if (edge_length > 0.00001f) {
            const vec2 outer_edge_start = edge_start + (edge_normal * vec2(shadow_size * size_scale_start));
            const vec2 outer_edge_end = edge_end + (edge_normal * vec2(shadow_size * size_scale_end));
            const vec2 scaled_edge_uv_start = solid_uv + ((edge_uv - solid_uv) * vec2(size_scale_start));
            const vec2 scaled_edge_uv_end = solid_uv + ((edge_uv - solid_uv) * vec2(size_scale_end));

            vertices_.emplace_back(edge_start, solid_uv, col);
            vertices_.emplace_back(edge_end, solid_uv, col);
            vertices_.emplace_back(outer_edge_end, scaled_edge_uv_end, col);
            vertices_.emplace_back(outer_edge_start, scaled_edge_uv_start, col);

            indices_.emplace_back(vertex_ptr_ + 0u);
            indices_.emplace_back(vertex_ptr_ + 1u);
            indices_.emplace_back(vertex_ptr_ + 2u);
            indices_.emplace_back(vertex_ptr_ + 0u);
            indices_.emplace_back(vertex_ptr_ + 2u);
            indices_.emplace_back(vertex_ptr_ + 3u);

            vertex_ptr_ += 4u;
        }

        edge_start = edge_end;
    }

    [[likely]] if (filled) {
        for (std::uint32_t i = 0u; i < num_points; i++) {
            vertices_.emplace_back(
                points[i],
                solid_uv,
                col
            );
        }
        for (std::uint32_t i = 2u; i < num_points; i++) {
            indices_.emplace_back(vertex_ptr_);
            indices_.emplace_back(vertex_ptr_ + i - 1u);
            indices_.emplace_back(vertex_ptr_ + i);
        }
        vertex_ptr_ += num_points;
    }
}

void drawlist2d::add_lines(const vec2* points, std::uint32_t num_points, color_u32 col, float line_width, bool closed)
{
    if (num_points < 2u ||
        (col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    assert(line_width >= 0.f && "line_width should not be negative");

    const float aa_scale = renderer_->aa_scale_;
    const vec2 opaque_uv = shared_data_->uv_white_px;
    const std::uint32_t count = closed ? num_points : num_points - 1u;
    const bool thick_line = (line_width > aa_scale);

    [[likely]] if (renderer_->flags().anti_aliased_lines) {
        const color_u32 col_no_alpha = col & ~color::alpha_mask;

        line_width = (std::max)(line_width, 1.0f);
        const int integer_line_width = static_cast<int>(line_width);
        const float fractional_line_width = line_width - integer_line_width;

        const bool use_texture = (renderer_->flags().anti_aliased_lines_use_tex) &&
            (integer_line_width < shared_data::kBakedLinesMaxWidth) &&
            (fractional_line_width <= 0.00001f) && (aa_scale == 1.0f);

        shared_data_->temp_buffer.clear();
        shared_data_->temp_buffer.reserve(num_points * ((use_texture || !thick_line) ? 3u : 5u));
        vec2* temp_normals = shared_data_->temp_buffer.data();
        vec2* temp_points = temp_normals + num_points;

        for (std::uint32_t i1 = 0u; i1 < count; i1++) {
            const std::uint32_t i2 = (i1 + 1u) == static_cast<std::int32_t>(num_points) ? 0u : i1 + 1u;
            vec2 d(points[i2].x - points[i1].x,
                   points[i2].y - points[i1].y
            );
            d = d.normalize();

            temp_normals[i1].x = d.y;
            temp_normals[i1].y = -d.x;
        }
        if (!closed) {
            temp_normals[num_points - 1u] = temp_normals[num_points - 2u];
        }

        if (use_texture || !thick_line) {
            const float half_draw_size = use_texture ? 
                ((line_width * 0.5f) + 1u) : aa_scale;

            if (!closed) {
                temp_points[0] = points[0] + temp_normals[0] * vec2(half_draw_size);
                temp_points[1] = points[0] - temp_normals[0] * vec2(half_draw_size);
                temp_points[(num_points - 1u) * 2u + 0u] =
                    points[num_points - 1u] + temp_normals[num_points - 1u] * vec2(half_draw_size);
                temp_points[(num_points - 1u) * 2u + 1u] = 
                    points[num_points - 1u] - temp_normals[num_points - 1u] * vec2(half_draw_size);
            }

            std::uint32_t idx1 = vertex_ptr_;
            for (std::uint32_t i1 = 0u; i1 < count; i1++)
            {
                const std::uint32_t i2 = (i1 + 1u) == num_points ? 0u : i1 + 1u;
                const std::uint32_t idx2 = ((i1 + 1u) == num_points) ? 
                    vertex_ptr_ : (idx1 + (use_texture ? 2u : 3u));

                vec2 dm(
                    (temp_normals[i1].x + temp_normals[i2].x) * 0.5f,
                    (temp_normals[i1].y + temp_normals[i2].y) * 0.5f
                );
                dm = dm.normalize(100.f);
                dm *= vec2(half_draw_size);

                vec2* out_vtx = &temp_points[i2 * 2u];
                out_vtx[0].x = points[i2].x + dm.x;
                out_vtx[0].y = points[i2].y + dm.y;
                out_vtx[1].x = points[i2].x - dm.x;
                out_vtx[1].y = points[i2].y - dm.y;

                if (use_texture) {
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx1 + 0u);
                    indices_.emplace_back(idx1 + 1u);
                    indices_.emplace_back(idx2 + 1u);
                    indices_.emplace_back(idx1 + 1u);
                    indices_.emplace_back(idx2 + 0u);
                }
                else {
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx1 + 2u);
                    indices_.emplace_back(idx1 + 2u);
                    indices_.emplace_back(idx2 + 2u);
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx2 + 1u);
                    indices_.emplace_back(idx1 + 1u);
                    indices_.emplace_back(idx1 + 0u);
                    indices_.emplace_back(idx1 + 0u);
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx2 + 1u);
                }

                idx1 = idx2;
            }

            if (use_texture) {
                vec4 tex_uvs = shared_data_->tex_uv_lines[integer_line_width];
                const vec2 tex_uv0(tex_uvs.x, tex_uvs.y);
                const vec2 tex_uv1(tex_uvs.z, tex_uvs.w);
                for (std::uint32_t i = 0u; i < num_points; i++) {
                    vertices_.emplace_back(temp_points[i * 2u + 0u], tex_uv0, col);
                    vertices_.emplace_back(temp_points[i * 2u + 1u], tex_uv1, col);

                    vertex_ptr_ += 2u;
                }
            }
            else {
                for (std::uint32_t i = 0u; i < num_points; i++) {
                    vertices_.emplace_back(points[i], opaque_uv, col);
                    vertices_.emplace_back(temp_points[i * 2u + 0u], opaque_uv, col_no_alpha);
                    vertices_.emplace_back(temp_points[i * 2u + 1u], opaque_uv, col_no_alpha);

                    vertex_ptr_ += 3u;
                }
            }
        }
        else {
            const float half_inner_line_width = (line_width - aa_scale) * 0.5f;

            if (!closed) {
                const std::uint32_t points_last = num_points - 1u;
                temp_points[0] = points[0] + temp_normals[0] * vec2(half_inner_line_width + aa_scale);
                temp_points[1] = points[0] + temp_normals[0] * vec2(half_inner_line_width);
                temp_points[2] = points[0] - temp_normals[0] * vec2(half_inner_line_width);
                temp_points[3] = points[0] - temp_normals[0] * vec2(half_inner_line_width + aa_scale);
                temp_points[points_last * 4u + 0u] = points[points_last] + temp_normals[points_last] * vec2(half_inner_line_width + aa_scale);
                temp_points[points_last * 4u + 1u] = points[points_last] + temp_normals[points_last] * vec2(half_inner_line_width);
                temp_points[points_last * 4u + 2u] = points[points_last] - temp_normals[points_last] * vec2(half_inner_line_width);
                temp_points[points_last * 4u + 3u] = points[points_last] - temp_normals[points_last] * vec2(half_inner_line_width + aa_scale);
            }

            std::uint32_t idx1 = vertex_ptr_;
            for (std::uint32_t i1 = 0u; i1 < count; i1++) {
                const std::uint32_t i2 = (i1 + 1u) == num_points ? 0u : (i1 + 1u);
                const std::uint32_t idx2 = (i1 + 1u) == num_points ? vertex_ptr_ : (idx1 + 4u);

                vec2 dm(
                    (temp_normals[i1].x + temp_normals[i2].x) * 0.5f,
                    (temp_normals[i1].y + temp_normals[i2].y) * 0.5f
                );
                dm = dm.normalize(100.f);

                vec2 dm_out = dm * vec2(half_inner_line_width + aa_scale);
                vec2 dm_in = dm * vec2(half_inner_line_width);

                vec2* out_vtx = &temp_points[i2 * 4];
                out_vtx[0].x = points[i2].x + dm_out.x;
                out_vtx[0].y = points[i2].y + dm_out.y;
                out_vtx[1].x = points[i2].x + dm_in.x;
                out_vtx[1].y = points[i2].y + dm_in.y;
                out_vtx[2].x = points[i2].x - dm_in.x;
                out_vtx[2].y = points[i2].y - dm_in.y;
                out_vtx[3].x = points[i2].x - dm_out.x;
                out_vtx[3].y = points[i2].y - dm_out.y;

                indices_.emplace_back(idx2 + 1u);
                indices_.emplace_back(idx1 + 1u);
                indices_.emplace_back(idx1 + 2u);

                indices_.emplace_back(idx1 + 2u);
                indices_.emplace_back(idx2 + 2u);
                indices_.emplace_back(idx2 + 1u);

                indices_.emplace_back(idx2 + 1u);
                indices_.emplace_back(idx1 + 1u);
                indices_.emplace_back(idx1 + 0u);

                indices_.emplace_back(idx1 + 0u);
                indices_.emplace_back(idx2 + 0u);
                indices_.emplace_back(idx2 + 1u);

                indices_.emplace_back(idx2 + 2u);
                indices_.emplace_back(idx1 + 2u);
                indices_.emplace_back(idx1 + 3u);

                indices_.emplace_back(idx1 + 3u);
                indices_.emplace_back(idx2 + 3u);
                indices_.emplace_back(idx2 + 2u);

                idx1 = idx2;
            }

            for (std::uint32_t i = 0; i < num_points; i++) {
                vertices_.emplace_back(temp_points[i * 4u + 0u], opaque_uv, col_no_alpha);
                vertices_.emplace_back(temp_points[i * 4u + 1u], opaque_uv, col);
                vertices_.emplace_back(temp_points[i * 4u + 2u], opaque_uv, col);
                vertices_.emplace_back(temp_points[i * 4u + 3u], opaque_uv, col_no_alpha);

                vertex_ptr_ += 4u;
            }
        }
    }
    else {
        for (std::uint32_t i1 = 0u; i1 < count; i1++) {
            const std::uint32_t i2 = (i1 + 1u) == num_points ? 0u : i1 + 1u;
            const vec2& p1 = points[i1];
            const vec2& p2 = points[i2];

            vec2 d(
                p2.x - p1.x,
                p2.y - p1.y
            );
            d = d.normalize();
            d *= vec2(line_width * 0.5f);

            vertices_.emplace_back(vec2{ p1.x + d.y, p1.y - d.x }, opaque_uv, col);
            vertices_.emplace_back(vec2{ p2.x + d.y, p2.y - d.x }, opaque_uv, col);
            vertices_.emplace_back(vec2{ p2.x - d.y, p2.y + d.x }, opaque_uv, col);
            vertices_.emplace_back(vec2{ p1.x - d.y, p1.y + d.x }, opaque_uv, col);

            indices_.emplace_back(vertex_ptr_ + 0u);
            indices_.emplace_back(vertex_ptr_ + 1u);
            indices_.emplace_back(vertex_ptr_ + 2u);
            indices_.emplace_back(vertex_ptr_ + 0u);
            indices_.emplace_back(vertex_ptr_ + 2u);
            indices_.emplace_back(vertex_ptr_ + 3u);

            vertex_ptr_ += 4u;
        }
    }
}

void drawlist2d::add_line_multicolor(const vec2& start_p, const vec2& end_p, color_u32 col_start, color_u32 col_end, float line_width)
{
    const bool odd = (static_cast<int>(std::round(line_width)) & 1) != 0;
    const float snap = odd ? 0.5f : 0.0f;

    const vec2 start = start_p + vec2(snap);
    const vec2 end = end_p + vec2(snap);

    if ((col_start & color::alpha_mask) == 0u &&
        (col_end & color::alpha_mask) == 0u) [[unlikely]]
        return;

    assert(path_.empty());
    const vec2 d = end - start;
    const float length = d.length();
    if (length < 1e-6f)
        return;

    const float aa_scale = renderer_->aa_scale_;
    const vec2 dir = d * vec2(1.f / length);
    const vec2 n = dir.perp() * vec2(line_width * 0.5f);
    const vec2 opaque_uv = shared_data_->uv_white_px;
    const color_u32 col_no_alpha_start = col_start & ~color::alpha_mask;
    const color_u32 col_no_alpha_end = col_end & ~color::alpha_mask;
    if (!renderer_->flags().anti_aliased_lines) [[unlikely]] {
        indices_.push_back(vertex_ptr_ + 0u);
        indices_.push_back(vertex_ptr_ + 1u);
        indices_.push_back(vertex_ptr_ + 2u);
        indices_.push_back(vertex_ptr_ + 0u);
        indices_.push_back(vertex_ptr_ + 2u);
        indices_.push_back(vertex_ptr_ + 3u);
        vertices_.emplace_back(start - n, opaque_uv, col_start);
        vertices_.emplace_back(start + n, opaque_uv, col_start);
        vertices_.emplace_back(end + n, opaque_uv, col_end);
        vertices_.emplace_back(end - n, opaque_uv, col_end);
        vertex_ptr_ += 4u;
        return;
    }
    line_width = (std::max)(line_width, 1.0f);
    const bool thick_line = (line_width > aa_scale);
    const int integer_line_width = static_cast<int>(line_width);
    const float fractional_line_width = line_width - integer_line_width;
    const bool use_texture = (renderer_->flags().anti_aliased_lines_use_tex) &&
        (integer_line_width < shared_data::kBakedLinesMaxWidth) &&
        (fractional_line_width <= 0.00001f) && (aa_scale == 1.0f);
    shared_data_->temp_buffer.clear();
    shared_data_->temp_buffer.reserve(2u * ((use_texture || !thick_line) ? 3u : 5u));
    vec2* temp_normals = shared_data_->temp_buffer.data();
    vec2* temp_points = temp_normals + 2u;
    temp_normals[0].x = dir.y;
    temp_normals[0].y = -dir.x;
    temp_normals[1] = temp_normals[0];
    if (use_texture || !thick_line) {
        const float half_draw_size = use_texture ?
            ((line_width * 0.5f) + 1.f) : aa_scale;
        temp_points[0] = start + temp_normals[0] * vec2(half_draw_size);
        temp_points[1] = start - temp_normals[0] * vec2(half_draw_size);
        temp_points[2] = end + temp_normals[1] * vec2(half_draw_size);
        temp_points[3] = end - temp_normals[1] * vec2(half_draw_size);
        std::uint32_t idx1 = vertex_ptr_;
        std::uint32_t idx2 = idx1 + (use_texture ? 2u : 3u);
        vec2 dm(
            (temp_normals[0].x + temp_normals[1].x) * 0.5f,
            (temp_normals[0].y + temp_normals[1].y) * 0.5f
        );
        dm = dm.normalize(100.f);
        dm *= vec2(half_draw_size);
        temp_points[2] = end + dm;
        temp_points[3] = end - dm;
        if (use_texture) {
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx1 + 0u);
            indices_.emplace_back(idx1 + 1u);
            indices_.emplace_back(idx2 + 1u);
            indices_.emplace_back(idx1 + 1u);
            indices_.emplace_back(idx2 + 0u);
        }
        else {
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx1 + 2u);
            indices_.emplace_back(idx1 + 2u);
            indices_.emplace_back(idx2 + 2u);
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx2 + 1u);
            indices_.emplace_back(idx1 + 1u);
            indices_.emplace_back(idx1 + 0u);
            indices_.emplace_back(idx1 + 0u);
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx2 + 1u);
        }
        if (use_texture) {
            vec4 tex_uvs = shared_data_->tex_uv_lines[integer_line_width];
            const vec2 tex_uv0(tex_uvs.x, tex_uvs.y);
            const vec2 tex_uv1(tex_uvs.z, tex_uvs.w);
            vertices_.emplace_back(temp_points[0], tex_uv0, col_start);
            vertices_.emplace_back(temp_points[1], tex_uv1, col_start);
            vertices_.emplace_back(temp_points[2], tex_uv0, col_end);
            vertices_.emplace_back(temp_points[3], tex_uv1, col_end);
            vertex_ptr_ += 4u;
        }
        else {
            vertices_.emplace_back(start, opaque_uv, col_start);
            vertices_.emplace_back(temp_points[0], opaque_uv, col_no_alpha_start);
            vertices_.emplace_back(temp_points[1], opaque_uv, col_no_alpha_start);
            vertices_.emplace_back(end, opaque_uv, col_end);
            vertices_.emplace_back(temp_points[2], opaque_uv, col_no_alpha_end);
            vertices_.emplace_back(temp_points[3], opaque_uv, col_no_alpha_end);
            vertex_ptr_ += 6u;
        }
    }
    else {
        const float half_inner_line_width = (line_width - aa_scale) * 0.5f;
        temp_points[0] = start + temp_normals[0] * vec2(half_inner_line_width + aa_scale);
        temp_points[1] = start + temp_normals[0] * vec2(half_inner_line_width);
        temp_points[2] = start - temp_normals[0] * vec2(half_inner_line_width);
        temp_points[3] = start - temp_normals[0] * vec2(half_inner_line_width + aa_scale);
        temp_points[4] = end + temp_normals[1] * vec2(half_inner_line_width + aa_scale);
        temp_points[5] = end + temp_normals[1] * vec2(half_inner_line_width);
        temp_points[6] = end - temp_normals[1] * vec2(half_inner_line_width);
        temp_points[7] = end - temp_normals[1] * vec2(half_inner_line_width + aa_scale);
        std::uint32_t idx1 = vertex_ptr_;
        std::uint32_t idx2 = idx1 + 4u;
        vec2 dm(
            (temp_normals[0].x + temp_normals[1].x) * 0.5f,
            (temp_normals[0].y + temp_normals[1].y) * 0.5f
        );
        dm = dm.normalize(100.f);
        vec2 dm_out = dm * vec2(half_inner_line_width + aa_scale);
        vec2 dm_in = dm * vec2(half_inner_line_width);
        temp_points[4] = end + dm_out;
        temp_points[5] = end + dm_in;
        temp_points[6] = end - dm_in;
        temp_points[7] = end - dm_out;

        indices_.emplace_back(idx2 + 1u);
        indices_.emplace_back(idx1 + 1u);
        indices_.emplace_back(idx1 + 2u);
        indices_.emplace_back(idx1 + 2u);
        indices_.emplace_back(idx2 + 2u);
        indices_.emplace_back(idx2 + 1u);
        indices_.emplace_back(idx2 + 1u);
        indices_.emplace_back(idx1 + 1u);
        indices_.emplace_back(idx1 + 0u);
        indices_.emplace_back(idx1 + 0u);
        indices_.emplace_back(idx2 + 0u);
        indices_.emplace_back(idx2 + 1u);
        indices_.emplace_back(idx2 + 2u);
        indices_.emplace_back(idx1 + 2u);
        indices_.emplace_back(idx1 + 3u);
        indices_.emplace_back(idx1 + 3u);
        indices_.emplace_back(idx2 + 3u);
        indices_.emplace_back(idx2 + 2u);

        vertices_.emplace_back(temp_points[0], opaque_uv, col_no_alpha_start);
        vertices_.emplace_back(temp_points[1], opaque_uv, col_start);
        vertices_.emplace_back(temp_points[2], opaque_uv, col_start);
        vertices_.emplace_back(temp_points[3], opaque_uv, col_no_alpha_start);
        vertices_.emplace_back(temp_points[4], opaque_uv, col_no_alpha_end);
        vertices_.emplace_back(temp_points[5], opaque_uv, col_end);
        vertices_.emplace_back(temp_points[6], opaque_uv, col_end);
        vertices_.emplace_back(temp_points[7], opaque_uv, col_no_alpha_end);
        vertex_ptr_ += 8u;
    }
}

/// primitives 3d

void drawlist2d::add_convex_filled(const point_3d* points, std::uint32_t num_points, color_u32 col)
{
    if (num_points < 3u ||
        (col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    const float aa_scale = renderer_->aa_scale_;
    const vec2 uv = shared_data_->uv_white_px;
    if (renderer_->flags().anti_aliased_fill) {
        const std::uint32_t vtx_inner_idx = vertex_ptr_;
        const std::uint32_t vtx_outer_idx = vertex_ptr_ + 1u;
        for (std::uint32_t i = 2u; i < num_points; i++) {
            indices_.emplace_back(vtx_inner_idx);
            indices_.emplace_back(vtx_inner_idx + ((i - 1u) << 1u));
            indices_.emplace_back(vtx_inner_idx + (i << 1u));
        }
        shared_data_->temp_buffer3d.clear();
        shared_data_->temp_buffer3d.resize(num_points);
        for (std::uint32_t i0 = num_points - 1u, i1 = 0u; i1 < num_points; i0 = i1++) {
            const vec2& p0 = points[i0].pos;
            const vec2& p1 = points[i1].pos;
            vec2 d = (p0 - p1).normalize();
            shared_data_->temp_buffer3d[i0].pos.x = d.y;
            shared_data_->temp_buffer3d[i0].pos.y = -d.x;
        }
        const color_u32 col_no_alpha = col & ~color::alpha_mask;
        for (std::uint32_t i0 = num_points - 1u, i1 = 0u; i1 < num_points; i0 = i1++) {
            const vec2& n0 = shared_data_->temp_buffer3d[i0].pos;
            const vec2& n1 = shared_data_->temp_buffer3d[i1].pos;
            vec2 dm = ((n0 + n1) * vec2(0.5f)).normalize(100.f);
            dm.x *= aa_scale * 0.5f;
            dm.y *= aa_scale * 0.5f;
            vertices_.emplace_back(
                vec2{ points[i1].pos.x - dm.x, points[i1].pos.y - dm.y },
                uv,
                col,
                points[i1].depth
            );
            vertices_.emplace_back(
                vec2{ points[i1].pos.x + dm.x, points[i1].pos.y + dm.y },
                uv,
                col_no_alpha,
                points[i1].depth
            );
            vertex_ptr_ += 2u;
            indices_.emplace_back(vtx_inner_idx + (i1 << 1u));
            indices_.emplace_back(vtx_inner_idx + (i0 << 1u));
            indices_.emplace_back(vtx_outer_idx + (i0 << 1u));
            indices_.emplace_back(vtx_outer_idx + (i0 << 1u));
            indices_.emplace_back(vtx_outer_idx + (i1 << 1u));
            indices_.emplace_back(vtx_inner_idx + (i1 << 1u));
        }
    }
    else {
        for (std::uint32_t i = 0u; i < num_points; i++) {
            vertices_.emplace_back(
                points[i].pos,
                uv,
                col,
                points[i].depth
            );
        }
        for (std::uint32_t i = 2u; i < num_points; i++) {
            indices_.emplace_back(vertex_ptr_);
            indices_.emplace_back(vertex_ptr_ + i - 1u);
            indices_.emplace_back(vertex_ptr_ + i);
        }
        vertex_ptr_ += num_points;
    }
}

void drawlist2d::add_shadow_convex(const point_3d* points, std::uint32_t num_points, color_u32 col, float shadow_size, bool filled)
{
    if (num_points < 3u ||
        (col & color::alpha_mask) == 0u)
        return;
    
    const int vertex_winding = (
        ((points[0].pos.x * (points[1].pos.y - points[2].pos.y)) +
            (points[1].pos.x * (points[2].pos.y - points[0].pos.y)) +
            (points[2].pos.x * (points[0].pos.y - points[1].pos.y))) < 0.0f) ? -1 : 1;
    const bool use_inset_distance = renderer_->flags().anti_aliased_fill && !filled;
    const vec2 inset_distance = vec2(0.5f);
    const vec4 shadow_uvs = shared_data_->shadow_uvs;
    const vec2 tex_size = vec2(
        static_cast<float>(renderer_->font_atlas_->get_width()),
        static_cast<float>(renderer_->font_atlas_->get_height())
    );
    const vec2 inv_tex_size = vec2(1.f) / tex_size;
    const vec2 uv_min_in = vec2(shadow_uvs.x, shadow_uvs.y);
    const vec2 uv_max_in = vec2(shadow_uvs.z, shadow_uvs.w);
    const vec2 solid_uv = uv_max_in;
    const vec2 edge_uv = vec2(uv_min_in.x, uv_max_in.y);
    const vec2 solid_to_edge_delta_texels = (edge_uv - solid_uv) * tex_size;
    const std::uint32_t num_edges = num_points;
    shared_data_->temp_buffer.resize(num_edges);
    shared_data_->temp_buffer2.resize(num_edges);
    auto* edge_size_scales = shared_data_->temp_buffer2.data();
    vec2* edge_normals = shared_data_->temp_buffer.data();
    for (std::uint32_t edge_index = 0u; edge_index < num_edges; edge_index++) {
        vec2 edge_start = points[edge_index].pos;
        vec2 edge_end = points[(edge_index + 1) % num_edges].pos;
        vec2 edge_normal = vec2(edge_end.y - edge_start.y, -(edge_end.x - edge_start.x)).normalize();
        edge_normals[edge_index] = edge_normal * vec2(static_cast<float>(vertex_winding));
    }
    {
        vec2 prev_edge_normal = edge_normals[num_edges - 1u];
        for (std::uint32_t edge_index = 0u; edge_index < num_edges; edge_index++) {
            vec2 edge_normal = edge_normals[edge_index];
            float cos_angle_coverage = edge_normal.dot(prev_edge_normal);
            if (cos_angle_coverage < 0.999999f) {
                float angle_coverage = std::acos(cos_angle_coverage);
                if (cos_angle_coverage <= 0.f)
                    angle_coverage *= 0.5f;
                edge_size_scales[edge_index] = 1.f / std::cos(angle_coverage * 0.5f);
            }
            else {
                edge_size_scales[edge_index] = 1.f;
            }
            prev_edge_normal = edge_normal;
        }
    }
    vec2 prev_edge_normal = edge_normals[num_edges - 1u];
    vec2 edge_start = points[0].pos;
    float edge_start_depth = points[0].depth;
    if (use_inset_distance)
        edge_start -= (edge_normals[0] + prev_edge_normal).normalize() * inset_distance;
    for (std::uint32_t edge_index = 0u; edge_index < num_edges; edge_index++) {
        vec2 edge_end = points[(edge_index + 1u) % num_edges].pos;
        float edge_end_depth = points[(edge_index + 1u) % num_edges].depth;
        vec2 edge_normal = edge_normals[edge_index];
        const float size_scale_start = edge_size_scales[edge_index];
        const float size_scale_end = edge_size_scales[(edge_index + 1) % num_edges];
        if (use_inset_distance)
            edge_end -= (edge_normals[(edge_index + 1u) % num_edges] +
                edge_normal).normalize() * inset_distance;
        float cos_angle_coverage = edge_normal.dot(prev_edge_normal);
        if (cos_angle_coverage < 0.999999f) {
            std::uint32_t num_steps = (cos_angle_coverage <= 0.0f) ? 2u : 1u;
            for (std::uint32_t step = 0u; step < num_steps; step++) {
                if (num_steps > 1u) {
                    if (step == 0u)
                        edge_normal = (edge_normal + prev_edge_normal).normalize();
                    else
                        edge_normal = edge_normals[edge_index];
                    cos_angle_coverage = edge_normal.dot(prev_edge_normal);
                }
                const float angle_coverage = std::acos(cos_angle_coverage);
                const float sin_angle_coverage = std::sin(angle_coverage);
                const vec2 edge_delta = solid_to_edge_delta_texels * vec2(size_scale_start);
                const vec2 rotated_edge_delta = vec2(
                    (edge_delta.x * cos_angle_coverage) +
                        (edge_delta.y * sin_angle_coverage),
                    (edge_delta.x * sin_angle_coverage) +
                        (edge_delta.y * cos_angle_coverage)
                );
                const vec2 edge_delta_uv = edge_delta * inv_tex_size;
                const vec2 rotated_edge_delta_uv = rotated_edge_delta * inv_tex_size;
                const vec2 expanded_edge_uv = solid_uv + edge_delta_uv;
                const vec2 other_edge_uv = solid_uv + rotated_edge_delta_uv;
                const vec2 expanded_thickness = vec2(shadow_size * size_scale_start);
                const vec2 outer_edge_start = edge_start + (prev_edge_normal * expanded_thickness);
                const vec2 outer_edge_end = edge_start + (edge_normal * expanded_thickness);
                vertices_.emplace_back(edge_start, solid_uv, col, edge_start_depth);
                vertices_.emplace_back(outer_edge_end, expanded_edge_uv, col, edge_start_depth);
                vertices_.emplace_back(outer_edge_start, other_edge_uv, col, edge_start_depth);
                indices_.emplace_back(vertex_ptr_ + 0u);
                indices_.emplace_back(vertex_ptr_ + 1u);
                indices_.emplace_back(vertex_ptr_ + 2u);
                vertex_ptr_ += 3u;
                prev_edge_normal = edge_normal;
            }
        }
        const float edge_length = (edge_end - edge_start).length();
        if (edge_length > 0.00001f) {
            const vec2 outer_edge_start = edge_start + (edge_normal * vec2(shadow_size * size_scale_start));
            const vec2 outer_edge_end = edge_end + (edge_normal * vec2(shadow_size * size_scale_end));
            const vec2 scaled_edge_uv_start = solid_uv + ((edge_uv - solid_uv) * vec2(size_scale_start));
            const vec2 scaled_edge_uv_end = solid_uv + ((edge_uv - solid_uv) * vec2(size_scale_end));
            vertices_.emplace_back(edge_start, solid_uv, col, edge_start_depth);
            vertices_.emplace_back(edge_end, solid_uv, col, edge_end_depth);
            vertices_.emplace_back(outer_edge_end, scaled_edge_uv_end, col, edge_end_depth);
            vertices_.emplace_back(outer_edge_start, scaled_edge_uv_start, col, edge_start_depth);
            indices_.emplace_back(vertex_ptr_ + 0u);
            indices_.emplace_back(vertex_ptr_ + 1u);
            indices_.emplace_back(vertex_ptr_ + 2u);
            indices_.emplace_back(vertex_ptr_ + 0u);
            indices_.emplace_back(vertex_ptr_ + 2u);
            indices_.emplace_back(vertex_ptr_ + 3u);
            vertex_ptr_ += 4u;
        }
        edge_start = edge_end;
        edge_start_depth = edge_end_depth;
    }
    [[likely]] if (filled) {
        for (std::uint32_t i = 0u; i < num_points; i++) {
            vertices_.emplace_back(
                points[i].pos,
                solid_uv,
                col,
                points[i].depth
            );
        }
        for (std::uint32_t i = 2u; i < num_points; i++) {
            indices_.emplace_back(vertex_ptr_);
            indices_.emplace_back(vertex_ptr_ + i - 1u);
            indices_.emplace_back(vertex_ptr_ + i);
        }
        vertex_ptr_ += num_points;
    }
}

void drawlist2d::add_lines(const point_3d* points, std::uint32_t num_points, color_u32 col, float line_width, bool closed)
{
    if (num_points < 2u ||
        (col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    const float aa_scale = renderer_->aa_scale_;
    assert(line_width >= 0.f && "line_width should not be negative");
    const vec2 opaque_uv = shared_data_->uv_white_px;
    const std::uint32_t count = closed ? num_points : num_points - 1u;
    const bool thick_line = (line_width > aa_scale);
    [[likely]] if (renderer_->flags().anti_aliased_lines) {
        const color_u32 col_no_alpha = col & ~color::alpha_mask;
        line_width = (std::max)(line_width, 1.0f);
        const int integer_line_width = static_cast<int>(line_width);
        const float fractional_line_width = line_width - integer_line_width;
        const bool use_texture = (renderer_->flags().anti_aliased_lines_use_tex) &&
            (integer_line_width < shared_data::kBakedLinesMaxWidth) &&
            (fractional_line_width <= 0.00001f) && (aa_scale == 1.0f);
        shared_data_->temp_buffer.clear();
        shared_data_->temp_buffer.reserve(num_points * ((use_texture || !thick_line) ? 3u : 5u));
        vec2* temp_normals = shared_data_->temp_buffer.data();
        vec2* temp_points = temp_normals + num_points;
        for (std::uint32_t i1 = 0u; i1 < count; i1++) {
            const std::uint32_t i2 = (i1 + 1u) == static_cast<std::int32_t>(num_points) ? 0u : i1 + 1u;
            vec2 d(points[i2].pos.x - points[i1].pos.x,
                   points[i2].pos.y - points[i1].pos.y
            );
            d = d.normalize();
            temp_normals[i1].x = d.y;
            temp_normals[i1].y = -d.x;
        }
        if (!closed) {
            temp_normals[num_points - 1u] = temp_normals[num_points - 2u];
        }
        if (use_texture || !thick_line) {
            const float half_draw_size = use_texture ?
                ((line_width * 0.5f) + 1u) : aa_scale;
            if (!closed) {
                temp_points[0] = points[0].pos + temp_normals[0] * vec2(half_draw_size);
                temp_points[1] = points[0].pos - temp_normals[0] * vec2(half_draw_size);
                temp_points[(num_points - 1u) * 2u + 0u] =
                    points[num_points - 1u].pos + temp_normals[num_points - 1u] * vec2(half_draw_size);
                temp_points[(num_points - 1u) * 2u + 1u] =
                    points[num_points - 1u].pos - temp_normals[num_points - 1u] * vec2(half_draw_size);
            }
            std::uint32_t idx1 = vertex_ptr_;
            for (std::uint32_t i1 = 0u; i1 < count; i1++)
            {
                const std::uint32_t i2 = (i1 + 1u) == num_points ? 0u : i1 + 1u;
                const std::uint32_t idx2 = ((i1 + 1u) == num_points) ?
                    vertex_ptr_ : (idx1 + (use_texture ? 2u : 3u));
                vec2 dm(
                    (temp_normals[i1].x + temp_normals[i2].x) * 0.5f,
                    (temp_normals[i1].y + temp_normals[i2].y) * 0.5f
                );
                dm = dm.normalize(100.f);
                dm *= vec2(half_draw_size);
                vec2* out_vtx = &temp_points[i2 * 2u];
                out_vtx[0].x = points[i2].pos.x + dm.x;
                out_vtx[0].y = points[i2].pos.y + dm.y;
                out_vtx[1].x = points[i2].pos.x - dm.x;
                out_vtx[1].y = points[i2].pos.y - dm.y;
                if (use_texture) {
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx1 + 0u);
                    indices_.emplace_back(idx1 + 1u);
                    indices_.emplace_back(idx2 + 1u);
                    indices_.emplace_back(idx1 + 1u);
                    indices_.emplace_back(idx2 + 0u);
                }
                else {
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx1 + 2u);
                    indices_.emplace_back(idx1 + 2u);
                    indices_.emplace_back(idx2 + 2u);
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx2 + 1u);
                    indices_.emplace_back(idx1 + 1u);
                    indices_.emplace_back(idx1 + 0u);
                    indices_.emplace_back(idx1 + 0u);
                    indices_.emplace_back(idx2 + 0u);
                    indices_.emplace_back(idx2 + 1u);
                }
                idx1 = idx2;
            }
            if (use_texture) {
                vec4 tex_uvs = shared_data_->tex_uv_lines[integer_line_width];
                const vec2 tex_uv0(tex_uvs.x, tex_uvs.y);
                const vec2 tex_uv1(tex_uvs.z, tex_uvs.w);
                for (std::uint32_t i = 0u; i < num_points; i++) {
                    vertices_.emplace_back(temp_points[i * 2u + 0u], tex_uv0, col, points[i].depth);
                    vertices_.emplace_back(temp_points[i * 2u + 1u], tex_uv1, col, points[i].depth);
                    vertex_ptr_ += 2u;
                }
            }
            else {
                for (std::uint32_t i = 0u; i < num_points; i++) {
                    vertices_.emplace_back(points[i].pos, opaque_uv, col, points[i].depth);
                    vertices_.emplace_back(temp_points[i * 2u + 0u], opaque_uv, col_no_alpha, points[i].depth);
                    vertices_.emplace_back(temp_points[i * 2u + 1u], opaque_uv, col_no_alpha, points[i].depth);
                    vertex_ptr_ += 3u;
                }
            }
        }
        else {
            const float half_inner_line_width = (line_width - aa_scale) * 0.5f;
            if (!closed) {
                const std::uint32_t points_last = num_points - 1u;
                temp_points[0] = points[0].pos + temp_normals[0] * vec2(half_inner_line_width + aa_scale);
                temp_points[1] = points[0].pos + temp_normals[0] * vec2(half_inner_line_width);
                temp_points[2] = points[0].pos - temp_normals[0] * vec2(half_inner_line_width);
                temp_points[3] = points[0].pos - temp_normals[0] * vec2(half_inner_line_width + aa_scale);
                temp_points[points_last * 4u + 0u] = points[points_last].pos + temp_normals[points_last] * vec2(half_inner_line_width + aa_scale);
                temp_points[points_last * 4u + 1u] = points[points_last].pos + temp_normals[points_last] * vec2(half_inner_line_width);
                temp_points[points_last * 4u + 2u] = points[points_last].pos - temp_normals[points_last] * vec2(half_inner_line_width);
                temp_points[points_last * 4u + 3u] = points[points_last].pos - temp_normals[points_last] * vec2(half_inner_line_width + aa_scale);
            }
            std::uint32_t idx1 = vertex_ptr_;
            for (std::uint32_t i1 = 0u; i1 < count; i1++) {
                const std::uint32_t i2 = (i1 + 1u) == num_points ? 0u : (i1 + 1u);
                const std::uint32_t idx2 = (i1 + 1u) == num_points ? vertex_ptr_ : (idx1 + 4u);
                vec2 dm(
                    (temp_normals[i1].x + temp_normals[i2].x) * 0.5f,
                    (temp_normals[i1].y + temp_normals[i2].y) * 0.5f
                );
                dm = dm.normalize(100.f);
                vec2 dm_out = dm * vec2(half_inner_line_width + aa_scale);
                vec2 dm_in = dm * vec2(half_inner_line_width);
                vec2* out_vtx = &temp_points[i2 * 4];
                out_vtx[0].x = points[i2].pos.x + dm_out.x;
                out_vtx[0].y = points[i2].pos.y + dm_out.y;
                out_vtx[1].x = points[i2].pos.x + dm_in.x;
                out_vtx[1].y = points[i2].pos.y + dm_in.y;
                out_vtx[2].x = points[i2].pos.x - dm_in.x;
                out_vtx[2].y = points[i2].pos.y - dm_in.y;
                out_vtx[3].x = points[i2].pos.x - dm_out.x;
                out_vtx[3].y = points[i2].pos.y - dm_out.y;
                indices_.emplace_back(idx2 + 1u);
                indices_.emplace_back(idx1 + 1u);
                indices_.emplace_back(idx1 + 2u);
                indices_.emplace_back(idx1 + 2u);
                indices_.emplace_back(idx2 + 2u);
                indices_.emplace_back(idx2 + 1u);
                indices_.emplace_back(idx2 + 1u);
                indices_.emplace_back(idx1 + 1u);
                indices_.emplace_back(idx1 + 0u);
                indices_.emplace_back(idx1 + 0u);
                indices_.emplace_back(idx2 + 0u);
                indices_.emplace_back(idx2 + 1u);
                indices_.emplace_back(idx2 + 2u);
                indices_.emplace_back(idx1 + 2u);
                indices_.emplace_back(idx1 + 3u);
                indices_.emplace_back(idx1 + 3u);
                indices_.emplace_back(idx2 + 3u);
                indices_.emplace_back(idx2 + 2u);
                idx1 = idx2;
            }
            for (std::uint32_t i = 0; i < num_points; i++) {
                vertices_.emplace_back(temp_points[i * 4u + 0u], opaque_uv, col_no_alpha, points[i].depth);
                vertices_.emplace_back(temp_points[i * 4u + 1u], opaque_uv, col, points[i].depth);
                vertices_.emplace_back(temp_points[i * 4u + 2u], opaque_uv, col, points[i].depth);
                vertices_.emplace_back(temp_points[i * 4u + 3u], opaque_uv, col_no_alpha, points[i].depth);
                vertex_ptr_ += 4u;
            }
        }
    }
    else {
        for (std::uint32_t i1 = 0u; i1 < count; i1++) {
            const std::uint32_t i2 = (i1 + 1u) == num_points ? 0u : i1 + 1u;
            const vec2& p1 = points[i1].pos;
            const vec2& p2 = points[i2].pos;
            vec2 d(
                p2.x - p1.x,
                p2.y - p1.y
            );
            d = d.normalize();
            d *= vec2(line_width * 0.5f);
            vertices_.emplace_back(vec2{ p1.x + d.y, p1.y - d.x }, opaque_uv, col, points[i1].depth);
            vertices_.emplace_back(vec2{ p2.x + d.y, p2.y - d.x }, opaque_uv, col, points[i2].depth);
            vertices_.emplace_back(vec2{ p2.x - d.y, p2.y + d.x }, opaque_uv, col, points[i2].depth);
            vertices_.emplace_back(vec2{ p1.x - d.y, p1.y + d.x }, opaque_uv, col, points[i1].depth);
            indices_.emplace_back(vertex_ptr_ + 0u);
            indices_.emplace_back(vertex_ptr_ + 1u);
            indices_.emplace_back(vertex_ptr_ + 2u);
            indices_.emplace_back(vertex_ptr_ + 0u);
            indices_.emplace_back(vertex_ptr_ + 2u);
            indices_.emplace_back(vertex_ptr_ + 3u);
            vertex_ptr_ += 4u;
        }
    }
}

void drawlist2d::add_line_multicolor(const point_3d& start_p, const point_3d& end_p, color_u32 col_start, color_u32 col_end, float line_width)
{
    const bool odd = (static_cast<int>(std::round(line_width)) & 1) != 0;
    const float snap = odd ? 0.5f : 0.0f;
    const vec2 start = start_p.pos + vec2(snap);
    const vec2 end = end_p.pos + vec2(snap);
    if ((col_start & color::alpha_mask) == 0u &&
        (col_end & color::alpha_mask) == 0u) [[unlikely]]
        return;

    const float aa_scale = renderer_->aa_scale_;
    assert(path_.empty());
    const vec2 d = end - start;
    const float length = d.length();
    if (length < 1e-6f)
        return;
    const vec2 dir = d * vec2(1.f / length);
    const vec2 n = dir.perp() * vec2(line_width * 0.5f);
    const vec2 opaque_uv = shared_data_->uv_white_px;
    const color_u32 col_no_alpha_start = col_start & ~color::alpha_mask;
    const color_u32 col_no_alpha_end = col_end & ~color::alpha_mask;
    if (!renderer_->flags().anti_aliased_lines) [[unlikely]] {
        indices_.push_back(vertex_ptr_ + 0u);
        indices_.push_back(vertex_ptr_ + 1u);
        indices_.push_back(vertex_ptr_ + 2u);
        indices_.push_back(vertex_ptr_ + 0u);
        indices_.push_back(vertex_ptr_ + 2u);
        indices_.push_back(vertex_ptr_ + 3u);
        vertices_.emplace_back(start - n, opaque_uv, col_start, start_p.depth);
        vertices_.emplace_back(start + n, opaque_uv, col_start, start_p.depth);
        vertices_.emplace_back(end + n, opaque_uv, col_end, end_p.depth);
        vertices_.emplace_back(end - n, opaque_uv, col_end, end_p.depth);
        vertex_ptr_ += 4u;
        return;
    }
    line_width = (std::max)(line_width, 1.0f);
    const bool thick_line = (line_width > aa_scale);
    const int integer_line_width = static_cast<int>(line_width);
    const float fractional_line_width = line_width - integer_line_width;
    const bool use_texture = (renderer_->flags().anti_aliased_lines_use_tex) &&
        (integer_line_width < shared_data::kBakedLinesMaxWidth) &&
        (fractional_line_width <= 0.00001f) && (aa_scale == 1.0f);
    shared_data_->temp_buffer.clear();
    shared_data_->temp_buffer.reserve(2u * ((use_texture || !thick_line) ? 3u : 5u));
    vec2* temp_normals = shared_data_->temp_buffer.data();
    vec2* temp_points = temp_normals + 2u;
    temp_normals[0].x = dir.y;
    temp_normals[0].y = -dir.x;
    temp_normals[1] = temp_normals[0];
    if (use_texture || !thick_line) {
        const float half_draw_size = use_texture ?
            ((line_width * 0.5f) + 1.f) : aa_scale;
        temp_points[0] = start + temp_normals[0] * vec2(half_draw_size);
        temp_points[1] = start - temp_normals[0] * vec2(half_draw_size);
        temp_points[2] = end + temp_normals[1] * vec2(half_draw_size);
        temp_points[3] = end - temp_normals[1] * vec2(half_draw_size);
        std::uint32_t idx1 = vertex_ptr_;
        std::uint32_t idx2 = idx1 + (use_texture ? 2u : 3u);
        vec2 dm(
            (temp_normals[0].x + temp_normals[1].x) * 0.5f,
            (temp_normals[0].y + temp_normals[1].y) * 0.5f
        );
        dm = dm.normalize(100.f);
        dm *= vec2(half_draw_size);
        temp_points[2] = end + dm;
        temp_points[3] = end - dm;
        if (use_texture) {
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx1 + 0u);
            indices_.emplace_back(idx1 + 1u);
            indices_.emplace_back(idx2 + 1u);
            indices_.emplace_back(idx1 + 1u);
            indices_.emplace_back(idx2 + 0u);
        }
        else {
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx1 + 2u);
            indices_.emplace_back(idx1 + 2u);
            indices_.emplace_back(idx2 + 2u);
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx2 + 1u);
            indices_.emplace_back(idx1 + 1u);
            indices_.emplace_back(idx1 + 0u);
            indices_.emplace_back(idx1 + 0u);
            indices_.emplace_back(idx2 + 0u);
            indices_.emplace_back(idx2 + 1u);
        }
        if (use_texture) {
            vec4 tex_uvs = shared_data_->tex_uv_lines[integer_line_width];
            const vec2 tex_uv0(tex_uvs.x, tex_uvs.y);
            const vec2 tex_uv1(tex_uvs.z, tex_uvs.w);
            vertices_.emplace_back(temp_points[0], tex_uv0, col_start, start_p.depth);
            vertices_.emplace_back(temp_points[1], tex_uv1, col_start, start_p.depth);
            vertices_.emplace_back(temp_points[2], tex_uv0, col_end, end_p.depth);
            vertices_.emplace_back(temp_points[3], tex_uv1, col_end, end_p.depth);
            vertex_ptr_ += 4u;
        }
        else {
            vertices_.emplace_back(start, opaque_uv, col_start, start_p.depth);
            vertices_.emplace_back(temp_points[0], opaque_uv, col_no_alpha_start, start_p.depth);
            vertices_.emplace_back(temp_points[1], opaque_uv, col_no_alpha_start, start_p.depth);
            vertices_.emplace_back(end, opaque_uv, col_end, end_p.depth);
            vertices_.emplace_back(temp_points[2], opaque_uv, col_no_alpha_end, end_p.depth);
            vertices_.emplace_back(temp_points[3], opaque_uv, col_no_alpha_end, end_p.depth);
            vertex_ptr_ += 6u;
        }
    }
    else {
        const float half_inner_line_width = (line_width - aa_scale) * 0.5f;
        temp_points[0] = start + temp_normals[0] * vec2(half_inner_line_width + aa_scale);
        temp_points[1] = start + temp_normals[0] * vec2(half_inner_line_width);
        temp_points[2] = start - temp_normals[0] * vec2(half_inner_line_width);
        temp_points[3] = start - temp_normals[0] * vec2(half_inner_line_width + aa_scale);
        temp_points[4] = end + temp_normals[1] * vec2(half_inner_line_width + aa_scale);
        temp_points[5] = end + temp_normals[1] * vec2(half_inner_line_width);
        temp_points[6] = end - temp_normals[1] * vec2(half_inner_line_width);
        temp_points[7] = end - temp_normals[1] * vec2(half_inner_line_width + aa_scale);
        std::uint32_t idx1 = vertex_ptr_;
        std::uint32_t idx2 = idx1 + 4u;
        vec2 dm(
            (temp_normals[0].x + temp_normals[1].x) * 0.5f,
            (temp_normals[0].y + temp_normals[1].y) * 0.5f
        );
        dm = dm.normalize(100.f);
        vec2 dm_out = dm * vec2(half_inner_line_width + aa_scale);
        vec2 dm_in = dm * vec2(half_inner_line_width);
        temp_points[4] = end + dm_out;
        temp_points[5] = end + dm_in;
        temp_points[6] = end - dm_in;
        temp_points[7] = end - dm_out;
        indices_.emplace_back(idx2 + 1u);
        indices_.emplace_back(idx1 + 1u);
        indices_.emplace_back(idx1 + 2u);
        indices_.emplace_back(idx1 + 2u);
        indices_.emplace_back(idx2 + 2u);
        indices_.emplace_back(idx2 + 1u);
        indices_.emplace_back(idx2 + 1u);
        indices_.emplace_back(idx1 + 1u);
        indices_.emplace_back(idx1 + 0u);
        indices_.emplace_back(idx1 + 0u);
        indices_.emplace_back(idx2 + 0u);
        indices_.emplace_back(idx2 + 1u);
        indices_.emplace_back(idx2 + 2u);
        indices_.emplace_back(idx1 + 2u);
        indices_.emplace_back(idx1 + 3u);
        indices_.emplace_back(idx1 + 3u);
        indices_.emplace_back(idx2 + 3u);
        indices_.emplace_back(idx2 + 2u);
        vertices_.emplace_back(temp_points[0], opaque_uv, col_no_alpha_start, start_p.depth);
        vertices_.emplace_back(temp_points[1], opaque_uv, col_start, start_p.depth);
        vertices_.emplace_back(temp_points[2], opaque_uv, col_start, start_p.depth);
        vertices_.emplace_back(temp_points[3], opaque_uv, col_no_alpha_start, start_p.depth);
        vertices_.emplace_back(temp_points[4], opaque_uv, col_no_alpha_end, end_p.depth);
        vertices_.emplace_back(temp_points[5], opaque_uv, col_end, end_p.depth);
        vertices_.emplace_back(temp_points[6], opaque_uv, col_end, end_p.depth);
        vertices_.emplace_back(temp_points[7], opaque_uv, col_no_alpha_end, end_p.depth);
        vertex_ptr_ += 8u;
    }
}

r2_end_