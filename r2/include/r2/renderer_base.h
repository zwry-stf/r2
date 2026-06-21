#pragma once
#include <backend/context.h>
#include "renderer_definitions.h"
#include "font/unicode.h"

#include <vector>
#include <thread>
#include <atomic>
#include <mutex>


r2_begin_

class renderer_base {
protected:
    renderer_flags flags_{};

    bool is_initialized_{ false };

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

    void set_render_thread(const std::thread::id& id) {
        render_thread_id_ = id;
    }
#endif

public:
    void set_flags(renderer_flags f);

    [[nodiscard]] bool is_initialized();

public:
    [[nodiscard]] const auto* render_data() const noexcept {
        return render_data_.get();
    }
    [[nodiscard]] auto flags() const noexcept {
        return flags_;
    }
};

r2_end_