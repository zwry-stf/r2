#include <r2/font/font.h>
#include <r2/font/font_atlas.h>
#include <r2/font/unicode.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include <hb.h>
#include <hb-ot.h>
#pragma warning(push, 0)
#define STB_IMAGE_RESIZE_STATIC
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#pragma warning(pop)
#include <algorithm>
#include <cmath>
#include <limits>


r2_begin_

struct font_data {
    std::vector<std::uint8_t> owned_data;
    const std::uint8_t* data{};
    std::size_t size{};
    std::vector<font_range> ranges;
    FT_Library library{};
    FT_Face face{};
    hb_font_t* shaping_font{};
    std::vector<bool> color_glyphs;
    std::uint32_t glyph_offset{};
    float scale_x{ 1.f };
    float scale_y{ 1.f };
    float baseline{};

    ~font_data() {
        if (shaping_font != nullptr) {
            hb_font_destroy(shaping_font);
        }
        if (face != nullptr) {
            FT_Done_Face(face);
        }
        if (library != nullptr) {
            FT_Done_FreeType(library);
        }
    }
};

struct shaped_run {
    struct glyph {
        wchar codepoint;
        float x;
        float y;
    };
    std::vector<glyph> glyphs;
    float advance_x{};
};

inline static bool codepoint_in_ranges(wchar cp, const std::vector<font_range>& ranges) noexcept {
    for (const auto& range : ranges) {
        if (cp >= range.range_min && cp <= range.range_max) {
            return true;
        }
    }
    return false;
}

font::font(font_atlas* atlas, const font_cfg& cfg)
    : atlas_(atlas),
      cfg_(cfg),
      shape_buffer_(hb_buffer_create()) { }

font::~font() {
    hb_buffer_destroy(shape_buffer_);
}

float font::baseline() const noexcept
{
    return fonts_.front()->baseline;
}

void font::update_on_render()
{
    frame_start_ += 1u;

    std::vector<pending_glyph> local_completed;
    {
        std::lock_guard<std::mutex> lock(completed_mutex_);
        local_completed.swap(completed_glyphs_);
    }

    for (auto& pg : local_completed) {
        if (pg.failed) {
            // remove supported flag
            (pg.blurred ?
                glyph_lookup_blurred_[pg.codepoint].supported :
                glyph_lookup_[pg.codepoint].supported) = 0u;
            continue;
        }
        apply_glyph(std::move(pg), false /* init time */);
    }

    // clean unused rects
    constexpr auto k_cleanup_time = std::chrono::seconds{ 20 };
    const auto now = std::chrono::steady_clock::now();
    if (now - last_cleanup_ > k_cleanup_time) {
        constexpr std::uint64_t k_remove_age = 100000u;
        bool removed = false;

        for (std::uint32_t idx = 0u; idx < glyphs_.size(); ++idx) {
            if (idx == fallback_glyph_) {
                continue;
            }

            auto& g = glyphs_[idx];
            if (!g.visible) {
                continue;
            }

            if (frame_start_ > g.last_access &&
                frame_start_ - g.last_access > k_remove_age) {
                atlas_->remove_rect(g.rect_id);

                auto& e = g.blurred ? glyph_lookup_blurred_[g.codepoint] : glyph_lookup_[g.codepoint];
                e.index = glyph_lookup_data::k_invalid_index;
                e.loading = 0u;

                g.visible = false;
                free_glyph_slots_.push_back(idx);
                removed = true;
            }
        }

        if (removed) {
            ++content_epoch_;
        }

        last_cleanup_ = now;
    }
}

void font::update_uvs()
{
    ++content_epoch_;

    const auto m = vec2(1.f) / vec2(
        static_cast<float>(atlas_->get_width()),
        static_cast<float>(atlas_->get_height())
    );
    for (auto& g : glyphs_) {
        if (!g.visible) {
            continue;
        }

        atlas_->get_rect_uv(g.rect_id, g.uv_min, g.uv_max);
    }
}

