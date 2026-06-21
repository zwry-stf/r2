#pragma once
#include "drawlist_base.h"
#include <r2/font/font.h>

#include <algorithm>


r2_begin_


inline void drawlist_base::set_clip_rect(const rect& r)
{
    on_changed_header(r, &draw_cmd::clip_rect);
}

inline void drawlist_base::push_clip_rect(const vec2& min, const vec2& max, bool intersect_current)
{
    push_clip_rect({
        static_cast<std::int32_t>(min.x),
        static_cast<std::int32_t>(min.y),
        static_cast<std::int32_t>(max.x),
        static_cast<std::int32_t>(max.y),
        },
        intersect_current
    );
}

inline void drawlist_base::push_clip_rect(const rect& r, bool intersect_current)
{
    rect rect = r;
    if (intersect_current) {
        if (header_.clip_rect.left > rect.left)
            rect.left = header_.clip_rect.left;
        if (header_.clip_rect.right < rect.right)
            rect.right = header_.clip_rect.right;
        if (header_.clip_rect.top > rect.top)
            rect.top = header_.clip_rect.top;
        if (header_.clip_rect.bottom < rect.bottom)
            rect.bottom = header_.clip_rect.bottom;
    }

    clip_rect_stack_.push_back(rect);
    set_clip_rect(rect);
}

inline void drawlist_base::modify_clip_rect_x(std::int32_t min, std::int32_t max)
{
    rect rect = header_.clip_rect;
    if (min > header_.clip_rect.left)
        rect.left = min;
    if (max < header_.clip_rect.right)
        rect.right = max;

    clip_rect_stack_.push_back(rect);
    set_clip_rect(rect);
}

inline void drawlist_base::modify_clip_rect_x(float min, float max)
{
    modify_clip_rect_x(
        static_cast<std::int32_t>(min),
        static_cast<std::int32_t>(max)
    );
}

inline void drawlist_base::modify_clip_rect_y(std::int32_t min, std::int32_t max)
{
    rect rect = header_.clip_rect;
    if (min > header_.clip_rect.top)
        rect.top = min;
    if (max < header_.clip_rect.bottom)
        rect.bottom = max;

    clip_rect_stack_.push_back(rect);
    set_clip_rect(rect);
}

inline void drawlist_base::modify_clip_rect_y(float min, float max)
{
    modify_clip_rect_y(
        static_cast<std::int32_t>(min),
        static_cast<std::int32_t>(max)
    );
}

inline void drawlist_base::pop_clip_rect()
{
    assert(clip_rect_stack_.size() > 1);

    clip_rect_stack_.pop_back();
    const auto& rect = clip_rect_stack_.back();

    on_changed_header(rect, &draw_cmd::clip_rect);
}

inline void drawlist_base::set_current_texture(texture_handle texture)
{
    on_changed_header(texture, &draw_cmd::texture);
}

inline void drawlist_base::push_texture_id(texture_handle texture)
{
    texture_stack_.push_back(texture);
    set_current_texture(texture);
}

inline void drawlist_base::push_texture_id(textureview* texture)
{
    assert(texture != nullptr && 
           texture->desc().usage == view_usage::shader_resource);
    push_texture_id(texture->native_texture_handle());
}

inline void drawlist_base::pop_texture_id()
{
    assert(texture_stack_.size() > 1);

    texture_stack_.pop_back();
    set_current_texture(texture_stack_.back());
}

inline void drawlist_base::set_current_font(font* font)
{
    current_font_ = font;
}

inline void drawlist_base::push_font(font* font)
{
    font_stack_.push_back(font);
    set_current_font(font);
}

inline void drawlist_base::pop_font()
{
    assert(font_stack_.size() > 1);

    font_stack_.pop_back();
    set_current_font(font_stack_.back());
}

inline draw_cmd& drawlist_base::add_draw_cmd()
{
    assert(cmds_.empty() ||
        vertex_ptr_ == vertices_.size() - cmds_.back().vertex_start);
    vertex_ptr_ = 0;

    auto& ret = cmds_.emplace_back();
    ret.index_start = static_cast<std::uint32_t>(indices_.size());
    ret.vertex_start = static_cast<std::uint32_t>(vertices_.size());
    ret.clip_rect = header_.clip_rect;
    ret.texture = header_.texture;
    return ret;
}

template<typename O>
inline void drawlist_base::on_changed_header(const O& new_value, O draw_cmd::* field)
{
#ifdef _DEBUG
    assert_render_thread();
#endif // _DEBUG
    assert(!cmds_.empty());

    if constexpr (std::is_same_v<O, decltype(draw_cmd::clip_rect)>) {
        header_.clip_rect = new_value;
    }
    else if constexpr (std::is_same_v<O, decltype(draw_cmd::texture)>) {
        header_.texture = new_value;
    }

    auto& curr_cmd = cmds_.back();

    if (curr_cmd.*field != new_value) {
        if (indices_.size() > curr_cmd.index_start) {
            add_draw_cmd();
        }
        else {
            curr_cmd.*field = new_value;
        }
    }
}

