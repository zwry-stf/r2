#include <r2/font/font.h>
#include <r2/font/unicode.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include <r2/font/font_atlas.h>
#include <algorithm>
#include <limits>


r2_begin_

font::font(font_atlas* atlas, const font_cfg& cfg)
    : atlas_(atlas), 
      cfg_(cfg)
{
}

font::~font() = default;

constexpr wchar kDefaultGlyphsStart = 0x20u;
constexpr wchar kDefaultGlyphsEnd = 0x7E;

bool font::update_on_render()
{
    frame_start_ += 1u;

    std::vector<pending_glyph> local_completed;
    {
        std::lock_guard<std::mutex> lock(completed_mutex_);
        local_completed.swap(completed_glyphs_);
    }

    bool atlas_changed = !local_completed.empty();

    for (auto& pg : local_completed) {
        if (pg.failed) {
            // remove supported flag
            (pg.blurred ?
                glyph_lookup_blurred_[pg.codepoint].supported :
                glyph_lookup_[pg.codepoint].supported) = 0u;
            continue;
        }
        apply_glyph(std::move(pg));
    }

    // clean unused rects
    constexpr auto kCleanupTime = std::chrono::seconds{ 10 };
    const auto now = std::chrono::steady_clock::now();
    if (now - last_cleanup_ > kCleanupTime) {
        constexpr std::uint64_t kRemoveAge = 100000u;

        for (std::uint32_t idx = 0u; idx < glyphs_.size(); ++idx) {
            auto& g = glyphs_[idx];
            if (!g.visible)
                continue;

            if (frame_start_ > g.last_access &&
                frame_start_ - g.last_access > kRemoveAge) {
                if (g.codepoint > kDefaultGlyphsStart && 
                    g.codepoint <= kDefaultGlyphsEnd)
                    continue;

                atlas_->remove_rect(g.rect_id);

                auto& e = g.blurred ? glyph_lookup_blurred_[g.codepoint] : glyph_lookup_[g.codepoint];
                e.index = glyph_lookup_data::kInvalidIndex;
                e.loading = 0u;

                g.visible = false;
                free_glyph_slots_.push_back(idx);
                atlas_changed = true;
            }
        }

        last_cleanup_ = now;
    }

    return atlas_changed;
}

void font::update_worker()
{
    std::vector<glyph_queue> local_requests;
    {
        std::lock_guard<std::mutex> lock(request_mutex_);
        local_requests.swap(glyphs_to_rasterize_);
    }

    if (local_requests.empty())
        return;

    std::vector<pending_glyph> local_completed;
    local_completed.reserve(local_requests.size());

    for (const auto& cp : local_requests) {
        auto pg = rasterize_glyph(cp.codepoint, nullptr, cp.blurred);
        local_completed.push_back(
            std::move(pg));
    }

    {
        std::lock_guard<std::mutex> lock(completed_mutex_);
        for (auto& g : local_completed)
            completed_glyphs_.push_back(std::move(g));
    }
}

bool font::build()
{
    assert(!fonts_.empty());
    assert(frame_start_ == 0u);

    const bool has_blur = cfg_.glow_radius > 0u;

    glyph_lookup_.resize(unicode::codepoint_max);
    if (has_blur) {
        glyph_lookup_blurred_.resize(unicode::codepoint_max);
        build_weights();
    }

    for (wchar i = kDefaultGlyphsStart; i <= kDefaultGlyphsEnd; i++) {
        auto ret = rasterize_glyph(i, nullptr, false);
        if (ret.failed)
            continue;

        apply_glyph(ret);
        private_buffer_.swap(ret.bitmap);
    } 

    // build lookup table
    for (std::size_t i = 0u; i < glyphs_.size(); i++) {
        auto& g = glyphs_[i];
        if (static_cast<std::size_t>(g.codepoint) < glyph_lookup_.size()) {
            auto& e = glyph_lookup_[g.codepoint];
            e.index = static_cast<std::uint32_t>(i);
        }
    }

    // mark supported glyphs
    for (std::uint32_t cp = kDefaultGlyphsStart; cp < unicode::codepoint_max; ++cp) {
        auto& l = glyph_lookup_[cp];
        if (cp >= 0xD800u && cp <= 0xDFFFu) {
            continue;
        }

        if (get_font_data_for_char(cp) != nullptr) {
            l.supported = true;
            if (has_blur) {
                glyph_lookup_blurred_[cp].supported = true;
            }
        }
    }

    // fallback glyph
    fallback_glyph_ = find_glyph('?');

    return true;
}

