#pragma once
#include <backend/context.h>
#include <r2/renderer_definitions.h>

#include <mutex>
#include <optional>
#include <functional>


r2_begin_

struct atlas_rect {
    std::uint32_t pos_x;
    std::uint32_t pos_y;
    std::uint32_t width;
    std::uint32_t height;
};

class font_atlas {
public:
    inline static constexpr std::uint32_t k_default_size = 512u;

private:
    class renderer_base* const renderer_;
    const std::uint32_t max_width_;
    const std::uint32_t max_height_;
    bool in_init_{ false };
    std::uint32_t padding_{ 1u };
    std::uint32_t width_{ 0u };
    std::uint32_t height_{ 0u };
    std::vector<std::uint32_t> data32_;
    std::vector<std::uint32_t> free_rect_slots_;
    std::vector<atlas_rect> rects_;

    struct rect_write_t {
        atlas_rect rect;
        std::vector<std::uint32_t> data;
    };
    std::vector<rect_write_t> rect_writes_;
    std::vector<std::uint32_t> temp_upload_;
    std::vector<std::uint32_t> temp_upload_clear_;

    bool resize_queued_{ false };
    std::uint32_t last_width_;
    std::uint32_t last_height_;
    std::function<void()> resize_callback_;

public:
    font_atlas(renderer_base* instance, std::uint32_t max_width = 0, std::uint32_t max_height = 0) noexcept;

public:
    [[nodiscard]] bool build();
    [[nodiscard]] std::optional<std::uint32_t> register_rect(std::uint32_t width, std::uint32_t height);
    void remove_rect(std::uint32_t id);
    void get_rect_pos(std::uint32_t id, vec2& min, vec2& max) const;
    void get_rect_uv(std::uint32_t id, vec2& uv_min, vec2& uv_max) const;
    [[nodiscard]] atlas_rect get_rect(std::uint32_t id);
    void write_data(std::uint32_t id, const std::uint8_t* data, std::size_t size);
    void write_data(std::uint32_t id, const std::uint32_t* data, std::size_t size);
    void write_data(std::uint32_t id, std::vector<std::uint32_t> data);
    void write_data(const atlas_rect& r, const std::uint32_t* data, std::size_t size);
    void write_data_init(std::uint32_t id, const std::uint8_t* data, std::size_t size);
    [[nodiscard]] bool update_resize();

    void set_resize_callback(std::function<void()> cb) noexcept {
        resize_callback_ = std::move(cb);
    }

private:
    bool check_side(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height);
    [[nodiscard]] bool find_rect(std::uint32_t width, std::uint32_t height, std::uint32_t& x, std::uint32_t& y);
    [[nodiscard]] bool add_white_pixel();
    [[nodiscard]] bool add_tex_lines();
    [[nodiscard]] bool add_shadow_tex();
    void grow_atlas();
    void update_cached_uvs();
    [[nodiscard]] bool create_font_texture();

public:
    [[nodiscard]] auto& get_data32() noexcept {
        return data32_;
    }
    [[nodiscard]] const auto& get_data32() const noexcept {
        return data32_;
    }
    [[nodiscard]] auto get_width() const noexcept {
        return width_;
    }
    [[nodiscard]] auto get_height() const noexcept {
        return height_;
    }
    [[nodiscard]] bool is_resize_queued() const noexcept {
        return resize_queued_;
    }
};

r2_end_