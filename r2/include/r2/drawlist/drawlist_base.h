#pragma once
#include <r2/renderer_definitions.h>
#include <r2/font/unicode.h>
#include <r2/util/vector.h>

#include <vector>
#include <cstdint>
#include <cstring>


r2_begin_

struct retained_glyph {
    font* font;
    std::uint32_t codepoint;
    bool blurred;
};

struct retained_key {
    std::uint64_t a{ 0x6C62272E07BB0142ull };
    std::uint64_t b{ 0x9E3779B97F4A7C15ull };

    void mix(std::uint64_t v) noexcept {
        a ^= v;
        a *= 0x100000001B3ull;
        b ^= v + 0x9E3779B97F4A7C15ull;
        b *= 0xC2B2AE3D27D4EB4Full;
    }
    void mix_u32(std::uint32_t v) noexcept {
        mix(v);
    }
    void mix_i32(std::int32_t v) noexcept {
        mix(static_cast<std::uint32_t>(v));
    }
    void mix_f32(float v) noexcept {
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        std::uint32_t bits = 0u;
        std::memcpy(&bits, &v, sizeof(bits));
        mix_u32(bits);
    }
    void mix_unit(float v) noexcept {
        mix_f32(v);
    }
    void mix_px(float v) noexcept {
        mix_f32(v);
    }
    void mix_color(const color& c) noexcept {
        mix_u32(static_cast<color_u32>(c));
    }
    void mix_rect(const rectf& r) noexcept {
        mix_px(r.x);
        mix_px(r.y);
        mix_px(r.w);
        mix_px(r.h);
    }
    void mix_rect(const rect& r) noexcept {
        mix_i32(r.left);
        mix_i32(r.top);
        mix_i32(r.right);
        mix_i32(r.bottom);
    }
    void mix_ptr(const void* p) noexcept {
        mix(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p)));
    }
    void mix_bytes(const void* data, std::size_t size) noexcept {
        const auto* bytes = static_cast<const unsigned char*>(data);
        std::uint64_t h = 14695981039346656037ull;
        for (std::size_t i = 0; i < size; ++i) {
            h ^= bytes[i];
            h *= 1099511628211ull;
        }
        mix(h);
        mix(static_cast<std::uint64_t>(size));
    }
    [[nodiscard]] bool operator==(const retained_key& o) const noexcept {
        return a == o.a && b == o.b;
    }
};

struct retained_mesh {
    vector<vertex> vertices;
    vector<index> indices;
    vector<draw_cmd> commands;
    vector<retained_glyph> glyphs;
    retained_key key{};
    font* end_font{ nullptr };
    bool valid{ false };

    void touch() const;
};

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
    vector<rect> text_clip_stack_;
    vector<texture_handle> texture_stack_;
    vector<font*> font_stack_;
    font* current_font_{ nullptr };

    struct capture_state {
        std::uint32_t vtx_begin;
        std::uint32_t idx_begin;
        std::uint32_t cmd_begin;
        std::uint32_t vertex_ptr_begin;
        std::uint32_t clip_depth;
        std::uint32_t texture_depth;
        std::uint32_t font_depth;
        std::uint32_t text_clip_depth;
    };
    capture_state capture_{};
    std::uint32_t capture_depth_{ 0u };
    vector<retained_glyph> capture_glyphs_;

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
    void push_text_clip(const vec2& min, const vec2& max);
    void push_text_clip_x(float min, float max);
    void pop_text_clip();
    [[nodiscard]] bool clip_against_text_clip(vec2& min, vec2& max) const noexcept;
    [[nodiscard]] const rect* text_clip() const noexcept;
    [[nodiscard]] font* current_font() const noexcept {
        return current_font_;
    }

    /// retained
    [[nodiscard]] bool is_capturing() const noexcept {
        return capture_depth_ != 0u;
    }
    [[nodiscard]] bool replay(const retained_mesh& mesh);
    void capture_begin();
    void capture_end(retained_mesh& mesh, const retained_key& key);
    template <typename Fn>
    void draw_retained(retained_mesh& mesh, const retained_key& key, Fn&& fn);
    void note_glyph(font* font, std::uint32_t codepoint, bool blurred);

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
    void push_index(index index) noexcept {
        indices_.emplace_back(index);
    }
    [[nodiscard]] vertex& push_vertex() noexcept {
        vertex_ptr_++;
        return vertices_.emplace_back();
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
    [[nodiscard]] std::uint32_t vertex_ptr_relative() const noexcept {
        return vertex_ptr_;
    }
    [[nodiscard]] renderer* get_renderer() const noexcept {
        return renderer_;
    }
    [[nodiscard]] const r2::vec2& get_uv_white_px() const noexcept {
        return shared_data_->uv_white_px;
    }

protected:
    template <typename O>
    void on_changed_header(const O& new_value, O draw_cmd::* field);
    draw_cmd& add_draw_cmd();
#ifdef _DEBUG
    void assert_render_thread();
#endif // _DEBUG
};

r2_end_

#include "drawlist_base.inline.inl"