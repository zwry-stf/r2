#include <r2/drawlist/drawlist_base.h>
#include <r2/renderer.h>
#include <r2/font/font.h>

#include <cstring>


r2_begin_

void retained_mesh::touch() const
{
    for (std::uint32_t i = 0u; i < glyphs.size(); ++i) {
        const retained_glyph& g = glyphs[i];
        if (g.font == nullptr) {
            continue;
        }
        const auto cp = static_cast<wchar>(g.codepoint);
        if (g.blurred) {
            (void)g.font->find_glyph_blurred(cp);
        }
        else {
            (void)g.font->find_glyph(cp);
        }
    }
}

namespace {

void copy_range(auto& dst, const auto* src, std::uint32_t count)
{
    dst.resize(count);
    if (count != 0u) {
        std::memcpy(dst.data(), src, static_cast<std::size_t>(count) * sizeof(*src));
    }
}

}

void drawlist_base::capture_begin()
{
#if defined(_DEBUG)
    assert_render_thread();
#endif
    assert(capture_depth_ == 0u);
    assert(!cmds_.empty());

    capture_.vtx_begin = static_cast<std::uint32_t>(vertices_.size());
    capture_.idx_begin = static_cast<std::uint32_t>(indices_.size());
    capture_.cmd_begin = static_cast<std::uint32_t>(cmds_.size() - 1u);
    capture_.vertex_ptr_begin = vertex_ptr_;
    capture_.clip_depth = static_cast<std::uint32_t>(clip_rect_stack_.size());
    capture_.texture_depth = static_cast<std::uint32_t>(texture_stack_.size());
    capture_.font_depth = static_cast<std::uint32_t>(font_stack_.size());
    capture_.text_clip_depth = static_cast<std::uint32_t>(text_clip_stack_.size());
    capture_glyphs_.clear();
    capture_depth_ = 1u;
}

void drawlist_base::capture_end(retained_mesh& mesh, const retained_key& key)
{
#if defined(_DEBUG)
    assert_render_thread();
#endif
    assert(capture_depth_ == 1u);
    capture_depth_ = 0u;

    const bool balanced =
        clip_rect_stack_.size() == capture_.clip_depth &&
        texture_stack_.size() == capture_.texture_depth &&
        font_stack_.size() == capture_.font_depth &&
        text_clip_stack_.size() == capture_.text_clip_depth;
    assert(balanced);
    if (!balanced) {
        mesh.valid = false;
        capture_glyphs_.clear();
        return;
    }

    const std::uint32_t vtx_end = static_cast<std::uint32_t>(vertices_.size());
    const std::uint32_t idx_end = static_cast<std::uint32_t>(indices_.size());
    const std::uint32_t vtx_count = vtx_end - capture_.vtx_begin;
    const std::uint32_t idx_count = idx_end - capture_.idx_begin;

    copy_range(mesh.vertices, vertices_.data() + capture_.vtx_begin, vtx_count);
    mesh.indices.resize(idx_count);
    mesh.commands.clear();

    std::uint32_t written = 0u;
    bool first = true;
    const std::uint32_t cmd_count = static_cast<std::uint32_t>(cmds_.size());
    for (std::uint32_t ci = capture_.cmd_begin; ci < cmd_count; ++ci) {
        const draw_cmd& cmd = cmds_[ci];
        const std::uint32_t idx_lo = first ? capture_.idx_begin : cmd.index_start;
        const std::uint32_t idx_hi = (ci + 1u < cmd_count) ?
            cmds_[ci + 1u].index_start : idx_end;
        first = false;
        if (idx_hi <= idx_lo) {
            continue;
        }

        draw_cmd stored = cmd;
        stored.index_start = written;
        if (ci == capture_.cmd_begin) {
            stored.vertex_start = 0u;
            for (std::uint32_t i = idx_lo; i < idx_hi; ++i) {
                assert(indices_[i] >= capture_.vertex_ptr_begin);
                mesh.indices[written++] = indices_[i] - capture_.vertex_ptr_begin;
            }
        }
        else {
            assert(cmd.vertex_start >= capture_.vtx_begin);
            stored.vertex_start = cmd.vertex_start - capture_.vtx_begin;
            for (std::uint32_t i = idx_lo; i < idx_hi; ++i) {
                mesh.indices[written++] = indices_[i];
            }
        }
        mesh.commands.push_back(stored);
    }

    assert(written == idx_count);
    mesh.glyphs.clear();
    if (!capture_glyphs_.empty()) {
        copy_range(mesh.glyphs, capture_glyphs_.data(),
            static_cast<std::uint32_t>(capture_glyphs_.size()));
    }
    capture_glyphs_.clear();
    mesh.end_font = current_font_;
    mesh.key = key;
    mesh.valid = true;
}

