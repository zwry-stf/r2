#include "font.h"
#pragma once


r2_begin_

inline const font_glyph* font::find_glyph(wchar c)
{
    if (c >= static_cast<wchar>(glyph_lookup_.size()))
        return fallback_glyph_;
    auto& i = glyph_lookup_[c];
    if (i.index == glyph_lookup_data::kInvalidIndex) {
        if (i.supported == 1u) {
            // queue for load
            if (i.loading == 0u) {
                i.loading = 1u;
                std::lock_guard<std::mutex> lock(request_mutex_);
                glyphs_to_rasterize_.emplace_back(c, false);
            }
        }
        return fallback_glyph_;
    }
    glyphs_[i.index].last_access = frame_start_;
    assert(!glyphs_[i.index].blurred);
    return &glyphs_[i.index];
}

inline const font_glyph* font::find_glyph_blurred(wchar c)
{
    if (c >= static_cast<wchar>(glyph_lookup_blurred_.size()))
        return fallback_glyph_;
    auto& i = glyph_lookup_blurred_[c];
    if (i.index == glyph_lookup_data::kInvalidIndex) {
        if (i.supported == 1u) {
            // queue for load
            if (i.loading == 0u) {
                i.loading = 1u;
                std::lock_guard<std::mutex> lock(request_mutex_);
                glyphs_to_rasterize_.emplace_back(c, true);
            }
        }
        return fallback_glyph_;
    }
    glyphs_[i.index].last_access = frame_start_;
    assert(glyphs_[i.index].blurred);
    return &glyphs_[i.index];
}

inline const font_glyph* font::find_glyph_no_fallback(wchar c)
{
    if (c >= static_cast<wchar>(glyph_lookup_.size()))
        return nullptr;
    auto& i = glyph_lookup_[c];
    if (i.index == glyph_lookup_data::kInvalidIndex) {
        if (i.supported == 1u) {
            // queue for load
            if (i.loading == 0u) {
                i.loading = 1u;
                std::lock_guard<std::mutex> lock(request_mutex_);
                glyphs_to_rasterize_.emplace_back(c, false);
            }
        }
        return nullptr;
    }
    glyphs_[i.index].last_access = frame_start_;
    assert(!glyphs_[i.index].blurred);
    return &glyphs_[i.index];
}

inline const font_cfg& font::cfg() const
{
    return cfg_;
}

r2_end_