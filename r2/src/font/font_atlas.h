#pragma once
#include <backend/context.h>
#include <r2/renderer_definitions.h>
#include <mutex>


r2_begin_

struct atlas_rect {
    std::uint32_t pos_x;
    std::uint32_t pos_y;
    std::uint32_t width;
    std::uint32_t height;
};

class font_atlas {
public:
    inline static constexpr std::uint32_t kDefaultSize = 4096u;

private:
    class renderer2d* const renderer_;
    std::uint32_t padding_{ 1u };
    const std::uint32_t width_;
    const std::uint32_t height_;
    std::vector<std::uint32_t> data32_;
    std::vector<std::uint32_t> free_rect_slots_;
    std::vector<atlas_rect> rects_;

public:
    inline static constexpr std::uint32_t kBakedLinesMaxWidth = 63u;
    vec4 tex_uv_lines[kBakedLinesMaxWidth + 1u]{};

public:
    font_atlas(renderer2d* instance) noexcept;

private:
    bool check_side(std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height);
    void find_rect(std::uint32_t width, std::uint32_t height, std::uint32_t& x, std::uint32_t& y);
    void add_white_pixel();
    void add_tex_lines();
    void add_shadow_tex();

public:
    std::uint32_t register_rect(std::uint32_t width, std::uint32_t height);
    void remove_rect(std::uint32_t id);
    void get_rect_uv(std::uint32_t id, vec2& uv_min, vec2& uv_max) const;
    [[nodiscard]] atlas_rect get_rect(std::uint32_t id);
    void write_data(std::uint32_t id, const std::uint8_t* data, std::size_t size);

public:
    bool build();

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
};

r2_end_