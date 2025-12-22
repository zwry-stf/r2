#include "font_atlas.h"
#include <r2/error.h>
#include <r2/renderer.h>


r2_begin_

font_atlas::font_atlas(renderer2d* instance) noexcept
    : renderer_(instance),
      width_(kDefaultSize),
      height_(kDefaultSize)
{
    data32_.resize(width_ * height_);

    // reserve white pixel
    white_px_id_ = register_rect(1u, 1u);

    // reserve tex lines
    if (instance->flags().anti_aliased_lines_use_tex) {
        tex_lines_id_ = register_rect(kBakedLinesMaxWidth + 2u, kBakedLinesMaxWidth + 1u);
    }
}


bool font_atlas::check_side(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    const std::uint32_t x2 = x + width + padding_;
    const std::uint32_t y2 = y + height + padding_;
    if (x2 > width_ || y2 > height_) {
        return false;
    }

    for (const auto& r2 : rects_) {
        const bool intersects_x =
            r2.pos_x <= x2 &&
            r2.pos_x + r2.width + padding_ > x;

        const bool intersects_y =
            r2.pos_y <= y2 &&
            r2.pos_y + r2.height + padding_ > y;

        if (intersects_x &&
            intersects_y) {
            return false;
        }
    }

    return true;
}

void font_atlas::find_rect(std::uint32_t width, std::uint32_t height, std::uint32_t& out_x, std::uint32_t& out_y)
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    assert(width + padding_ * 2u <= width_);
    assert(height + padding_ * 2u <= height_);
    assert(width > 0u);
    assert(height > 0u);

    if (rects_.empty()) {
        out_x = padding_;
        out_y = padding_;
        return;
    }

    for (const auto& r : rects_) {
        // check right
        std::uint32_t x = r.pos_x + r.width + padding_;
        std::uint32_t y = r.pos_y;

        if (check_side(x, y, width, height)) {
            out_x = x;
            out_y = y;
            return;
        }

        // check left
        if (r.pos_x > width + padding_ * 2u) {
            x = r.pos_x - width - padding_;
            if (check_side(x, y, width, height)) {
                out_x = x;
                out_y = y;
                return;
            }
        }

        // check below
        x = r.pos_x;
        y = r.pos_y + r.height + padding_;
        if (check_side(x, y, width, height)) {
            out_x = x;
            out_y = y;
            return;
        }

        // check above
        if (r.pos_y > height + padding_ * 2u) {
            y = r.pos_y - height - padding_;
            if (check_side(x, y, width, height)) {
                out_x = x;
                out_y = y;
                return;
            }
        }
    }

    throw error(error_code::font_atlas_full);
}

void font_atlas::add_tex_lines()
{
    if (!renderer_->flags().anti_aliased_lines_use_tex)
        return;

    const auto& r = this->get_rect(tex_lines_id_);

    for (std::uint32_t n = 0u; n < kBakedLinesMaxWidth + 1u; n++) {
        std::uint32_t y = n;
        std::uint32_t line_width = n;

        assert(line_width <= r.width);
        std::uint32_t pad_left = (r.width - line_width) / 2u;
        assert(pad_left + line_width <= r.width);
        std::uint32_t pad_right = r.width - (pad_left + line_width);

        assert(pad_left + line_width + pad_right == r.width && y < r.height);

        std::uint32_t* write_ptr = &data32_[r.pos_x + ((r.pos_y + y) * width_)];
        for (std::uint32_t i = 0u; i < pad_left; i++)
            *(write_ptr + i) = static_cast<std::uint32_t>(
                (color_u32)color::color::white().transparent());

        std::memset(
            write_ptr + pad_left, 
            0xFFu, 
            line_width * sizeof(std::uint32_t)
        );

        for (std::uint32_t i = 0u; i < pad_right; i++)
            *(write_ptr + pad_left + line_width + i) = static_cast<std::uint32_t>(
                (color_u32)color::color::white().transparent());

        vec2 uv0 = vec2(
            static_cast<float>(r.pos_x + pad_left - 1u), 
            static_cast<float>(r.pos_y + y)
        );
        vec2 uv1 = vec2(
            static_cast<float>(r.pos_x + pad_left + line_width + 1u),
            static_cast<float>(r.pos_y + y + 1u)
        );
        float half_v = (uv0.y + uv1.y) * 0.5f;
        tex_uv_lines[n] = vec4(
            uv0.x / static_cast<float>(width_),
            half_v / static_cast<float>(height_), 
            uv1.x / static_cast<float>(width_),
            half_v / static_cast<float>(height_)
        );
    }
}

std::uint32_t font_atlas::register_rect(std::uint32_t width, std::uint32_t height)
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    std::uint32_t x, y;
    find_rect(width, height, x, y);

    if (!free_rect_slots_.empty()) {
        auto idx = free_rect_slots_.back();
        free_rect_slots_.pop_back();

        rects_[idx].pos_x = x;
        rects_[idx].pos_y = y;
        rects_[idx].width = width;
        rects_[idx].height = height;
        return idx;
    }
    // push if no empty rect found
    rects_.emplace_back(x, y, width, height);

    return static_cast<std::uint32_t>(rects_.size()) - 1u;
}

void font_atlas::remove_rect(std::uint32_t id)
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    if (id >= rects_.size()) {
        // maybe show a warning here
        return;
    }

#if defined(_DEBUG)
    // clear in debug
    auto& rect = rects_[id];
    for (std::uint32_t y = 0u; y < rect.height; ++y) {
        uint32_t* dst_row = data32_.data() + (rect.pos_y + y) *
            width_ + rect.pos_x;
        std::memset(dst_row, 0, rect.width * sizeof(std::uint32_t));
    }
#endif

    // make "invisible" to "find_rect"
    rects_[id].pos_x = 0;
    rects_[id].pos_y = 0;
    rects_[id].width = 0;
    rects_[id].height = 0;

    free_rect_slots_.push_back(id);
}

void font_atlas::get_rect_uv(std::uint32_t id, vec2& uv_min, vec2& uv_max) const
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    assert(id < rects_.size());

    auto& r = rects_[id];

    uv_min.x = static_cast<float>(r.pos_x) / static_cast<float>(width_);
    uv_min.y = static_cast<float>(r.pos_y) / static_cast<float>(height_);

    uv_max.x = uv_min.x + static_cast<float>(r.width) / static_cast<float>(width_);
    uv_max.y = uv_min.y + static_cast<float>(r.height) / static_cast<float>(height_);
}

atlas_rect font_atlas::get_rect(std::uint32_t id)
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    assert(id < rects_.size());

    return rects_[id];
}

void font_atlas::write_data(std::uint32_t id, const std::uint8_t* data, std::size_t size)
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    (void)size;

    atlas_rect rect = get_rect(id);

    assert(rect.width * rect.height == size);

    for (std::uint32_t y = 0u; y < rect.height; ++y) {
        const uint8_t* src_row = data + y * rect.width;
        uint32_t* dst_row = data32_.data() + (rect.pos_y + y) *
            width_ + rect.pos_x;
        for (std::uint32_t x = 0u; x < rect.width; x++) {
            auto byte = src_row[x];
            dst_row[x] = byte | (byte << 8) | (byte << 16) | (byte << 24);
        }
    }
}

bool font_atlas::build()
{
    // white px
    {
        constexpr std::uint8_t kWhitePixel = 0xFFu;
        write_data(white_px_id_, &kWhitePixel, 1u);
    }

    add_tex_lines();

    return true;
}

r2_end_