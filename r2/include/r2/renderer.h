#pragma once
#include <backend/context.h>
#include "renderer_definitions.h"
#include "renderer_base.h"
#include "drawlist/drawlist_base.h"

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>


r2_begin_

class renderer : public renderer_base {
private:
    std::unique_ptr<class font_atlas> font_atlas_;

    std::mutex font_mutex_;
    std::vector<std::unique_ptr<font>> fonts_;

    float aa_scale_{ 1.f };

    vec2 display_size_;

    std::atomic<bool> destroyed_;
    std::thread update_thread_;

    bool resources_created_{ false };

    friend class font_atlas;
    friend class drawlist2d;

public:
    renderer();
    ~renderer();

public:
    [[nodiscard]] virtual error init(const platform_init_data& pinit, const backend_init_data& binit) override;
    [[nodiscard]] virtual error init(r2::context* ctx) override;
    void destroy();

    [[nodiscard]] bool build_fonts();

    void pre_resize();
    [[nodiscard]] bool post_resize();

    font* add_font(const font_cfg& cfg);
    void remove_font(font* font);

    template <typename T>
        requires (std::is_base_of_v<drawlist_base, T>)
    [[nodiscard]] std::unique_ptr<T> create_drawlist() {
        return std::make_unique<T>(&shared_data_, this);
    }

public:
    /// frame
    [[nodiscard]] bool update_fonts_on_frame();
    void setup_render_state();
    void backup_render_state();
    void restore_render_state();
    [[nodiscard]] bool render(const drawlist_base& list);
    void set_multisampled(bool multisample);
    void enable_depth(bool enabled);

private:
    [[nodiscard]] error do_init();
    [[nodiscard]] error create_resources();
    [[nodiscard]] bool ensure_capacity(std::uint32_t num_indices, std::uint32_t num_vertices);
    void font_update_thread();
    [[nodiscard]] bool update_display_size();

public:
    [[nodiscard]] const vec2& get_render_size() const noexcept {
        return display_size_;
    }
    [[nodiscard]] font_atlas* font_atlas() const noexcept {
        return font_atlas_.get();
    }
    [[nodiscard]] texture_handle font_texture() const noexcept;
};

r2_end_