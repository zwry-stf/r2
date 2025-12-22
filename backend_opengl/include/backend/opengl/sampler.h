#pragma once
#include <backend/sampler.h>
#include <backend/opengl/object.h>


r2_begin_

enum class gl_sampler_error : std::int32_t {
    sampler_generation,
    sampler_creation
};

class gl_sampler : public sampler,
                   protected gl_object {
private:
    GLuint sampler_{ 0u };

public:
    gl_sampler(gl_context* ctx, const sampler_desc& desc);
    ~gl_sampler();

public:
    [[nodiscard]] auto sampler() const noexcept {
        return sampler_;
    }

public:
    static [[nodiscard]] GLenum to_gl_filter(const sampler_desc& desc) noexcept;
    static [[nodiscard]] GLenum to_gl_address(sampler_address_mode m) noexcept;
    static [[nodiscard]] GLenum to_gl_compare(sampler_compare_func f) noexcept;
};

r2_end_