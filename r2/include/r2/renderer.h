#pragma once
#include <backend/context.h>
#include "renderer_definitions.h"
#include "font/unicode.h"
#include <vector>
#include <thread>
#include <atomic>


r2_begin_

class renderer2d {
private:
    std::unique_ptr<context> context_;
    std::unique_ptr<class render_data> render_data_;
    std::unique_ptr<class font_atlas> font_atlas_;

    // rendering
    std::vector<vertex> vertices_;
    std::vector<index> indices_;
    std::vector<draw_cmd> cmds_;
    std::uint32_t vertex_ptr_;
    std::vector<vec2> path_;
    cmd_header header_;
    shared_data shared_data_;

    renderer_flags flags_{};

    bool atlas_update_queued_{ false };
    std::vector<rect> clip_rect_stack_;
    std::vector<texture_handle> texture_stack_;
    std::vector<font*> font_stack_;
    std::vector<std::unique_ptr<font>> fonts_;
    font* current_font_{ nullptr };

    float aa_scale_{ 1.f };

    vec2 display_size_;

    std::atomic<bool> destroyed_;
    std::thread update_thread_;

    bool resources_created_{ false };
    bool is_initialized_{ false };

#if defined(_DEBUG)
    std::thread::id render_thread_id_;
#endif

    friend class font_atlas;

public:
    renderer2d();
    ~renderer2d();

#if defined(_DEBUG)
    void assert_render_thread() const noexcept {
        assert(!is_initialized_ ||
            std::this_thread::get_id() == render_thread_id_);
    }

    void set_render_thread(const std::thread::id& id) {
        render_thread_id_ = id;
    }
#endif

public:
    void init(const platform_init_data& pinit, const backend_init_data& binit);
    void init(context* ctx);
    void destroy();

    void build_fonts();
    void create_font_texture();

    void pre_resize();
    void post_resize();

    void set_flags(renderer_flags f);

    font* add_font(const font_cfg& cfg);

    [[nodiscard]] bool is_initialized();

public:
    /// frame
    void update_fonts_on_frame();
    void setup_render_state();
    void backup_render_state();
    void restore_render_state();
    void reset_render_data();
    void render();

    /// states
    void set_clip_rect(const rect& r);
    void push_clip_rect(const vec2& min, const vec2& max, bool intersect_current = false);
    void push_clip_rect(const rect& r, bool intersect_current = false);
    void pop_clip_rect();
    void set_current_texture(texture_handle texture);
    void push_texture_id(texture_handle texture);
    void push_texture_id(textureview* texture);
    void pop_texture_id();
    void set_current_font(font* font);
    void push_font(font* font);
    void pop_font();

    /// render
    void prim_rect(const vec2& min, const vec2& max, color_u32 col);
    void add_rect(const vec2& min, const vec2& max, color_u32 col, float line_width, float rounding = 0.f,
                  e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void add_rect_inner(const vec2& min, const vec2& max, color_u32 col, float line_width, float rounding = 0.f,
                        e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void add_rect_filled(const vec2& min, const vec2& max, color_u32 col, float rounding = 0.f,
                         e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void add_rect_filled_multicolor(const vec2& min, const vec2& max,
                                    color_u32 col_tl, color_u32 col_tr, color_u32 col_bl, color_u32 col_br);
    void add_rect_filled_faded(const vec2& min, const vec2& max, color_u32 col, color_u32 faded_col, 
                               float fade_start, float fade_end);
    void add_shadow_rect_filled(const vec2& min, const vec2& max, color_u32 col, float rounding = 0.f, float shadow_size = 50.f,
                                e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void add_quad_filled(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4, color_u32 col);
    void add_quad_filled_multicolor(const vec2& p1, const vec2& p2, const vec2& p3, const vec2& p4, 
                                    color_u32 col1, color_u32 col2, color_u32 col3, color_u32 col4);
    void add_line(const vec2& start, const vec2& end, color_u32 col, float line_width);
    void add_convex_filled(const vec2* points, std::uint32_t num_points, color_u32 col);
    void add_shadow_convex_filled(const vec2* points, std::uint32_t num_points, color_u32 col, float shadow_size, bool filled = true);
    void add_lines(const vec2* points, std::uint32_t num_points, color_u32 col, float line_width, bool closed = false);

    void add_image(texture_handle texture, const vec2& min, const vec2& max, color_u32 col = color::white(),
                   const vec2& uv_min = vec2(0.f), const vec2& uv_max = vec2(1.f));
    void add_image_rounded(texture_handle texture, const vec2& min, const vec2& max, float rounding, color_u32 col = color::white(),
                           const vec2& uv_min = vec2(0.f), const vec2& uv_max = vec2(1.f));
    void shade_vertices_uv(std::uint32_t vtx_start, std::uint32_t vtx_end, const vec2& min, const vec2& max, 
                           const vec2& uv_min, const vec2& uv_max);

    /// text
    template <unicode::string_like String>
    void add_text(const vec2& pos, color_u32 col, const String& text, bool blurred = false);
    template <unicode::string_like String>
    void add_text_faded(const vec2& pos, color_u32 col, color_u32 faded_col, float fade_start, float fade_end, const String& text, bool blurred = false);
    // Calculates text width using the same glyph resolution logic as rendering.
    // Missing glyphs may be skipped.
    // Text may *not* contain new line characters
    template <unicode::string_like String, std::integral T = std::uint32_t>
    [[nodiscard]] float get_text_width(const String& text, T offset = 0u, std::optional<T> count = std::nullopt);
    // Calculates text size using the same glyph resolution logic as rendering.
    // Missing glyphs may be skipped.
    template <unicode::string_like String, std::integral T = std::uint32_t>
    [[nodiscard]] vec2 get_text_size(const String& text, T offset = 0u, std::optional<T> count = std::nullopt);
    // Attempts to calculate the width of the text using *only* loaded glyphs.
    // Returns false immediately if any character is missing.
    // May be used for valid caching.
    // Text may *not* contain new line characters
    template <unicode::string_like String, std::integral T = std::uint32_t>
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

    /// path
    void path_clear();
    void path_add_point(const vec2& p);
    template <int a_min_of_12, int a_max_of_12>
    void path_arc_to(const vec2& center, float radius, float step);
    void path_rect(const vec2& min, const vec2& max, float rounding,
                   e_rounding_flags flags = e_rounding_flags::rounding_all, float corner_step = 2.f);
    void path_fill_convex(color_u32 col);
    void path_stroke(color_u32 col, float line_width, bool closed = false);

private:
    void do_init();
    void create_resources();
    void ensure_capacity(std::uint32_t num_indices, std::uint32_t num_vertices);
    draw_cmd& add_draw_cmd();
    void font_update_thread();
    void update_display_size();

    template <typename O>
    void on_changed_header(const O& new_value, O draw_cmd::* field);

    void aa_side(const vec2& start, const vec2& end, std::uint32_t vtx_start, std::uint32_t vtx_end, color_u32 col);

public:
    [[nodiscard]] auto* context() const noexcept {
        return context_.get();
    }
    [[nodiscard]] auto flags() const noexcept {
        return flags_;
    }
    [[nodiscard]] const auto& get_render_size() const noexcept {
        return display_size_;
    }
    [[nodiscard]] auto* font_atlas() const noexcept {
        return font_atlas_.get();
    }
    [[nodiscard]] const auto* render_data() const noexcept {
        return render_data_.get();
    }
    [[nodiscard]] texture_handle font_texture() const noexcept;

    void queue_atlas_update() noexcept {
        atlas_update_queued_ = true;
    }
};

r2_end_

#include "renderer.inline.inl"