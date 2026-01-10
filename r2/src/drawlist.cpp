#include <r2/renderer.h>
#include <algorithm>
#include "internal.h"
#include <r2/font/font_atlas.h>


r2_begin_

void renderer2d::add_convex_filled(const vec2* points, std::uint32_t num_points, color_u32 col)
{
    if (num_points < 3u ||
        (col & color::alpha_mask) == 0u) [[unlikely]]
        return;
    
    const vec2 uv = shared_data_.uv_white_px;

    if (flags_.anti_aliased_fill) {
        const std::uint32_t vtx_inner_idx = vertex_ptr_;
        const std::uint32_t vtx_outer_idx = vertex_ptr_ + 1u;
        for (std::uint32_t i = 2u; i < num_points; i++) {
            indices_.emplace_back(vtx_inner_idx);
            indices_.emplace_back(vtx_inner_idx + ((i - 1u) << 1u));
            indices_.emplace_back(vtx_inner_idx + (i << 1u));
        }

        shared_data_.temp_buffer.clear();
        shared_data_.temp_buffer.resize(num_points);
        for (std::uint32_t i0 = num_points - 1u, i1 = 0u; i1 < num_points; i0 = i1++) {
            const vec2& p0 = points[i0];
            const vec2& p1 = points[i1];
            vec2 d = (p0 - p1).normalize();
            shared_data_.temp_buffer[i0].x = d.y;
            shared_data_.temp_buffer[i0].y = -d.x;
        }

        const color_u32 col_no_alpha = col & ~color::alpha_mask;
        for (std::uint32_t i0 = num_points - 1u, i1 = 0u; i1 < num_points; i0 = i1++) {
            const vec2& n0 = shared_data_.temp_buffer[i0];
            const vec2& n1 = shared_data_.temp_buffer[i1];

            vec2 dm = ((n0 + n1) * vec2(0.5f)).normalize(100.f);

            dm.x *= aa_scale_ * 0.5f;
            dm.y *= aa_scale_ * 0.5f;

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

void renderer2d::add_shadow_convex(const vec2* points, std::uint32_t num_points, color_u32 col, float shadow_size, bool filled)
{
    if (num_points < 3u ||
        (col & color::alpha_mask) == 0u)
        return;

    const int vertex_winding = (
        ((points[0].x * (points[1].y - points[2].y)) + 
            (points[1].x * (points[2].y - points[0].y)) + 
            (points[2].x * (points[0].y - points[1].y))) < 0.0f) ? -1 : 1;
    const bool use_inset_distance = flags_.anti_aliased_fill && !filled;
    const vec2 inset_distance = vec2(0.5f);

    const vec4 shadow_uvs = shared_data_.shadow_uvs;

    const vec2 tex_size = vec2(
        static_cast<float>(font_atlas_->get_width()),
        static_cast<float>(font_atlas_->get_height())
    );
    const vec2 inv_tex_size = vec2(1.f) / tex_size;

    const vec2 uv_min_in = vec2(shadow_uvs.x, shadow_uvs.y);
    const vec2 uv_max_in = vec2(shadow_uvs.z, shadow_uvs.w);

    const vec2 solid_uv = uv_max_in;
    const vec2 edge_uv = vec2(uv_min_in.x, uv_max_in.y);

    const vec2 solid_to_edge_delta_texels = (edge_uv - solid_uv) * tex_size;

    const std::uint32_t num_edges = num_points;

    shared_data_.temp_buffer.resize(num_edges);
    shared_data_.temp_buffer2.resize(num_edges);
    auto* edge_size_scales = shared_data_.temp_buffer2.data();
    vec2* edge_normals = shared_data_.temp_buffer.data();

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

void renderer2d::add_lines(const vec2* points, std::uint32_t num_points, color_u32 col, float line_width, bool closed)
{
    if (num_points < 2u ||
        (col & color::alpha_mask) == 0u) [[unlikely]]
        return;

    assert(line_width >= 0.f && "line_width should not be negative");

    const vec2 opaque_uv = shared_data_.uv_white_px;
    const std::uint32_t count = closed ? num_points : num_points - 1u;
    const bool thick_line = (line_width > aa_scale_);

    [[likely]] if (flags_.anti_aliased_lines) {
        const color_u32 col_no_alpha = col & ~color::alpha_mask;

        line_width = (std::max)(line_width, 1.0f);
        const int integer_line_width = static_cast<int>(line_width);
        const float fractional_line_width = line_width - integer_line_width;

        const bool use_texture = (flags_.anti_aliased_lines_use_tex) &&
            (integer_line_width < font_atlas::kBakedLinesMaxWidth) &&
            (fractional_line_width <= 0.00001f) && (aa_scale_ == 1.0f);

        shared_data_.temp_buffer.clear();
        shared_data_.temp_buffer.reserve(num_points * ((use_texture || !thick_line) ? 3u : 5u));
        vec2* temp_normals = shared_data_.temp_buffer.data();
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
                ((line_width * 0.5f) + 1u) : aa_scale_;

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
                vec4 tex_uvs = font_atlas_->tex_uv_lines[integer_line_width];
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
            const float half_inner_line_width = (line_width - aa_scale_) * 0.5f;

            if (!closed) {
                const std::uint32_t points_last = num_points - 1u;
                temp_points[0] = points[0] + temp_normals[0] * vec2(half_inner_line_width + aa_scale_);
                temp_points[1] = points[0] + temp_normals[0] * vec2(half_inner_line_width);
                temp_points[2] = points[0] - temp_normals[0] * vec2(half_inner_line_width);
                temp_points[3] = points[0] - temp_normals[0] * vec2(half_inner_line_width + aa_scale_);
                temp_points[points_last * 4u + 0u] = points[points_last] + temp_normals[points_last] * vec2(half_inner_line_width + aa_scale_);
                temp_points[points_last * 4u + 1u] = points[points_last] + temp_normals[points_last] * vec2(half_inner_line_width);
                temp_points[points_last * 4u + 2u] = points[points_last] - temp_normals[points_last] * vec2(half_inner_line_width);
                temp_points[points_last * 4u + 3u] = points[points_last] - temp_normals[points_last] * vec2(half_inner_line_width + aa_scale_);
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

                vec2 dm_out = dm * vec2(half_inner_line_width + aa_scale_);
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

r2_end_