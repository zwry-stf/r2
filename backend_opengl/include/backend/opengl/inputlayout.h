#pragma once
#include <backend/inputlayout.h>
#include <backend/opengl/object.h>
#include <vector>


r2_begin_

enum class gl_inputlayout_error : std::int32_t {
    layout_generation,
    layout_creation,
    link_inputlayout
};

struct gl_attr_info
{
    GLint      size;
    GLenum     type;
    GLboolean  normalized;
    bool       integer;
};

class gl_inputlayout : public inputlayout, 
                       protected gl_object {
private:
    GLuint vao_{ 0u };
    std::vector<vertex_attribute_desc> desc_;

public:
    gl_inputlayout(gl_context* ctx, const vertex_attribute_desc* desc, std::uint32_t count, 
                   const std::uint8_t* vs_data, std::size_t vs_data_size);
    ~gl_inputlayout();

public:
    virtual void bind() const override;
    virtual void link(class buffer* buffer) override;

    [[nodiscard]] auto vao() const { 
        return vao_; 
    }

public:
    static [[nodiscard]] gl_attr_info to_gl_attr_info(vertex_attribute_format fmt) noexcept;
    static [[nodiscard]] GLuint type_size(GLenum t) noexcept;
};

r2_end_