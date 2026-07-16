#pragma once
#include <backend/context.h>
#include "renderer_definitions.h"
#include "error.h"

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <cassert>


r2_begin_

class renderer_base {
protected:
    renderer_flags flags_{};
    bool is_initialized_{ false };

    std::unique_ptr<context> context_;
    std::unique_ptr<class render_data> render_data_;
    shared_data shared_data_;

#if defined(_DEBUG)
    std::thread::id render_thread_id_;
#endif

    friend class font_atlas;

public:
    renderer_base();
    virtual ~renderer_base();

#if defined(_DEBUG)
    void assert_render_thread() const noexcept {
        assert(!is_initialized_ ||
            std::this_thread::get_id() == render_thread_id_);
    }

    void set_render_thread(const std::thread::id& id) noexcept {
        render_thread_id_ = id;
    }
#endif

public:
    [[nodiscard]] virtual error init(const platform_init_data& pinit, const backend_init_data& binit);
    [[nodiscard]] virtual error init(r2::context* ctx);

    void set_flags(renderer_flags f) noexcept {
        flags_ = f;
    }

public:
    [[nodiscard]] bool is_initialized() const noexcept {
        return is_initialized_;
    }
    [[nodiscard]] context* context() const noexcept {
        return context_.get();
    }
    [[nodiscard]] const render_data* render_data() const noexcept {
        return render_data_.get();
    }
    [[nodiscard]] renderer_flags flags() const noexcept {
        return flags_;
    }
};

r2_end_