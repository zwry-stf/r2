#include <r2/font/font_atlas.h>
#include <r2/error.h>
#include <r2/renderer_base.h>
#include <r2/render_data.h>

#include <cstring>


r2_begin_

font_atlas::font_atlas(renderer_base* instance, std::uint32_t max_width, std::uint32_t max_height) noexcept
    : renderer_(instance),
      max_width_(max_width == 0 ? std::numeric_limits<std::uint32_t>::max() : max_width),
      max_height_(max_height == 0 ? std::numeric_limits<std::uint32_t>::max() : max_height)
{
}

bool font_atlas::check_side(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    if (x < padding_ ||
        y < padding_) {
        return false;
    }

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

bool font_atlas::find_rect(std::uint32_t width, std::uint32_t height, std::uint32_t& out_x, std::uint32_t& out_y)
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    assert(width > 0u);
    assert(height > 0u);

    if (width + padding_ * 2u > width_ ||
        height + padding_ * 2u > height_) {
        return false;
    }

    if (rects_.empty()) {
        out_x = padding_;
        out_y = padding_;
        return true;
    }

    for (const auto& r : rects_) {
        // check right
        std::uint32_t x = r.pos_x + r.width + padding_;
        std::uint32_t y = std::max(r.pos_y, padding_);

        if (check_side(x, y, width, height)) {
            out_x = x;
            out_y = y;
            return true;
        }

        // check left
        if (r.pos_x > width + padding_ * 2u) {
            x = r.pos_x - width - padding_;
            if (check_side(x, y, width, height)) {
                out_x = x;
                out_y = y;
                return true;
            }
        }

        // check below
        x = std::max(r.pos_x, padding_);
        y = r.pos_y + r.height + padding_;
        if (check_side(x, y, width, height)) {
            out_x = x;
            out_y = y;
            return true;
        }

        // check above
        if (r.pos_y > height + padding_ * 2u) {
            y = r.pos_y - height - padding_;
            if (check_side(x, y, width, height)) {
                out_x = x;
                out_y = y;
                return true;
            }
        }
    }

    return false;
}

bool font_atlas::add_white_pixel()
{
    constexpr std::uint8_t k_white_pixel = 0xFFu;
    auto rect_id = register_rect(1u, 1u);
    if (!rect_id) {
        return false;
    }

    write_data_init(*rect_id, &k_white_pixel, 1u);

    vec2 min, max;
    get_rect_pos(*rect_id, min, max);
    renderer_->shared_data_.pos_white_px = (min + max) * vec2(0.5f);

    return true;
}

bool font_atlas::add_tex_lines()
{
    if (!renderer_->flags().anti_aliased_lines_use_tex) {
        return true;
    }

    auto rect_id = register_rect(shared_data::k_baked_lines_max_width + 2u, shared_data::k_baked_lines_max_width + 1u);
    if (!rect_id) {
        return false;
    }

    const auto& r = this->get_rect(*rect_id);

    for (std::uint32_t n = 0u; n < shared_data::k_baked_lines_max_width + 1u; n++) {
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
        renderer_->shared_data_.tex_pos_lines[n] = vec4(
            uv0.x,
            half_v,
            uv1.x,
            half_v
        );
    }

    return true;
}

bool font_atlas::add_shadow_tex()
{
    constexpr std::uint32_t k_padding = 1u;
    constexpr std::uint32_t k_shadow_tex_size = 32u;
    constexpr float k_shadow_falloff_power = 4.f;
    constexpr float k_shadow_distance_field_offset = 3.8f;

    const std::uint32_t shadow_convex_size = k_shadow_tex_size + k_padding * 2u;
    auto rect_id = register_rect(shadow_convex_size, shadow_convex_size);
    if (!rect_id) {
        return false;
    }

    std::vector<std::uint8_t> data(shadow_convex_size * shadow_convex_size);

    const std::uint32_t side_min = k_padding;
    const std::uint32_t side_max = shadow_convex_size - k_padding - 1u;

    // calculate highest value for scale
    const float max_size = vec2(
        static_cast<float>(side_max - side_min)
    ).length();

    auto calc_shadow = [&](float dist) 
        {
            float alpha = 1.f - std::min(
                std::max(dist + k_shadow_distance_field_offset, 0.f) /
                std::max(max_size + k_shadow_distance_field_offset, 0.001f),
                1.f
            );
            return std::pow(alpha, k_shadow_falloff_power);
        };

    float scale = calc_shadow(0.f);
    scale = (scale > 0.f) ? 1.f / scale : 1.f;

    // build
    const vec2 target_point = vec2(static_cast<float>(side_max));

    for (std::uint32_t y = 0u; y < shadow_convex_size; y++) {
        for (std::uint32_t x = 0u; x < shadow_convex_size; x++) {
            const std::uint32_t clamped_x = std::clamp(x, side_min, side_max);
            const std::uint32_t clamped_y = std::clamp(y, side_min, side_max);

            const float dist = (vec2(
                static_cast<float>(clamped_x),
                static_cast<float>(clamped_y)
            ) - target_point).length();

            float alpha = calc_shadow(dist) * scale;
            data[x + (y * shadow_convex_size)] =
                static_cast<std::uint8_t>(std::clamp(alpha * 255.f, 0.f, 255.f));
        }
    }

    write_data_init(*rect_id, data.data(), data.size());

    vec2 min, max;
    get_rect_pos(*rect_id, min, max);

    min += vec2(static_cast<float>(k_padding));
    max -= vec2(static_cast<float>(k_padding));

    renderer_->shared_data_.shadow_positions = vec4(
        min.x, min.y,
        max.x, max.y
    );

    return true;
}

