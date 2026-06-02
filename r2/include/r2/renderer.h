#pragma once
#include <backend/context.h>
#include "renderer_definitions.h"
#include "renderer_base.h"
#include "font/unicode.h"
#include "error.h"
#include "drawlist/drawlist_base.h"

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>


r2_begin_

class renderer : public renderer_base {
private:
    std::unique_ptr<context> context_;
    std::unique_ptr<class font_atlas> font_atlas_;

    bool atlas_update_queued_{ false };
    std::mutex font_mutex_;
    std::vector<std::shared_ptr<font>> fonts_;

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
    [[nodiscard]] error init(const platform_init_data& pinit, const backend_init_data& binit);
    [[nodiscard]] error init(context* ctx);
    void destroy();
    void destroy_render();

    [[nodiscard]] bool build_fonts();
    // will clear the internal font cpu data
    // call build_fonts again to rebuild data
    // can be used to clear old texture
    [[nodiscard]] bool create_font_texture();

    void pre_resize();
    [[nodiscard]] bool post_resize();

    void set_flags(renderer_flags f);

    std::shared_ptr<font> add_font(const font_cfg& cfg);
    void remove_font(font* font);

    [[nodiscard]] bool is_initialized();

    template <typename T>
        requires (std::is_base_of_v<drawlist_base, T>)
    [[nodiscard]] std::unique_ptr<T> create_drawlist() {
        return std::make_unique<T>(&shared_data_, this);
    }

public:
    /// frame
    void update_fonts_on_frame();
    void setup_render_state();
    void backup_render_state();
    void restore_render_state();
    void render(const drawlist_base& list);
    void set_multisampled(bool multisample);
    void enable_depth(bool enabled);

private:
    [[nodiscard]] error do_init();
    [[nodiscard]] error create_resources();
    void ensure_capacity(std::uint32_t num_indices, std::uint32_t num_vertices);
    void font_update_thread();
    void update_display_size();

public:
    [[nodiscard]] auto* context() const noexcept {
        return context_.get();
    }
    [[nodiscard]] const auto& get_render_size() const noexcept {
        return display_size_;
    }
    [[nodiscard]] auto* font_atlas() const noexcept {
        return font_atlas_.get();
    }
    [[nodiscard]] texture_handle font_texture() const noexcept;

    void queue_atlas_update() noexcept {
        atlas_update_queued_ = true;
    }
};

r2_end_