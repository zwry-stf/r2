#pragma once
#include "../renderer_definitions.h"
#include <r2/font/unicode.h>

#include <array>
#include <cmath>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <chrono>


struct hb_buffer_t;
r2_begin_

struct font_glyph {
    std::uint32_t rect_id;
    bool visible;
    bool blurred;
    bool colored{};
    wchar codepoint;
    float advance_x;
    float x0, y0, x1, y1;
    vec2 uv_min, uv_max;
    std::uint64_t last_access{ 0u };
};

struct pending_glyph {
    bool failed = false;
    wchar codepoint;
    bool visible;
    bool blurred{};
    float advance_x;
    float x0, y0, x1, y1;
    std::uint32_t bmp_w{};
    std::uint32_t bmp_h{};
    std::vector<std::uint8_t> bitmap;
    std::vector<std::uint32_t> color_bitmap;
};

struct glyph_lookup_data {
    inline static constexpr std::uint32_t k_invalid_index = static_cast<std::uint32_t>(-1) & ~((1 << 30) | (1 << 31));

    std::uint32_t supported : 1 { 0u };
    std::uint32_t loading : 1 { 0u };
    std::uint32_t index : 30 { k_invalid_index };
};

struct font_range {
    wchar range_min;
    wchar range_max;
};

struct font_data;
struct shaped_run;

struct glyph_queue {
    wchar codepoint;
    bool blurred{};
    const shaped_run* run{};
};

class font {
private:
    class font_atlas* const atlas_;
    const font_cfg cfg_;
    float center_y_{};
    std::vector<std::unique_ptr<font_data>> fonts_;
    std::vector<bool> color_characters_;
    hb_buffer_t* shape_buffer_{};
    std::uint32_t shaped_glyph_start_{};
    std::vector<std::unique_ptr<shaped_run>> shaped_runs_;
    std::unordered_map<std::u32string, wchar> shaped_runs_by_text_;

    struct shape_result {
        wchar codepoint{};
        std::uint32_t count{ 1u };
    };
    struct shape_cache_entry {
        std::u32string text;
        shape_result result;
    };
    std::array<shape_cache_entry, 256> shape_cache_;
    bool is_destroyed_{ false };
    bool prepared_{ false };

    std::uint32_t fallback_glyph_{ glyph_lookup_data::k_invalid_index };

    // 
    std::vector<font_glyph> glyphs_;
    std::vector<std::uint32_t> free_glyph_slots_;
    mutable std::vector<glyph_lookup_data> glyph_lookup_;
    mutable std::vector<glyph_lookup_data> glyph_lookup_blurred_;
    std::chrono::steady_clock::time_point last_cleanup_;
    std::uint64_t frame_start_{ 0u };
    std::uint32_t content_epoch_{ 1u };

    std::vector<float> kernel_weights_;
    std::vector<std::uint8_t> private_buffer_;
    std::vector<std::uint8_t> private_buffer2_;
    std::vector<float> private_buffer3_;
    std::vector<float> private_buffer4_;

    // synced load
    mutable std::mutex request_mutex_;
    mutable std::vector<glyph_queue> glyphs_to_rasterize_;

    mutable std::mutex completed_mutex_;
    mutable std::vector<pending_glyph> completed_glyphs_;

public:
    font(font_atlas* atlas, const font_cfg& cfg);
    ~font();

public:
    template <unicode::string_like String>
    [[nodiscard]] unicode::unicode_type next_char(const String& text, std::uint32_t length, std::uint32_t& pos);
    [[nodiscard]] const font_glyph* find_glyph(wchar c);
    [[nodiscard]] const font_glyph* find_glyph_blurred(wchar c);
    [[nodiscard]] const font_glyph* find_glyph_no_fallback(wchar c);

    [[nodiscard]] float baseline() const noexcept;
    [[nodiscard]] float center_offset_y(float height) const noexcept {
        return std::round(height * 0.5f - center_y_);
    }
    [[nodiscard]] const font_cfg& cfg() const noexcept {
        return cfg_;
    }
    [[nodiscard]] bool is_destroyed() const noexcept {
        return is_destroyed_;
    }
    [[nodiscard]] std::uint32_t content_epoch() const noexcept {
        return content_epoch_;
    }

    void update_on_render();
    void update_uvs();
    void update_worker();
    bool prepare();
    bool build(bool initial_build = false);
    void destroy();

    // Adds font data to the font instance.
    // The order of added fonts matters. TrueType and OpenType fonts are supported.
    bool add_font(const std::uint8_t* data, std::size_t data_size);
    // Adds font data to the font instance.
    // The order of added fonts matters. TrueType and OpenType fonts are supported.
    bool add_font(const std::uint8_t* data, std::size_t data_size, const std::vector<font_range>& ranges);
    // Adds font data to the font instance.
    // The order of added fonts matters. TrueType and OpenType fonts are supported.
    bool add_font(const std::uint8_t* data, std::size_t data_size, std::vector<font_range>&& ranges);

    // Adds font data to the font instance.
    // The order of added fonts matters. TrueType and OpenType fonts are supported.
    bool add_font(std::vector<std::uint8_t> data);
    // Adds font data to the font instance.
    // The order of added fonts matters. TrueType and OpenType fonts are supported.
    bool add_font(std::vector<std::uint8_t> data, const std::vector<font_range>& ranges);
    // Adds font data to the font instance.
    // The order of added fonts matters. TrueType and OpenType fonts are supported.
    bool add_font(std::vector<std::uint8_t> data, std::vector<font_range>&& ranges);

private:
    bool add_font_data(std::unique_ptr<font_data> data);
    [[nodiscard]] shape_result shape_char(std::span<const char32_t> text);
    void queue_glyph(wchar glyph, bool blurred);
    pending_glyph rasterize_run(wchar glyph, const shaped_run& run, bool blurred);
    pending_glyph rasterize_glyph(wchar glyph, bool blurred = false, const shaped_run* run = nullptr);
    void apply_glyph(const pending_glyph& pg, bool is_init_time);
    void build_weights();
    void blur_rect(std::uint32_t w, std::uint32_t h);
    void glow_rect(std::uint32_t w, std::uint32_t h);
    font_data* get_font_data_for_char(wchar c) const noexcept;
};

r2_end_

#include "font.inline.inl"