#include <r2/renderer.h>
#include <algorithm>
#include "internal.h"
#include <font/font_atlas.h>


r2_begin_

void renderer2d::add_convex_filled(const vec2* points, std::uint32_t num_points, color_u32 col)
{
	if (num_points < 3u ||
		(col & color::alpha_mask) == 0u)
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

void renderer2d::add_lines(const vec2* points, std::uint32_t num_points, color_u32 col, float line_width, bool closed)
{
	if (num_points < 2u ||
		(col & color::alpha_mask) == 0u)
		return;

    if (line_width == 0.f)
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