#include <r2/font/font.h>
#include <r2/font/unicode.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#include "font_atlas.h"
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
        if (pg.has_value())
            local_completed.push_back(
                std::move(pg.value()));
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
        if (!ret.has_value())
            continue;

        apply_glyph(*ret);
        private_buffer_.swap(ret->bitmap);
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

        for (auto& data : fonts_) {
            const int glyph_index = stbtt_FindGlyphIndex(
                data.font_info.get(),
                static_cast<int>(cp)
            );
            if (glyph_index != 0) {
                l.supported = true;
                if (has_blur) {
                    glyph_lookup_blurred_[cp].supported = true;
                }
                break;
            }
        }
    }

    // fallback glyph
    fallback_glyph_ = find_glyph('?');

    return true;
}

bool font::add_font(const std::uint8_t* data, std::size_t data_size)
{
    fonts_.emplace_back(data, data_size, std::make_unique<stbtt_fontinfo>());

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

    if (!pg.visible) {
        lookup.loading = 0u;
        return;
    }

    font_glyph g{};
    g.codepoint = pg.codepoint;
    g.visible = true;
    g.advance_x = pg.advance_x;
    g.x0 = pg.x0;
    g.y0 = pg.y0;
    g.x1 = pg.x1;
    g.y1 = pg.y1;
    g.last_access = frame_start_;
    g.blurred = pg.blurred;

    g.rect_id = atlas_->register_rect(pg.bmp_w, pg.bmp_h);
    atlas_->get_rect_uv(g.rect_id, g.uv_min, g.uv_max);
    atlas_->write_data(g.rect_id, pg.bitmap.data(), pg.bitmap.size());

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
                 k <= static_cast<std::int32_t>(cfg_.glow_radius); ++k)
            {
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
                 k <= static_cast<std::int32_t>(cfg_.glow_radius); ++k)
            {
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

std::optional<pending_glyph> font::rasterize_glyph(wchar c, font_data* data, bool blurred)
{
    assert(!blurred || cfg_.glow_radius > 0u);
    assert(cfg_.oversample_h > 1u);
    assert(cfg_.oversample_v > 1u);

    pending_glyph e{};
    e.codepoint = c;
    e.blurred = blurred;

    stbtt_fontinfo* info = nullptr;
    if (data != nullptr) {
        info = data->font_info.get();
    }
    else {
        for (auto& d : fonts_) {
            const int glyph_index = stbtt_FindGlyphIndex(
                d.font_info.get(), 
                static_cast<int>(c)
            );
            if (glyph_index != 0) {
                info = d.font_info.get();
                break;
            }
        }
    }
    if (info == nullptr) {
        return std::nullopt;
    }

    const int glyph_index = stbtt_FindGlyphIndex(
        info, 
        static_cast<int>(c)
    );
    if (glyph_index == 0) {
        return std::nullopt;
    }

    const float scale = stbtt_ScaleForPixelHeight(
        info, 
        static_cast<float>(cfg_.size)
    );

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
    const float ascent = std::round(
        (float)unscaled_ascent * scale + ((unscaled_ascent > 0) ? +1.f : -1.f)
    );

    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBoxSubpixel(
        info,
        glyph_index,
        scale,
        scale,
        0.f, 0.f, /* shift */
        &x0, &y0, &x1, &y1
    );

    const int over_w = x1 - x0;
    const int over_h = y1 - y0;
    const int blur_size = blurred ?
        static_cast<int>(cfg_.glow_radius) : 0;
    const int width = over_w + (cfg_.oversample_h - 1) + blur_size * 2;
    const int height = over_h + (cfg_.oversample_v - 1) + blur_size * 2;

    if (width <= 0 || height <= 0) {
        e.visible = false;
        return e;
    }

    e.x0 = static_cast<float>(x0);
    e.x1 = static_cast<float>(x1);
    e.y0 = static_cast<float>(y0) + ascent;
    e.y1 = static_cast<float>(y1) + ascent;

    private_buffer_.clear();
    private_buffer_.resize(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height));

    std::uint8_t* const pixel_data = private_buffer_.data() + blur_size * width + blur_size;

    if (cfg_.oversample_h == 1u && cfg_.oversample_v == 1u) {
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
        float sub_x = 0.f;
        float sub_y = 0.f;

        stbtt_MakeGlyphBitmapSubpixelPrefilter(
            info,
            pixel_data,
            width - blur_size * 2, height - blur_size * 2,
            width * sizeof(std::uint8_t), /* stride */
            scale, scale,
            0.f, 0.f, /* shift */
            static_cast<int>(cfg_.oversample_h), 
            static_cast<int>(cfg_.oversample_v),
            &sub_x, &sub_y,
            glyph_index
        );
        e.x0 += sub_x / static_cast<float>(cfg_.oversample_h);
        e.y0 += sub_y / static_cast<float>(cfg_.oversample_v);
        e.x1 += sub_x / static_cast<float>(cfg_.oversample_h);
        e.y1 += sub_y / static_cast<float>(cfg_.oversample_v);
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