void font::update_worker()
{
    std::vector<glyph_queue> local_requests;
    {
        std::lock_guard<std::mutex> lock(request_mutex_);
        local_requests.swap(glyphs_to_rasterize_);
    }

    if (local_requests.empty()) {
        return;
    }

    std::vector<pending_glyph> local_completed;
    local_completed.reserve(local_requests.size());

    for (const auto& cp : local_requests) {
        auto pg = rasterize_glyph(cp.codepoint, cp.blurred, cp.run);
        local_completed.push_back(
            std::move(pg)
        );
    }

    {
        std::lock_guard<std::mutex> lock(completed_mutex_);
        for (auto& g : local_completed) {
            completed_glyphs_.push_back(std::move(g));
        }
    }
}

bool font::prepare()
{
    assert(!fonts_.empty());

    if (prepared_) {
        return true;
    }

    const bool has_blur = cfg_.glow_radius > 0u;

    std::size_t lookup_size = unicode::codepoint_max + 1u;
    for (auto& source : fonts_) {
        if (source->shaping_font != nullptr) {
            source->glyph_offset = static_cast<std::uint32_t>(lookup_size);
            lookup_size += source->color_glyphs.size();
        }
    }
    shaped_glyph_start_ = static_cast<std::uint32_t>(lookup_size);
    const auto color_glyph_count = lookup_size - unicode::codepoint_max - 1u;
    glyph_lookup_.reserve(lookup_size + color_glyph_count);
    glyph_lookup_.resize(lookup_size);
    if (lookup_size > unicode::codepoint_max + 1u) {
        color_characters_.resize(unicode::codepoint_max + 1u);
    }
    for (auto& source : fonts_) {
        FT_UInt glyph{};
        auto cp = FT_Get_First_Char(source->face, &glyph);
        while (glyph != 0u) {
            if (cp >= 0x20u && cp <= unicode::codepoint_max &&
                !(cp >= 0xD800u && cp <= 0xDFFFu) &&
                codepoint_in_ranges(static_cast<wchar>(cp), source->ranges)) {
                glyph_lookup_[cp].supported = 1u;
                if (source->shaping_font != nullptr) {
                    color_characters_[cp] = true;
                }
            }
            cp = FT_Get_Next_Char(source->face, cp, &glyph);
        }
        for (std::uint32_t i = 0u; i < source->color_glyphs.size(); ++i) {
            if (source->color_glyphs[i]) {
                glyph_lookup_[source->glyph_offset + i].supported = 1u;
            }
        }
    }

    if (has_blur) {
        glyph_lookup_blurred_.reserve(glyph_lookup_.capacity());
        glyph_lookup_blurred_ = glyph_lookup_;
        build_weights();
    }

    prepared_ = true;

    return true;
}

bool font::build(bool initial_build)
{
    assert(frame_start_ == 0u);

    if (!prepare()) {
        return false;
    }

    constexpr wchar k_fallback_glyph = '?';

    auto ret = rasterize_glyph(k_fallback_glyph);
    fallback_glyph_ = glyph_lookup_data::k_invalid_index;
    if (!ret.failed) {
        apply_glyph(ret, initial_build /* init time */);
        private_buffer_.swap(ret.bitmap);
        if (!glyphs_.empty() &&
            glyphs_.back().codepoint == k_fallback_glyph) {
            fallback_glyph_ = static_cast<std::uint32_t>(glyphs_.size()) - 1;
        }
    }

    // build lookup table
    for (std::size_t i = 0u; i < glyphs_.size(); i++) {
        auto& g = glyphs_[i];
        if (static_cast<std::size_t>(g.codepoint) < glyph_lookup_.size()) {
            auto& e = glyph_lookup_[g.codepoint];
            e.index = static_cast<std::uint32_t>(i);
        }
    }

    return true;
}

void font::destroy()
{
    assert(!is_destroyed_);

    for (auto& glyph : glyphs_) {
        if (glyph.visible) {
            atlas_->remove_rect(glyph.rect_id);
        }
    }

    glyphs_.clear();
    glyph_lookup_.clear();
    glyph_lookup_blurred_.clear();
    free_glyph_slots_.clear();

    is_destroyed_ = true;
}

bool font::add_font(const std::uint8_t* data, std::size_t data_size)
{
    std::vector<font_range> ranges;
    ranges.emplace_back(wchar(0), unicode::codepoint_max);

    return add_font(data, data_size, std::move(ranges));
}

bool font::add_font(const std::uint8_t* data, std::size_t data_size, const std::vector<font_range>& ranges)
{
    std::vector<font_range> copy = ranges;
    return add_font(data, data_size, std::move(copy));
}

