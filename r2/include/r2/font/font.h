#pragma once
#include "../renderer_definitions.h"
#include <mutex>
#include <chrono>


struct stbtt_fontinfo;
r2_begin_

struct font_glyph {
    std::uint32_t rect_id;
    bool visible;
    bool blurred;
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
};

struct glyph_lookup_data {
    inline static constexpr std::uint32_t kInvalidIndex = static_cast<std::uint32_t>(-1) & ~((1 << 30) | (1 << 31));

    std::uint32_t supported : 1 { 0u };
    std::uint32_t loading : 1 { 0u };
    std::uint32_t index : 30 { kInvalidIndex };
};

struct font_range {
    wchar range_min;
    wchar range_max;
};

struct font_data {
    const std::uint8_t* const data;
    const std::size_t size;
    std::vector<font_range> ranges;
    std::unique_ptr<stbtt_fontinfo> font_info;
};

struct glyph_queue {
    wchar codepoint;
    bool blurred{};
};

class font {
private:
    class font_atlas* const atlas_;
    const font_cfg cfg_;
    std::vector<font_data> fonts_;

    const font_glyph* fallback_glyph_{ nullptr };

    // 
    std::vector<font_glyph> glyphs_;
    std::vector<std::uint32_t> free_glyph_slots_;
    mutable std::vector<glyph_lookup_data> glyph_lookup_;
    mutable std::vector<glyph_lookup_data> glyph_lookup_blurred_;
    std::chrono::steady_clock::time_point last_cleanup_;
    std::uint64_t frame_start_{ 0u };

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
    [[nodiscard]] const font_glyph* find_glyph(wchar c);
    [[nodiscard]] const font_glyph* find_glyph_blurred(wchar c);
    [[nodiscard]] const font_glyph* find_glyph_no_fallback(wchar c);
    [[nodiscard]] const font_cfg& cfg() const;

    bool update_on_render();
    void update_worker();
    bool build();
    void destroy();

    // Adds font data to the font instance.
    // The order of added fonts matters. Only TTF format is supported.
    bool add_font(const std::uint8_t* data, std::size_t data_size);
    // Adds font data to the font instance.
    // The order of added fonts matters. Only TTF format is supported.
    bool add_font(const std::uint8_t* data, std::size_t data_size, const std::vector<font_range>& ranges);
    // Adds font data to the font instance.
    // The order of added fonts matters. Only TTF format is supported.
    bool add_font(const std::uint8_t* data, std::size_t data_size, std::vector<font_range>&& ranges);

private:
    pending_glyph rasterize_glyph(wchar glyph, font_data* data = nullptr, bool blurred = false);
    void apply_glyph(const pending_glyph& pg);
    void build_weights();
    void blur_rect(std::uint32_t w, std::uint32_t h);
    void glow_rect(std::uint32_t w, std::uint32_t h);
    stbtt_fontinfo* get_font_data_for_char(wchar c) const noexcept;
};

r2_end_

#include "font.inline.inl"