void font::destroy()
{
    for (auto& glyph : glyphs_) {
        if (glyph.visible) {
            atlas_->remove_rect(glyph.rect_id);
        }
    }
    glyphs_.clear();
    glyph_lookup_.clear();
    glyph_lookup_blurred_.clear();
}

bool font::add_font(const std::uint8_t* data, std::size_t data_size)
{
    std::vector<font_range> ranges;
    ranges.emplace_back(wchar(0), unicode::codepoint_max);

    return add_font(data, data_size, std::move(ranges));
}

bool font::add_font(const std::uint8_t* data, std::size_t data_size, const std::vector<font_range>& ranges)
{
    std::vector<font_range> copy = ranges;
    return add_font(data, data_size, std::move(copy));
}

bool font::add_font(const std::uint8_t* data, std::size_t data_size, std::vector<font_range>&& ranges)
{
    fonts_.emplace_back(
        data,
        data_size,
        std::move(ranges),
        std::make_unique<stbtt_fontinfo>()
    );

    const int ok = stbtt_InitFont(
        fonts_.back().font_info.get(),
        data,
        0
    );
    if (ok == 0)
        return false;

    return true;
}

void font::apply_glyph(const pending_glyph& pg)
{
    const std::size_t lookup_size = (pg.blurred ? glyph_lookup_blurred_.size() : glyph_lookup_.size());
    assert(lookup_size == unicode::codepoint_max);
    if (static_cast<std::size_t>(pg.codepoint) >=
        lookup_size)
        return;

    auto& lookup = pg.blurred ?
        glyph_lookup_blurred_[pg.codepoint] : glyph_lookup_[pg.codepoint];

    font_glyph g{};
    g.codepoint = pg.codepoint;
    g.visible = pg.visible;
    g.advance_x = pg.advance_x;
    g.x0 = pg.x0;
    g.y0 = pg.y0;
    g.x1 = pg.x1;
    g.y1 = pg.y1;
    g.last_access = frame_start_;
    g.blurred = pg.blurred;

    if (pg.visible) {
        g.rect_id = atlas_->register_rect(pg.bmp_w, pg.bmp_h);
        atlas_->get_rect_uv(g.rect_id, g.uv_min, g.uv_max);
        atlas_->write_data(g.rect_id, pg.bitmap.data(), pg.bitmap.size());
    }

    std::uint32_t slot = 0u;
    if (!free_glyph_slots_.empty()) {
        slot = free_glyph_slots_.back();
        free_glyph_slots_.pop_back();
        glyphs_[slot] = std::move(g);
    } else {
        slot = static_cast<std::uint32_t>(glyphs_.size());
        glyphs_.push_back(std::move(g));
    }

    lookup.index = slot;
    lookup.loading = 0u;
}

void font::build_weights()
{
    kernel_weights_.resize(cfg_.glow_radius + 1u);

    const float sigma = std::max(1.f, cfg_.glow_radius / 3.f);
    const float two_sigma2 = 2.f * sigma * sigma;

    float sum = 0.f;
    for (std::uint32_t i = 0; i <= cfg_.glow_radius; ++i) {
        const float t = static_cast<float>(i);
        const float wgt = std::exp(-(t * t) / two_sigma2);
        kernel_weights_[i] = wgt;
        sum += (i == 0) ? wgt : (2.f * wgt);
    }

    const float inv_sum = 1.f / sum;
    for (std::uint32_t i = 0; i <= cfg_.glow_radius; ++i)
        kernel_weights_[i] *= inv_sum;
}