bool font::add_font(const std::uint8_t* data, std::size_t data_size, std::vector<font_range>&& ranges)
{
    auto source = std::make_unique<font_data>();
    source->data = data;
    source->size = data_size;
    source->ranges = std::move(ranges);
    return add_font_data(std::move(source));
}

bool font::add_font(std::vector<std::uint8_t> data)
{
    std::vector<font_range> ranges;
    ranges.emplace_back(wchar(0), unicode::codepoint_max);

    return add_font(std::move(data), std::move(ranges));
}

bool font::add_font(std::vector<std::uint8_t> data, const std::vector<font_range>& ranges)
{
    std::vector<font_range> copy = ranges;
    return add_font(std::move(data), std::move(copy));
}

bool font::add_font(std::vector<std::uint8_t> data, std::vector<font_range>&& ranges)
{
    auto source = std::make_unique<font_data>();
    source->owned_data = std::move(data);
    source->data = source->owned_data.data();
    source->size = source->owned_data.size();
    source->ranges = std::move(ranges);
    return add_font_data(std::move(source));
}

bool font::add_font_data(std::unique_ptr<font_data> data)
{
    assert(!prepared_);
    if (data->data == nullptr || data->size == 0u ||
        data->size > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max()) ||
        FT_Init_FreeType(&data->library) != 0 ||
        FT_New_Memory_Face(data->library, data->data, static_cast<FT_Long>(data->size), 0, &data->face) != 0 ||
        FT_Select_Charmap(data->face, FT_ENCODING_UNICODE) != 0) {
        return false;
    }

    auto* face = data->face;
    if (FT_IS_SCALABLE(face)) {
        const auto em_size = static_cast<float>(cfg_.size) * face->units_per_EM /
            static_cast<float>(face->ascender - face->descender);
        if (FT_Set_Char_Size(face,
            static_cast<FT_F26Dot6>(std::lround(em_size * cfg_.oversample_h * 64.f)),
            static_cast<FT_F26Dot6>(std::lround(em_size * cfg_.oversample_v * 64.f)), 72u, 72u) != 0) {
            return false;
        }
        data->scale_x = 1.f / cfg_.oversample_h;
        data->scale_y = 1.f / cfg_.oversample_v;
    }
    else if (face->num_fixed_sizes > 0) {
        auto strike = 0;
        const auto target = static_cast<FT_Pos>(cfg_.size * cfg_.oversample_v * 64u);
        for (auto i = 1; i < face->num_fixed_sizes; ++i) {
            if (std::abs(face->available_sizes[i].y_ppem - target) <
                std::abs(face->available_sizes[strike].y_ppem - target)) {
                strike = i;
            }
        }
        if (FT_Select_Size(face, strike) != 0) {
            return false;
        }
        data->scale_y = static_cast<float>(cfg_.size) * 64.f /
            static_cast<float>(face->size->metrics.height);
        data->scale_x = data->scale_y;
    }
    else {
        return false;
    }

    data->baseline = std::ceil(static_cast<float>(face->size->metrics.ascender) / 64.f * data->scale_y) + cfg_.offset_y;
    if (fonts_.empty()) {
        auto center_to_baseline = static_cast<float>(face->size->metrics.ascender + face->size->metrics.descender) /
            128.f * data->scale_y;
        const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
        if (FT_IS_SCALABLE(face) && os2 != nullptr && os2->version >= 2u &&
            os2->version != 0xFFFFu && os2->sCapHeight > 0) {
            center_to_baseline = static_cast<float>(FT_MulFix(os2->sCapHeight, face->size->metrics.y_scale)) /
                128.f * data->scale_y;
        }
        center_y_ = data->baseline - center_to_baseline;
    }

    if (FT_HAS_COLOR(face)) {
        auto* blob = hb_blob_create(reinterpret_cast<const char*>(data->data),
            static_cast<unsigned>(data->size), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
        auto* hb_face = hb_face_create(blob, 0u);
        data->shaping_font = hb_font_create(hb_face);
        hb_ot_font_set_funcs(data->shaping_font);
        hb_font_set_ppem(data->shaping_font, face->size->metrics.x_ppem, face->size->metrics.y_ppem);
        hb_font_make_immutable(data->shaping_font);
        hb_face_destroy(hb_face);
        hb_blob_destroy(blob);
        data->color_glyphs.resize(static_cast<std::size_t>(face->num_glyphs));
        for (std::uint32_t i = 0u; i < data->color_glyphs.size(); ++i) {
            auto* png = hb_ot_color_glyph_reference_png(data->shaping_font, i);
            data->color_glyphs[i] = hb_blob_get_length(png) != 0u;
            hb_blob_destroy(png);
        }
    }

    fonts_.push_back(std::move(data));
    return true;
}