template <float CharOffset, unicode::string_like String, std::integral T>
inline float drawlist_base::get_text_width(const String& text, T offset, std::optional<T> count)
{
    std::uint32_t length = static_cast<std::uint32_t>(text.length());
    if (count.has_value() &&
        static_cast<std::uint32_t>(*count) < length) {
        length = static_cast<std::uint32_t>(*count);
    }

    float ret = 0.f;
    std::uint32_t s = offset;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            assert(cp != U'\n');
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph(cp);
        if (glyph == nullptr)
            continue;

        ret += glyph->advance_x;
        if constexpr (CharOffset != 0.f) {
            ret += CharOffset;
        }
    }

    return ret;
}

template <unicode::string_like String, std::integral T>
inline vec2 drawlist_base::get_text_size(const String& text, T offset, std::optional<T> count)
{
    std::uint32_t length = static_cast<std::uint32_t>(text.length());
    if (count.has_value() &&
        static_cast<std::uint32_t>(*count) < length) {
        length = static_cast<std::uint32_t>(*count);
    }
    const float line_height = static_cast<float>(current_font_->cfg().size);

    vec2 ret;
    std::uint32_t s = offset;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            if (cp == U'\n') {
                ret.y += line_height;
                continue;
            }
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph(cp);
        if (glyph == nullptr)
            continue;

        ret.x = (std::max)(ret.x, glyph->advance_x);
    }

    return ret;
}

template <float CharOffset, unicode::string_like String, std::integral T>
inline bool drawlist_base::get_text_width_strict(const String& text, float& out, T offset, std::optional<T> count)
{
    std::uint32_t length = static_cast<std::uint32_t>(text.length());
    if (count.has_value() &&
        static_cast<std::uint32_t>(*count) < length) {
        length = static_cast<std::uint32_t>(*count);
    }

    out = 0.f;
    std::uint32_t s = offset;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            assert(cp != U'\n');
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph_no_fallback(cp);
        if (glyph == nullptr)
            return false;

        out += glyph->advance_x;
        if constexpr (CharOffset != 0.f) {
            out += CharOffset;
        }
    }

    return true;
}

template <unicode::string_like String, std::integral T>
inline bool drawlist_base::get_text_size_strict(const String& text, vec2& out, T offset, std::optional<T> count)
{
    std::uint32_t length = static_cast<std::uint32_t>(text.length());
    if (count.has_value() &&
        static_cast<std::uint32_t>(*count) < length) {
        length = static_cast<std::uint32_t>(*count);
    }
    const float line_height = static_cast<float>(current_font_->cfg().size);

    out = vec2(0.f);
    std::uint32_t s = offset;
    while (s < length) {
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            if (cp == U'\n') {
                out.y += line_height;
                continue;
            }
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph_no_fallback(cp);
        if (glyph == nullptr)
            return false;

        out.x = (std::max)(out.x, glyph->advance_x);
    }

    return true;
}

template <bool center, unicode::string_like String>
inline std::uint32_t drawlist_base::get_char_at_pos(const String& text, float pos)
{
    if (pos <= 0.f)
        return 0u;

    const std::uint32_t length = static_cast<std::uint32_t>(text.length());

    std::uint32_t s = 0u;
    float x = 0.f;
    float prev_width = 0.f;
    while (s < length) {
        const std::uint32_t start = s;
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            assert(cp != U'\n');
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph_no_fallback(cp);
        if (glyph == nullptr)
            continue;

        x += glyph->advance_x;
        if constexpr (center) {
            const float curr_center = x - glyph->advance_x * 0.5f;
            const float prev_center = (x - prev_width * 0.5f - glyph->advance_x);
            if (pos >= prev_center && pos < curr_center) {
                return start;
            }
        }
        else {
            if (pos >= x &&
                pos < x + glyph->advance_x) {
                return start;
            }
        }
        prev_width = glyph->advance_x;
    }

    return length;
}

template <bool center, unicode::string_like String>
inline bool drawlist_base::get_char_at_pos_strict(const String& text, float pos, std::uint32_t& index)
{
    if (pos <= 0.f) {
        index = 0u;
        return true;
    }

    const std::uint32_t length = static_cast<std::uint32_t>(text.length());

    std::uint32_t s = 0u;
    float x = 0.f;
    float prev_width = 0.f;
    while (s < length) {
        const std::uint32_t start = s;
        unicode::unicode_type cp = unicode::get_char_auto(text, length, s);
        if (cp == unicode::codepoint_invalid)
            continue;

        if (cp < 0x20u) {
            assert(cp != U'\n');
            if (cp == U'\r')
                continue;

            continue;
        }

        const auto* glyph = current_font_->find_glyph_no_fallback(cp);
        if (glyph == nullptr)
            return false;

        if constexpr (center) {
            const float curr_center = x - glyph->advance_x * 0.5f;
            const float prev_center = (x - prev_width * 0.5f - glyph->advance_x);
            if (pos >= prev_center && pos < curr_center) {
                index = start;
                return true;
            }
        }
        else {
            if (pos >= x &&
                pos < x + glyph->advance_x) {
                index = start;
                return true;
            }
        }
        x += glyph->advance_x;
        prev_width = glyph->advance_x;
    }

    index = length;

    return true;
}

r2_end_