void font_atlas::grow_atlas()
{
    constexpr std::uint64_t k_grow_factor = 2;
    constexpr std::uint64_t k_alignment = 64;
    constexpr std::uint64_t k_min_size = 128;

    const auto current_area = static_cast<std::uint64_t>(width_) * static_cast<std::uint64_t>(height_);
    if (current_area > std::numeric_limits<std::uint64_t>::max() / k_grow_factor) {
        return; // 64 bit integer overflow
    }

    const auto new_area = current_area * k_grow_factor;
    const auto new_dim = static_cast<std::uint64_t>(
        std::ceil(std::sqrt(static_cast<double>(new_area)))
    );

    // align up to k_alignment
    const auto new_dim_aligned = std::max(
        ((new_dim + k_alignment - 1) / k_alignment) * k_alignment,
        k_min_size
    );
    if (new_dim_aligned > std::numeric_limits<std::uint32_t>::max()) {
        return; // 32 bit integer overflow
    }

    if (!resize_queued_) {
        last_height_ = height_;
        last_width_ = width_;
    }
    width_ = std::min(
        static_cast<std::uint32_t>(new_dim_aligned),
        max_width_
    );
    height_ = std::min(
        static_cast<std::uint32_t>(new_dim_aligned),
        max_height_
    );
    if (width_ == last_width_ &&
        height_ == last_height_) {
        return;
    }

    resize_queued_ = true;

    if (in_init_) {
        data32_.resize(width_ * height_);
    }
}

void font_atlas::update_cached_uvs()
{
    const auto tex_mult = vec4(1.f) / vec4(
        static_cast<float>(width_), static_cast<float>(height_),
        static_cast<float>(width_), static_cast<float>(height_)
    );

    renderer_->shared_data_.uv_white_px = renderer_->shared_data_.pos_white_px *
        vec2(tex_mult.x, tex_mult.y);

    renderer_->shared_data_.shadow_uvs = renderer_->shared_data_.shadow_positions * tex_mult;

    for (std::size_t i = 0; i < shared_data::k_baked_lines_max_width + 1u; i++) {
        renderer_->shared_data_.tex_uv_lines[i] = renderer_->shared_data_.tex_pos_lines[i] * tex_mult;
    }
}

bool font_atlas::create_font_texture()
{
    assert(width_ > 0 &&
        height_ > 0);

    auto* render_data = renderer_->render_data_.get();
    auto* ctx = renderer_->context();

    texture_desc d{};
    d.width = width_;
    d.height = height_;
    d.usage = texture_usage::shader_resource | texture_usage::render_target;
    d.format = texture_format::rgba8_unorm;

    render_data->font_texture = ctx->create_texture2d(
        d,
        data32_.empty() ? nullptr : data32_.data()
    );
    if (render_data->font_texture->has_error()) {
        return false;
    }

    textureview_desc vd{};
    vd.usage = view_usage::shader_resource | view_usage::render_target;
    render_data->font_view = ctx->create_textureview(
        render_data->font_texture.get(),
        vd
    );
    if (render_data->font_view->has_error()) {
        return false;
    }

    framebuffer_desc fd{};
    fd.color_attachment.view = render_data->font_view.get();

    render_data->font_fbo = ctx->create_framebuffer(
        fd
    );
    if (render_data->font_fbo->has_error()) {
        return false;
    }

    data32_.clear();
    data32_.shrink_to_fit();

    return true;
}

std::optional<std::uint32_t> font_atlas::register_rect(std::uint32_t width, std::uint32_t height)
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    std::uint32_t x, y;
    if (!find_rect(width, height, x, y)) {
        const auto last_width = width_;
        const auto last_height = height_;
        const auto queue_resize = resize_queued_;
        grow_atlas();

        if (!find_rect(width, height, x, y)) {
            /// undo grow
            // we dont use member variables here,
            // because its possible for a resize to already be queued before this function got called
            width_ = last_width;
            height_ = last_height;
            resize_queued_ = queue_resize;

            return std::nullopt;
        }
    }

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

    // make "invisible" to "find_rect"
    rects_[id].pos_x = 0;
    rects_[id].pos_y = 0;
    rects_[id].width = 0;
    rects_[id].height = 0;

    free_rect_slots_.push_back(id);
}