font::shape_result font::shape_char(std::span<const char32_t> text)
{
    const std::u32string_view key(text.data(), text.size());
    const auto hash = std::hash<std::u32string_view>{}(key);
    auto& cached = shape_cache_[hash % shape_cache_.size()];
    if (cached.text == key) {
        return cached.result;
    }

    shape_result result{ text.front(), 1u };
    for (const auto& source : fonts_) {
        if (source->shaping_font == nullptr || !codepoint_in_ranges(text.front(), source->ranges)) {
            continue;
        }
        hb_buffer_clear_contents(shape_buffer_);
        hb_buffer_set_direction(shape_buffer_, HB_DIRECTION_LTR);
        hb_buffer_set_script(shape_buffer_, HB_SCRIPT_COMMON);
        hb_buffer_set_flags(shape_buffer_, HB_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES);
        for (std::uint32_t i = 0u; i < text.size(); ++i) {
            hb_buffer_add(shape_buffer_, text[i], i);
        }
        hb_buffer_set_content_type(shape_buffer_, HB_BUFFER_CONTENT_TYPE_UNICODE);
        hb_shape(source->shaping_font, shape_buffer_, nullptr, 0u);
        unsigned count{};
        const auto* glyphs = hb_buffer_get_glyph_infos(shape_buffer_, &count);
        if (count == 0u || glyphs[0].cluster != 0u) {
            continue;
        }
        auto cluster_end = 1u;
        while (cluster_end < count && glyphs[cluster_end].cluster == 0u) {
            ++cluster_end;
        }
        auto colored = true;
        for (auto i = 0u; i < cluster_end; ++i) {
            if (glyphs[i].codepoint >= source->color_glyphs.size() ||
                !source->color_glyphs[glyphs[i].codepoint]) {
                colored = false;
                break;
            }
        }
        if (!colored) {
            continue;
        }
        result.count = cluster_end < count ? glyphs[cluster_end].cluster : static_cast<std::uint32_t>(text.size());
        if (cluster_end == 1u) {
            result.codepoint = static_cast<wchar>(source->glyph_offset + glyphs[0].codepoint);
        }
        else {
            std::u32string cluster(text.data(), result.count);
            const auto found = shaped_runs_by_text_.find(cluster);
            if (found != shaped_runs_by_text_.end()) {
                result.codepoint = found->second;
            }
            else {
                auto run = std::make_unique<shaped_run>();
                const auto* positions = hb_buffer_get_glyph_positions(shape_buffer_, nullptr);
                const auto upem = static_cast<float>(hb_face_get_upem(hb_font_get_face(source->shaping_font)));
                const auto sx = source->face->size->metrics.x_ppem * source->scale_x / upem;
                const auto sy = source->face->size->metrics.y_ppem * source->scale_y / upem;
                auto x = 0.f;
                auto y = 0.f;
                for (auto i = 0u; i < cluster_end; ++i) {
                    run->glyphs.push_back(
                        {
                            static_cast<wchar>(source->glyph_offset + glyphs[i].codepoint),
                            x + positions[i].x_offset * sx, y + positions[i].y_offset * sy
                        }
                    );
                    x += positions[i].x_advance * sx;
                    y += positions[i].y_advance * sy;
                }
                run->advance_x = x;
                result.codepoint = static_cast<wchar>(glyph_lookup_.size());
                glyph_lookup_data lookup{};
                lookup.supported = 1u;
                glyph_lookup_.push_back(lookup);
                if (cfg_.glow_radius > 0u) {
                    glyph_lookup_blurred_.push_back(lookup);
                }
                shaped_runs_by_text_.emplace(std::move(cluster), result.codepoint);
                shaped_runs_.push_back(std::move(run));
            }
        }
        break;
    }
    cached.text = key;
    cached.result = result;
    return result;
}