void font::blur_rect(std::uint32_t w, std::uint32_t h)
{
    assert(cfg_.glow_radius > 0u);

    const std::uint32_t stride = w;

    std::vector<float>& tmp_h = private_buffer3_;
    std::vector<float>& tmp_v = private_buffer4_;

    tmp_h.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    tmp_v.resize(static_cast<size_t>(w) * static_cast<size_t>(h));

    for (std::uint32_t row = 0; row < h; ++row) {
        const std::uint32_t iy = row;
        const std::uint8_t* src_row = private_buffer_.data() + static_cast<size_t>(iy) * stride;

        for (std::uint32_t col = 0; col < w; ++col) {
            float acc = 0.f;

            for (std::int32_t k = -static_cast<std::int32_t>(cfg_.glow_radius);
                 k <= static_cast<std::int32_t>(cfg_.glow_radius); ++k) {
                std::int32_t sx = static_cast<std::int32_t>(col) + k;
                sx = std::clamp(sx, 0, static_cast<std::int32_t>(w) - 1);

                const float wgt = kernel_weights_[static_cast<std::uint32_t>(std::abs(k))];
                acc += static_cast<float>(src_row[sx]) * wgt;
            }

            tmp_h[static_cast<size_t>(row) * w + col] = acc;
        }
    }

    for (std::uint32_t col = 0; col < w; ++col) {
        for (std::uint32_t row = 0; row < h; ++row) {
            float acc = 0.f;

            for (std::int32_t k = -static_cast<std::int32_t>(cfg_.glow_radius);
                 k <= static_cast<std::int32_t>(cfg_.glow_radius); ++k) {
                std::int32_t ry = static_cast<std::int32_t>(row) + k;
                ry = std::clamp(ry, 0, static_cast<std::int32_t>(h) - 1);

                const float wgt = kernel_weights_[static_cast<std::uint32_t>(std::abs(k))];
                acc += tmp_h[static_cast<size_t>(ry) * w + col] * wgt;
            }

            tmp_v[static_cast<size_t>(row) * w + col] = acc;
        }
    }

    for (std::uint32_t row = 0; row < h; ++row) {
        std::uint8_t* dst_row = private_buffer_.data() + static_cast<size_t>(row) * stride;

        for (std::uint32_t col = 0; col < w; ++col) {
            const float v = tmp_v[static_cast<size_t>(row) * w + col];
            const int iv = std::clamp(static_cast<int>(v + 0.5f),
                0, static_cast<int>(std::numeric_limits<std::uint8_t>::max()));
            dst_row[col] = static_cast<std::uint8_t>(iv);
        }
    }
}

void font::glow_rect(std::uint32_t w, std::uint32_t h)
{
    const std::uint32_t stride = w;

    private_buffer2_.resize(
        static_cast<std::size_t>(w) * static_cast<std::size_t>(h)
    );
    assert(private_buffer_.size() == private_buffer2_.size());
    for (std::uint32_t row = 0; row < h; ++row) {
        const std::uint8_t* src = private_buffer_.data() + static_cast<size_t>(row) * stride;
        std::memcpy(private_buffer2_.data() + static_cast<size_t>(row) * w, src, w);
    }

    blur_rect(w, h);

    for (std::uint32_t row = 0; row < h; ++row) {
        std::uint8_t* dst = private_buffer_.data() + static_cast<size_t>(row) * stride;
        const std::uint8_t* core_row = private_buffer2_.data() + static_cast<size_t>(row) * w;

        for (std::uint32_t col = 0; col < w; ++col) {
            const int core_v = core_row[col];
            const int halo_v = static_cast<int>(dst[col] * cfg_.glow_strength + 0.5f);

            const int out_v = std::clamp(std::max(core_v, halo_v),
                0, static_cast<int>(std::numeric_limits<std::uint8_t>::max()));
            dst[col] = static_cast<std::uint8_t>(out_v);
        }
    }
}