void font_atlas::get_rect_pos(std::uint32_t id, vec2& min, vec2& max) const
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    assert(id < rects_.size());

    auto& r = rects_[id];

    min.x = static_cast<float>(r.pos_x);
    min.y = static_cast<float>(r.pos_y);

    max.x = min.x + static_cast<float>(r.width);
    max.y = min.y + static_cast<float>(r.height);
}

void font_atlas::get_rect_uv(std::uint32_t id, vec2& uv_min, vec2& uv_max) const
{
    get_rect_pos(id, uv_min, uv_max);

    const auto m = vec2(1.f) / vec2(static_cast<float>(width_), static_cast<float>(height_));
    uv_min *= m;
    uv_max *= m;
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
    if (temp_upload_.size() < size) {
        temp_upload_.resize(size);
    }

    auto* dst = temp_upload_.data();

    for (std::size_t i = 0u; i < size; ++i) {
        dst[i] = 0x00ffffffu |
            (static_cast<std::uint32_t>(data[i]) << 24u);
    }

    return write_data(id, dst, size);
}

void font_atlas::write_data(std::uint32_t id, const std::uint32_t* data, std::size_t size)
{
    if (resize_queued_) {
        std::vector<std::uint32_t> buf;
        buf.assign(data, data + size);
        write_data(id, std::move(buf));
        return;
    }

    assert(!resize_queued_);
    assert(renderer_->render_data());
    assert(renderer_->render_data()->font_texture);

    atlas_rect r = get_rect(id);
    assert(r.width * r.height == size);
    (void)size;

    assert(r.pos_x >= padding_ && r.pos_y >= padding_);
    assert(r.pos_x + r.width + padding_ <= width_);
    assert(r.pos_y + r.height + padding_ <= height_);

    const std::uint32_t upload_width = r.width + padding_ * 2u;
    const std::uint32_t upload_height = r.height + padding_ * 2u;
    const std::size_t upload_size =
        static_cast<std::size_t>(upload_width) * static_cast<std::size_t>(upload_height);

    constexpr std::uint32_t k_transparent_white = 0x00ffffffu;
    temp_upload_padding_.assign(upload_size, k_transparent_white);

    for (std::uint32_t y = 0u; y < r.height; ++y) {
        const auto* src_row = data + static_cast<std::size_t>(y) * r.width;
        auto* dst_row = temp_upload_padding_.data() +
            static_cast<std::size_t>(y + padding_) * upload_width + padding_;

        std::memcpy(
            dst_row,
            src_row,
            static_cast<std::size_t>(r.width) * sizeof(std::uint32_t)
        );
    }

    renderer_->render_data()->font_texture->update(
        temp_upload_padding_.data(),
        upload_width * sizeof(std::uint32_t),
        r.pos_x - padding_,
        r.pos_y - padding_,
        upload_width,
        upload_height
    );
}

void font_atlas::write_data(std::uint32_t id, std::vector<std::uint32_t> data)
{
    if (resize_queued_) {
        rect_writes_.emplace_back(id, std::move(data));
        return;
    }

    return write_data(id, data.data(), data.size());
}

void font_atlas::write_data_init(std::uint32_t id, const std::uint8_t* data, std::size_t size)
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
            const std::uint8_t byte = src_row[x];
            constexpr std::uint8_t white = 0xffu;
            dst_row[x] = white | (white << 8) | (white << 16) | (byte << 24);
        }
    }
}

bool font_atlas::update_resize()
{
#if defined(_DEBUG)
    renderer_->assert_render_thread();
#endif

    if (resize_queued_) {
        resize_queued_ = false;

        auto* render_data = renderer_->render_data_.get();
        auto* ctx = renderer_->context();

        auto old_texture = std::move(render_data->font_texture);
        auto old_view = std::move(render_data->font_view);
        auto old_fbo = std::move(render_data->font_fbo);

        if (!create_font_texture()) {
            return false;
        }

        if (old_fbo) {
            ctx->clear_framebuffer(render_data->font_fbo.get());
        }

        // copy old atlas onto new atlas
        if (last_width_ > 0 &&
            last_height_ > 0) {
            assert(old_fbo);
            const auto r = rect(
                0, 0, // left, top
                last_width_, last_height_
            );

            ctx->copy_subresource(
                render_data->font_fbo.get(),
                old_fbo.get(),
                r,
                r
            );
        }

        update_cached_uvs();
    }

    // update pending writes
    for (auto& w : rect_writes_) {
        write_data(
            w.rect_id,
            std::move(w.data)
        );
    }

    rect_writes_.clear();

    return true;
}

bool font_atlas::build()
{
    in_init_ = true;
    data32_.resize(width_ * height_);

    if (!add_white_pixel()) {
        return false;
    }

    if (!add_tex_lines()) {
        return false;
    }

    if (!add_shadow_tex()) {
        return false;
    }

    update_cached_uvs();
    in_init_ = false;

    return true;
}

r2_end_