void font::apply_glyph(const pending_glyph& pg, bool is_init_time)
{
    const std::size_t lookup_size = (pg.blurred ? glyph_lookup_blurred_.size() : glyph_lookup_.size());
    if (static_cast<std::size_t>(pg.codepoint) >=
        lookup_size) {
        return;
    }

    auto& lookup = pg.blurred ?
        glyph_lookup_blurred_[pg.codepoint] : glyph_lookup_[pg.codepoint];

    font_glyph g{};
    g.codepoint = pg.codepoint;
    g.visible = pg.visible;
    g.advance_x = pg.advance_x;
    g.x0 = pg.x0;
    g.y0 = pg.y0;
    g.x1 = pg.x1;
    g.y1 = pg.y1;
    g.last_access = frame_start_;
    g.blurred = pg.blurred;
    g.colored = !pg.color_bitmap.empty();

    if (pg.visible) {
        const auto rect_id = atlas_->register_rect(pg.bmp_w, pg.bmp_h);
        if (!rect_id) {
            lookup.loading = 0u;
            return;
        }

        g.rect_id = *rect_id;
        atlas_->get_rect_uv(g.rect_id, g.uv_min, g.uv_max);
        if (g.colored) {
            is_init_time ?
                atlas_->write_data_init(g.rect_id, pg.color_bitmap.data(), pg.color_bitmap.size()) :
                atlas_->write_data(g.rect_id, pg.color_bitmap.data(), pg.color_bitmap.size());
        }
        else {
            is_init_time ?
                atlas_->write_data_init(g.rect_id, pg.bitmap.data(), pg.bitmap.size()) :
                atlas_->write_data(g.rect_id, pg.bitmap.data(), pg.bitmap.size());
        }
    }

    std::uint32_t slot = 0u;
    if (!free_glyph_slots_.empty()) {
        slot = free_glyph_slots_.back();
        free_glyph_slots_.pop_back();
        glyphs_[slot] = std::move(g);
    } else {
        slot = static_cast<std::uint32_t>(glyphs_.size());
        glyphs_.push_back(std::move(g));
    }

    lookup.index = slot;
    lookup.loading = 0u;

    if (!is_init_time) {
        ++content_epoch_;
    }
}

void font::build_weights()
{
    kernel_weights_.resize(cfg_.glow_radius + 1u);

    const float sigma = std::max(1.f, cfg_.glow_radius / 3.f);
    const float two_sigma2 = 2.f * sigma * sigma;

    float sum = 0.f;
    for (std::uint32_t i = 0; i <= cfg_.glow_radius; ++i) {
        const float t = static_cast<float>(i);
        const float wgt = std::exp(-(t * t) / two_sigma2);
        kernel_weights_[i] = wgt;
        sum += (i == 0) ? wgt : (2.f * wgt);
    }

    const float inv_sum = 1.f / sum;
    for (std::uint32_t i = 0; i <= cfg_.glow_radius; ++i)
        kernel_weights_[i] *= inv_sum;
}

