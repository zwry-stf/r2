#pragma once
#include <r2/renderer_definitions.h>
#include <r2/font/unicode.h>
#include <r2/util/vector.h>

#include <vector>
#include <cstdint>


r2_begin_

class drawlist_base {
protected:
    shared_data* const shared_data_;
    class renderer* const renderer_;
    vector<vertex> vertices_;
    vector<index> indices_;
    vector<draw_cmd> cmds_;
    std::uint32_t vertex_ptr_;
    vector<vec2> path_;
    vector<point_3d> path3d_;
    cmd_header header_;

    vector<rect> clip_rect_stack_;
    vector<texture_handle> texture_stack_;
    vector<font*> font_stack_;
    font* current_font_{ nullptr };

    friend class renderer;

public:
    drawlist_base(shared_data* shared, class renderer* r) noexcept
        : shared_data_(shared),
          renderer_(r) { }

public:
    /// states
    void set_clip_rect(const rect& r);
    void push_clip_rect(const vec2& min, const vec2& max, bool intersect_current = false);
    void push_clip_rect(const rect& r, bool intersect_current = false);
    void modify_clip_rect_x(std::int32_t min, std::int32_t max);
    void modify_clip_rect_x(float min, float max);
    void modify_clip_rect_y(std::int32_t min, std::int32_t max);
    void modify_clip_rect_y(float min, float max);
    void pop_clip_rect();
    void set_current_texture(texture_handle texture);
    void push_texture_id(texture_handle texture);
    void push_texture_id(textureview* texture);
    void pop_texture_id();
    void set_current_font(font* font);
    void push_font(font* font);
    void pop_font();

    /// text
    // Calculates text width using the same glyph resolution logic as rendering.
    // Missing glyphs may be skipped.
    // Text may *not* contain new line characters
    template <float CharOffset = 0.f, unicode::string_like String, std::integral T = std::uint32_t>
    [[nodiscard]] float get_text_width(const String& text, T offset = 0u, std::optional<T> count = std::nullopt);
    // Calculates text size using the same glyph resolution logic as rendering.
    // Missing glyphs may be skipped.
    template <unicode::string_like String, std::integral T = std::uint32_t>
    [[nodiscard]] vec2 get_text_size(const String& text, T offset = 0u, std::optional<T> count = std::nullopt);
    // Attempts to calculate the width of the text using *only* loaded glyphs.
    // Returns false immediately if any character is missing.
    // May be used for valid caching.
    // Text may *not* contain new line characters
    template <float CharOffset = 0.f, unicode::string_like String, std::integral T = std::uint32_t>
    bool get_text_width_strict(const String& text, float& out, T offset = 0u, std::optional<T> count = std::nullopt);
    // Attempts to calculate the size of the text using *only* loaded glyphs.
    // Returns false immediately if any character is missing.
    // May be used for valid caching.
    template <unicode::string_like String, std::integral T = std::uint32_t>
    bool get_text_size_strict(const String& text, vec2& out, T offset = 0u, std::optional<T> count = std::nullopt);
    // Calculates the index of the char at a given position.
    // Missing glyphs may be skipped.
    // Text may *not* contain new line characters.
    template <bool center = false, unicode::string_like String>
    [[nodiscard]] std::uint32_t get_char_at_pos(const String& text, float pos);
    // Attempts to calculate the index of the char at a given position using *only* loaded glyphs.
    // Returns false immediately if any character is missing.
    // May be used for valid caching.
    // Text may *not* contain new line characters.
    template <bool center = false, unicode::string_like String>
    bool get_char_at_pos_strict(const String& text, float pos, std::uint32_t& index);

public:
    [[nodiscard]] auto& path() noexcept {
        return path_;
    }
    [[nodiscard]] const auto& path() const noexcept {
        return path_;
    }
    [[nodiscard]] const auto& path3d() const noexcept {
        return path3d_;
    }
    [[nodiscard]] const auto& cmd_header() const noexcept {
        return header_;
    }
    [[nodiscard]] vertex& get_vertex(std::uint32_t vtx_ptr) noexcept {
        return vertices_[vtx_ptr];
    }
    [[nodiscard]] const vertex& get_vertex(std::uint32_t vtx_ptr) const noexcept {
        return vertices_[vtx_ptr];
    }
    [[nodiscard]] std::uint32_t vertex_ptr() const noexcept {
        return static_cast<std::uint32_t>(vertices_.size());
    }

protected:
    template <typename O>
    void on_changed_header(const O& new_value, O draw_cmd::* field);
    draw_cmd& add_draw_cmd();
};

r2_end_

#include "drawlist_base.inline.inl"