stbtt_fontinfo* font::get_font_data_for_char(wchar c) const noexcept
{
    for (auto& d : fonts_) {
        bool in_range = false;
        for (auto& r : d.ranges) {
            if (c >= r.range_min &&
                c <= r.range_max) {
                in_range = true;
                break;
            }
        }
        if (!in_range)
            continue;

        const int glyph_index = stbtt_FindGlyphIndex(
            d.font_info.get(),
            static_cast<int>(c)
        );
        if (glyph_index != 0) {
            return d.font_info.get();
        }
    }

    return nullptr;
}

inline int floor_div(int a, int b) {
    if (a >= 0) 
        return a / b;
    return -(((-a) + b - 1) / b);
}

inline int ceil_div(int a, int b) {
    if (a >= 0) 
        return (a + b - 1) / b;
    return -((-a) / b);
}

void box_downsample_u8(const std::uint8_t* src, int src_w, int src_h, int src_stride,
                       std::uint8_t* dst, int dst_w, int dst_h, int dst_stride,
                       int factor_x, int factor_y,
                       int dx, int dy) {
    const int denom = factor_x * factor_y;

    for (int y = 0; y < dst_h; ++y) {
        const int sy0 = dy + y * factor_y;

        for (int x = 0; x < dst_w; ++x) {
            const int sx0 = dx + x * factor_x;

            std::uint32_t sum = 0;
            for (int yy = 0; yy < factor_y; ++yy) {
                const int sy = sy0 + yy;
                if ((unsigned)sy >= (unsigned)src_h) continue;

                const std::uint8_t* row = src + sy * src_stride;

                for (int xx = 0; xx < factor_x; ++xx) {
                    const int sx = sx0 + xx;
                    if ((unsigned)sx >= (unsigned)src_w) continue;

                    sum += row[sx];
                }
            }

            dst[y * dst_stride + x] = static_cast<std::uint8_t>(sum / denom);
        }
    }
}