void font::blur_rect(std::uint32_t w, std::uint32_t h)
{
    assert(cfg_.glow_radius > 0u);

    const std::uint32_t stride = w;

    std::vector<float>& tmp_h = private_buffer3_;
    std::vector<float>& tmp_v = private_buffer4_;

    tmp_h.resize(static_cast<size_t>(w) * static_cast<size_t>(h));
    tmp_v.resize(static_cast<size_t>(w) * static_cast<size_t>(h));

    for (std::uint32_t row = 0; row < h; ++row) {
        const std::uint32_t iy = row;
        const std::uint8_t* src_row = private_buffer_.data() + static_cast<size_t>(iy) * stride;

        for (std::uint32_t col = 0; col < w; ++col) {
            float acc = 0.f;

            for (std::int32_t k = -static_cast<std::int32_t>(cfg_.glow_radius);
                 k <= static_cast<std::int32_t>(cfg_.glow_radius); ++k) {
                std::int32_t sx = static_cast<std::int32_t>(col) + k;
                sx = std::clamp(sx, 0, static_cast<std::int32_t>(w) - 1);

                const float wgt = kernel_weights_[static_cast<std::uint32_t>(std::abs(k))];
                acc += static_cast<float>(src_row[sx]) * wgt;
            }

            tmp_h[static_cast<size_t>(row) * w + col] = acc;
        }
    }

    for (std::uint32_t col = 0; col < w; ++col) {
        for (std::uint32_t row = 0; row < h; ++row) {
            float acc = 0.f;

            for (std::int32_t k = -static_cast<std::int32_t>(cfg_.glow_radius);
                 k <= static_cast<std::int32_t>(cfg_.glow_radius); ++k) {
                std::int32_t ry = static_cast<std::int32_t>(row) + k;
                ry = std::clamp(ry, 0, static_cast<std::int32_t>(h) - 1);

                const float wgt = kernel_weights_[static_cast<std::uint32_t>(std::abs(k))];
                acc += tmp_h[static_cast<size_t>(ry) * w + col] * wgt;
            }

            tmp_v[static_cast<size_t>(row) * w + col] = acc;
        }
    }

    for (std::uint32_t row = 0; row < h; ++row) {
        std::uint8_t* dst_row = private_buffer_.data() + static_cast<size_t>(row) * stride;

        for (std::uint32_t col = 0; col < w; ++col) {
            const float v = tmp_v[static_cast<size_t>(row) * w + col];
            const int iv = std::clamp(static_cast<int>(v + 0.5f),
                0, static_cast<int>(std::numeric_limits<std::uint8_t>::max()));
            dst_row[col] = static_cast<std::uint8_t>(iv);
        }
    }
}

void font::glow_rect(std::uint32_t w, std::uint32_t h)
{
    const std::uint32_t stride = w;

    private_buffer2_.resize(
        static_cast<std::size_t>(w) * static_cast<std::size_t>(h)
    );
    assert(private_buffer_.size() == private_buffer2_.size());
    for (std::uint32_t row = 0; row < h; ++row) {
        const std::uint8_t* src = private_buffer_.data() + static_cast<size_t>(row) * stride;
        std::memcpy(private_buffer2_.data() + static_cast<size_t>(row) * w, src, w);
    }

    blur_rect(w, h);

    for (std::uint32_t row = 0; row < h; ++row) {
        std::uint8_t* dst = private_buffer_.data() + static_cast<size_t>(row) * stride;
        const std::uint8_t* core_row = private_buffer2_.data() + static_cast<size_t>(row) * w;

        for (std::uint32_t col = 0; col < w; ++col) {
            const int core_v = core_row[col];
            const int halo_v = static_cast<int>(dst[col] * cfg_.glow_strength + 0.5f);

            const int out_v = std::clamp(std::max(core_v, halo_v),
                0, static_cast<int>(std::numeric_limits<std::uint8_t>::max()));
            dst[col] = static_cast<std::uint8_t>(out_v);
        }
    }
}

font_data* font::get_font_data_for_char(wchar c) const noexcept
{
    for (const auto& source : fonts_) {
        if (source->shaping_font != nullptr && c >= source->glyph_offset &&
            c - source->glyph_offset < source->color_glyphs.size()) {
            return source.get();
        }
        if (codepoint_in_ranges(c, source->ranges) && FT_Get_Char_Index(source->face, c) != 0u) {
            return source.get();
        }
    }
    return nullptr;
}

void font::queue_glyph(wchar glyph, bool blurred)
{
    const auto* run = glyph >= shaped_glyph_start_ ? shaped_runs_[glyph - shaped_glyph_start_].get() : nullptr;
    std::lock_guard<std::mutex> lock(request_mutex_);
    glyphs_to_rasterize_.emplace_back(glyph, blurred, run);
}

