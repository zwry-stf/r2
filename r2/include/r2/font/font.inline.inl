#include "font.h"
#pragma once


r2_begin_

template <unicode::string_like String>
inline unicode::unicode_type font::next_char(const String& text, std::uint32_t length, std::uint32_t& pos)
{
    const auto cp = unicode::get_char_auto(text, length, pos);
    if ((cp >= 0xFE00u && cp <= 0xFE0Fu) || cp == 0x200Du ||
        (cp >= 0xE0020u && cp <= 0xE007Fu) || (cp >= 0xE0100u && cp <= 0xE01EFu)) {
        return U'\r';
    }
    if (cp >= color_characters_.size() || !color_characters_[cp]) {
        return cp;
    }

    std::array<char32_t, 32> codepoints{};
    std::array<std::uint32_t, 32> positions{};
    codepoints[0] = cp;
    positions[0] = pos;
    std::uint32_t count = 1u;
    auto next = pos;
    while (next < length && count < codepoints.size()) {
        const auto c = unicode::get_char_auto(text, length, next);
        if (c == 0xFE0Eu && count == 1u) {
            pos = next;
            return cp;
        }
        if (c != 0x200Du && c != 0xFE0Fu && c != 0x20E3u &&
            !(c >= 0xE0020u && c <= 0xE007Fu) &&
            (c >= color_characters_.size() || !color_characters_[c])) {
            break;
        }
        codepoints[count] = c;
        positions[count++] = next;
    }
    const auto result = shape_char(std::span(codepoints.data(), count));
    pos = positions[result.count - 1u];
    return result.codepoint;
}

inline const font_glyph* font::find_glyph(wchar c)
{
    if (c >= static_cast<wchar>(glyph_lookup_.size())) {
        return &glyphs_[fallback_glyph_];
    }
    auto& i = glyph_lookup_[c];
    if (i.index == glyph_lookup_data::k_invalid_index) [[unlikely]] {
        if (i.supported == 1u) {
            // queue for load
            if (i.loading == 0u) {
                i.loading = 1u;
                queue_glyph(c, false);
            }
        }
        return &glyphs_[fallback_glyph_];
    }
    glyphs_[i.index].last_access = frame_start_;
    assert(!glyphs_[i.index].blurred);
    return &glyphs_[i.index];
}

inline const font_glyph* font::find_glyph_blurred(wchar c)
{
    if (c >= static_cast<wchar>(glyph_lookup_blurred_.size())) {
        return &glyphs_[fallback_glyph_];
    }
    auto& i = glyph_lookup_blurred_[c];
    if (i.index == glyph_lookup_data::k_invalid_index) [[unlikely]] {
        if (i.supported == 1u) {
            // queue for load
            if (i.loading == 0u) {
                i.loading = 1u;
                queue_glyph(c, true);
            }
        }
        return &glyphs_[fallback_glyph_];
    }
    glyphs_[i.index].last_access = frame_start_;
    assert(glyphs_[i.index].blurred);
    return &glyphs_[i.index];
}

inline const font_glyph* font::find_glyph_no_fallback(wchar c)
{
    if (c >= static_cast<wchar>(glyph_lookup_.size())) {
        return nullptr;
    }
    auto& i = glyph_lookup_[c];
    if (i.index == glyph_lookup_data::k_invalid_index) [[unlikely]] {
        if (i.supported == 1u) {
            // queue for load
            if (i.loading == 0u) {
                i.loading = 1u;
                queue_glyph(c, false);
            }
        }
        return nullptr;
    }
    glyphs_[i.index].last_access = frame_start_;
    assert(!glyphs_[i.index].blurred);
    return &glyphs_[i.index];
}

r2_end_