pending_glyph font::rasterize_glyph(wchar c, font_data* data, bool blurred)
{
    assert(!blurred || cfg_.glow_radius > 0u);
    assert(cfg_.oversample_h >= 1u);
    assert(cfg_.oversample_v >= 1u);

    pending_glyph e{};
    e.failed = false;
    e.codepoint = c;
    e.blurred = blurred;

    stbtt_fontinfo* info = nullptr;
    if (data != nullptr) {
        info = data->font_info.get();
    }
    else {
        info = get_font_data_for_char(c);
    }
    if (info == nullptr) {
        e.failed = true;
        return e;
    }

    const int glyph_index = stbtt_FindGlyphIndex(
        info, 
        static_cast<int>(c)
    );
    if (glyph_index == 0) {
        e.failed = true;
        return e;
    }

    const float scale = stbtt_ScaleForPixelHeight(
        info, 
        static_cast<float>(cfg_.size)
    );
    const float scale_x = scale * static_cast<float>(cfg_.oversample_h);
    const float scale_y = scale * static_cast<float>(cfg_.oversample_v);

    int adv, lsb;
    stbtt_GetGlyphHMetrics(
        info, 
        glyph_index, 
        &adv, 
        &lsb
    );
    e.advance_x = static_cast<float>(adv) * scale;

    int unscaled_ascent, unscaled_descent, unscaled_line_gap;
    stbtt_GetFontVMetrics(
        info,
        &unscaled_ascent,
        &unscaled_descent,
        &unscaled_line_gap
    );
    const float ascent_f = static_cast<float>(unscaled_ascent) * scale;
    const float baseline_px = std::ceil(ascent_f);

    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBoxSubpixel(
        info,
        glyph_index,
        scale, scale,
        0.f, 0.f, /* shift */
        &x0, &y0, &x1, &y1
    );

    const int over_w = x1 - x0;
    const int over_h = y1 - y0;
    const int blur_size = blurred ?
        static_cast<int>(cfg_.glow_radius) : 0;
    const int width = over_w + blur_size * 2;
    const int height = over_h + blur_size * 2;

    if (width <= 0 || height <= 0) {
        e.visible = false;
        return e;
    }

    e.x0 = static_cast<float>(x0);
    e.x1 = static_cast<float>(x1);
    e.y0 = static_cast<float>(y0) + baseline_px;
    e.y1 = static_cast<float>(y1) + baseline_px;

    if (cfg_.oversample_h == 1u && cfg_.oversample_v == 1u) {
        private_buffer_.assign(
            static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
            0u
        );

        std::uint8_t* const pixel_data = private_buffer_.data() + blur_size * width + blur_size;

        stbtt_MakeGlyphBitmapSubpixel(
            info,
            pixel_data,
            width - blur_size * 2, height - blur_size * 2,
            width * sizeof(std::uint8_t), /* stride */
            scale, scale,
            0.f, 0.f, /* shift */
            glyph_index
        );
    }
    else {
        const int oh = static_cast<int>(cfg_.oversample_h);
        const int ov = static_cast<int>(cfg_.oversample_v);

        int x0o, y0o, x1o, y1o;
        stbtt_GetGlyphBitmapBoxSubpixel(
            info, glyph_index,
            scale_x, scale_y,
            0.f, 0.f,
            &x0o, &y0o, &x1o, &y1o
        );

        const int src_w = x1o - x0o;
        const int src_h = y1o - y0o;

        if (src_w <= 0 || src_h <= 0) {
            e.visible = false;
            return e;
        }

        private_buffer2_.assign(
            static_cast<std::size_t>(src_w) * static_cast<std::size_t>(src_h),
            0u
        );

        stbtt_MakeGlyphBitmapSubpixel(
            info,
            private_buffer2_.data(),
            src_w, src_h,
            src_w * static_cast<int>(sizeof(std::uint8_t)),
            scale_x, scale_y,
            0.f, 0.f,
            glyph_index
        );

        const int x0b = floor_div(x0o, oh);
        const int y0b = floor_div(y0o, ov);
        const int x1b = ceil_div(x1o, oh);
        const int y1b = ceil_div(y1o, ov);

        const int over_w2 = x1b - x0b;
        const int over_h2 = y1b - y0b;

        const int width2 = over_w2 + blur_size * 2;
        const int height2 = over_h2 + blur_size * 2;

        if (width2 <= 0 || height2 <= 0) {
            e.visible = false;
            return e;
        }

        x0 = x0b; y0 = y0b; x1 = x1b; y1 = y1b;

        e.x0 = static_cast<float>(x0);
        e.x1 = static_cast<float>(x1);
        e.y0 = static_cast<float>(y0) + baseline_px;
        e.y1 = static_cast<float>(y1) + baseline_px;

        private_buffer_.assign(
            static_cast<std::size_t>(width2) * static_cast<std::size_t>(height2), 
            0u
        );
        std::uint8_t* const pixel_data = private_buffer_.data() + blur_size * width2 + blur_size;

        const int dx = (x0b * oh) - x0o;
        const int dy = (y0b * ov) - y0o;

        box_downsample_u8(
            private_buffer2_.data(), src_w, src_h, src_w,
            pixel_data, over_w2, over_h2, width2,
            oh, ov,
            dx, dy
        );
    }

    if (blurred) {
        assert(cfg_.glow_strength > 0.f);
        glow_rect(
            width, height
        );
    }

    e.x0 += static_cast<float>(cfg_.offset_x);
    e.x1 += static_cast<float>(cfg_.offset_x);
    e.y0 += static_cast<float>(cfg_.offset_y);
    e.y1 += static_cast<float>(cfg_.offset_y);

    if (blurred) {
        e.x0 -= static_cast<float>(blur_size);
        e.y0 -= static_cast<float>(blur_size);
        e.x1 += static_cast<float>(blur_size);
        e.y1 += static_cast<float>(blur_size);
    }

    e.bmp_w = static_cast<std::uint32_t>(width);
    e.bmp_h = static_cast<std::uint32_t>(height);
    private_buffer_.swap(e.bitmap);

    e.visible = true;

    return e;
}

r2_end_