pending_glyph font::rasterize_run(wchar c, const shaped_run& run, bool blurred)
{
    pending_glyph result{};
    result.codepoint = c;
    result.blurred = blurred;
    result.advance_x = run.advance_x;
    std::vector<pending_glyph> glyphs;
    glyphs.reserve(run.glyphs.size());
    for (const auto& part : run.glyphs) {
        auto glyph = rasterize_glyph(part.codepoint);
        if (glyph.failed || glyph.color_bitmap.empty()) {
            result.failed = true;
            return result;
        }
        const auto x = std::round(part.x);
        const auto y = std::round(part.y);
        glyph.x0 += x;
        glyph.x1 += x;
        glyph.y0 -= y;
        glyph.y1 -= y;
        if (glyphs.empty()) {
            result.x0 = glyph.x0;
            result.y0 = glyph.y0;
            result.x1 = glyph.x1;
            result.y1 = glyph.y1;
        }
        else {
            result.x0 = std::min(result.x0, glyph.x0);
            result.y0 = std::min(result.y0, glyph.y0);
            result.x1 = std::max(result.x1, glyph.x1);
            result.y1 = std::max(result.y1, glyph.y1);
        }
        glyphs.push_back(std::move(glyph));
    }
    result.bmp_w = static_cast<std::uint32_t>(result.x1 - result.x0);
    result.bmp_h = static_cast<std::uint32_t>(result.y1 - result.y0);
    result.color_bitmap.resize(static_cast<std::size_t>(result.bmp_w) * result.bmp_h);
    for (const auto& glyph : glyphs) {
        const auto dx = static_cast<std::uint32_t>(glyph.x0 - result.x0);
        const auto dy = static_cast<std::uint32_t>(glyph.y0 - result.y0);
        for (auto y = 0u; y < glyph.bmp_h; ++y) {
            for (auto x = 0u; x < glyph.bmp_w; ++x) {
                const auto src = glyph.color_bitmap[y * glyph.bmp_w + x];
                auto& dst = result.color_bitmap[(dy + y) * result.bmp_w + dx + x];
                const auto sa = src >> 24u;
                const auto da = dst >> 24u;
                const auto alpha = sa + (da * (255u - sa) + 127u) / 255u;
                auto pixel = alpha << 24u;
                if (alpha > 0u) {
                    for (auto shift = 0u; shift < 24u; shift += 8u) {
                        const auto sc = (src >> shift) & 255u;
                        const auto dc = (dst >> shift) & 255u;
                        const auto value = (sc * sa + (dc * da * (255u - sa) + 127u) / 255u + alpha / 2u) / alpha;
                        pixel |= value << shift;
                    }
                }
                dst = pixel;
            }
        }
    }
    if (blurred) {
        const auto padding = cfg_.glow_radius;
        const auto width = result.bmp_w + padding * 2u;
        const auto height = result.bmp_h + padding * 2u;
        private_buffer_.assign(static_cast<std::size_t>(width) * height, 0u);
        for (auto y = 0u; y < result.bmp_h; ++y) {
            for (auto x = 0u; x < result.bmp_w; ++x) {
                private_buffer_[(y + padding) * width + x + padding] =
                    static_cast<std::uint8_t>(result.color_bitmap[y * result.bmp_w + x] >> 24u);
            }
        }
        glow_rect(width, height);
        private_buffer_.swap(result.bitmap);
        result.color_bitmap.clear();
        result.x0 -= padding;
        result.y0 -= padding;
        result.x1 += padding;
        result.y1 += padding;
        result.bmp_w = width;
        result.bmp_h = height;
    }
    result.visible = true;
    return result;
}