bool drawlist_base::replay(const retained_mesh& mesh)
{
#if defined(_DEBUG)
    assert_render_thread();
#endif
    if (!mesh.valid) {
        return false;
    }

    mesh.touch();

    const r2::cmd_header saved = header_;
    if (!mesh.indices.empty()) {
        const std::uint32_t cmd_count = static_cast<std::uint32_t>(mesh.commands.size());
        for (std::uint32_t ci = 0u; ci < cmd_count; ++ci) {
            const draw_cmd& cmd = mesh.commands[ci];
            const std::uint32_t v_hi = (ci + 1u < cmd_count) ?
                mesh.commands[ci + 1u].vertex_start :
                static_cast<std::uint32_t>(mesh.vertices.size());
            const std::uint32_t i_hi = (ci + 1u < cmd_count) ?
                mesh.commands[ci + 1u].index_start :
                static_cast<std::uint32_t>(mesh.indices.size());
            if (i_hi <= cmd.index_start || v_hi < cmd.vertex_start) {
                continue;
            }

            const std::uint32_t vcount = v_hi - cmd.vertex_start;
            const std::uint32_t icount = i_hi - cmd.index_start;
            const bool can_merge = !cmds_.empty() &&
                indices_.size() > cmds_.back().index_start &&
                cmds_.back().clip_rect == cmd.clip_rect &&
                cmds_.back().texture == cmd.texture &&
                header_.clip_rect == cmd.clip_rect &&
                header_.texture == cmd.texture;

            std::uint32_t base = vertex_ptr_;
            if (!can_merge) {
                header_.clip_rect = cmd.clip_rect;
                header_.texture = cmd.texture;
                if (!cmds_.empty() && indices_.size() > cmds_.back().index_start) {
                    add_draw_cmd();
                }
                else {
                    cmds_.back().clip_rect = header_.clip_rect;
                    cmds_.back().texture = header_.texture;
                }
                base = vertex_ptr_;
            }

            auto* vtx = vertices_.append(vcount);
            std::memcpy(vtx, mesh.vertices.data() + cmd.vertex_start,
                static_cast<std::size_t>(vcount) * sizeof(vertex));
            auto* idx = indices_.append(icount);
            const index* src = mesh.indices.data() + cmd.index_start;
            for (std::uint32_t i = 0u; i < icount; ++i) {
                idx[i] = src[i] + base;
            }
            vertex_ptr_ += vcount;
        }
    }

    header_ = saved;
    current_font_ = mesh.end_font;
    if (!cmds_.empty()) {
        draw_cmd& back = cmds_.back();
        if (back.clip_rect != header_.clip_rect || back.texture != header_.texture) {
            if (indices_.size() > back.index_start) {
                add_draw_cmd();
            }
            else {
                back.clip_rect = header_.clip_rect;
                back.texture = header_.texture;
            }
        }
    }
    return true;
}

#ifdef _DEBUG
void drawlist_base::assert_render_thread()
{
    renderer_->assert_render_thread();
}
#endif // _DEBUG

r2_end_