pending_glyph font::rasterize_glyph(wchar c, bool blurred, const shaped_run* run)
{
    if (run != nullptr) {
        return rasterize_run(c, *run, blurred);
    }
    assert(!blurred || cfg_.glow_radius > 0u);

    pending_glyph result{};
    result.codepoint = c;
    result.blurred = blurred;
    auto* source = get_font_data_for_char(c);
    if (source == nullptr) {
        result.failed = true;
        return result;
    }
    auto* face = source->face;
    const auto glyph = c > unicode::codepoint_max ? c - source->glyph_offset : FT_Get_Char_Index(face, c);
    if (FT_Load_Glyph(face, glyph, FT_LOAD_RENDER | FT_LOAD_COLOR | FT_LOAD_NO_HINTING | FT_LOAD_NO_SVG) != 0) {
        result.failed = true;
        return result;
    }

    const auto* slot = face->glyph;
    const auto& bitmap = slot->bitmap;
    const auto sx = source->scale_x;
    const auto sy = source->scale_y;
    result.advance_x = static_cast<float>(slot->advance.x) / 64.f * sx;
    if (bitmap.width == 0u || bitmap.rows == 0u) {
        return result;
    }
    if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY && bitmap.pixel_mode != FT_PIXEL_MODE_MONO &&
        bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
        result.failed = true;
        return result;
    }

    const auto x0 = std::floor(slot->bitmap_left * sx);
    const auto y0 = std::floor(-slot->bitmap_top * sy);
    const auto x1 = std::ceil((slot->bitmap_left + static_cast<int>(bitmap.width)) * sx);
    const auto y1 = std::ceil((static_cast<int>(bitmap.rows) - slot->bitmap_top) * sy);
    const auto width = static_cast<int>(x1 - x0);
    const auto height = static_cast<int>(y1 - y0);
    const auto padding = blurred ? static_cast<int>(cfg_.glow_radius) : 0;
    const auto out_width = width + padding * 2;
    const auto out_height = height + padding * 2;
    const auto colored = bitmap.pixel_mode == FT_PIXEL_MODE_BGRA && !blurred;
    const auto channels = colored ? 4u : 1u;
    private_buffer2_.resize(static_cast<std::size_t>(bitmap.width) * bitmap.rows * channels);
    for (std::uint32_t y = 0u; y < bitmap.rows; ++y) {
        const auto* row = bitmap.buffer + (bitmap.pitch >= 0 ? y : bitmap.rows - y - 1u) * std::abs(bitmap.pitch);
        auto* dst = private_buffer2_.data() + static_cast<std::size_t>(y) * bitmap.width * channels;
        if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
            std::copy_n(row, bitmap.width, dst);
            continue;
        }
        for (std::uint32_t x = 0u; x < bitmap.width; ++x) {
            if (colored) {
                const auto alpha = row[x * 4u + 3u];
                for (std::uint32_t channel = 0u; channel < 3u; ++channel) {
                    dst[x * 4u + channel] = alpha == 0u ? 0u : static_cast<std::uint8_t>(
                        std::min(255u, (row[x * 4u + 2u - channel] * 255u + alpha / 2u) / alpha)
                    );
                }
                dst[x * 4u + 3u] = alpha;
            }
            else if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
                dst[x] = row[x * 4u + 3u];
            }
            else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                dst[x] = (row[x / 8u] & (0x80u >> (x % 8u))) != 0u ? 255u : 0u;
            }
            else {
                dst[x] = row[x];
            }
        }
    }

    std::uint8_t* pixels{};
    if (colored) {
        result.color_bitmap.resize(static_cast<std::size_t>(out_width) * out_height);
        pixels = reinterpret_cast<std::uint8_t*>(result.color_bitmap.data());
    }
    else {
        private_buffer_.assign(static_cast<std::size_t>(out_width) * out_height, 0u);
        pixels = private_buffer_.data() + padding * out_width + padding;
    }
    STBIR_RESIZE resize{};
    stbir_resize_init(
        &resize,
        private_buffer2_.data(),
        static_cast<int>(bitmap.width),
        static_cast<int>(bitmap.rows),
        0,
        pixels,
        width,
        height,
        out_width * static_cast<int>(channels),
        colored ? STBIR_RGBA : STBIR_1CHANNEL,
        STBIR_TYPE_UINT8
    );
    stbir_set_edgemodes(&resize, STBIR_EDGE_ZERO, STBIR_EDGE_ZERO);
    stbir_set_input_subrect(
        &resize,
        (static_cast<double>(x0) / sx - slot->bitmap_left) / bitmap.width,
        (static_cast<double>(y0) / sy + slot->bitmap_top) / bitmap.rows,
        (static_cast<double>(x1) / sx - slot->bitmap_left) / bitmap.width,
        (static_cast<double>(y1) / sy + slot->bitmap_top) / bitmap.rows
    );
    if (!colored) {
        stbir_set_filters(&resize, STBIR_FILTER_BOX, STBIR_FILTER_BOX);
    }
    if (!stbir_resize_extended(&resize)) {
        result.failed = true;
        return result;
    }
    if (blurred) {
        glow_rect(static_cast<std::uint32_t>(out_width), static_cast<std::uint32_t>(out_height));
    }
    result.x0 = x0 + cfg_.offset_x - padding;
    result.y0 = source->baseline + y0 - padding;
    result.x1 = result.x0 + out_width;
    result.y1 = result.y0 + out_height;
    result.bmp_w = static_cast<std::uint32_t>(out_width);
    result.bmp_h = static_cast<std::uint32_t>(out_height);
    result.visible = true;
    if (!colored) {
        private_buffer_.swap(result.bitmap);
    }
    return result;
}

r2_end_