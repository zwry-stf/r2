#pragma once
#include <assert.h>


r2_begin_

template <typename T>
struct get_native_t;

template <>
struct get_native_t<blendstate*> {
    using type = class d3d11_blendstate*;
};

template <>
struct get_native_t<buffer*> {
    using type = class d3d11_buffer*;
};

template <>
struct get_native_t<depthstencilstate*> {
    using type = class d3d11_depthstencilstate*;
};

template <>
struct get_native_t<inputlayout*> {
    using type = class d3d11_inputlayout*;
};

template <>
struct get_native_t<pixelshader*> {
    using type = class d3d11_pixelshader*;
};

template <>
struct get_native_t<rasterizerstate*> {
    using type = class d3d11_rasterizerstate*;
};

template <>
struct get_native_t<sampler*> {
    using type = class d3d11_sampler*;
};

template <>
struct get_native_t<shaderprogram*> {
    using type = class d3d11_shaderprogram*;
};

template <>
struct get_native_t<texture2d*> {
    using type = class d3d11_texture2d*;
};

template <>
struct get_native_t<textureview*> {
    using type = class d3d11_textureview*;
};

template <>
struct get_native_t<vertexshader*> {
    using type = class d3d11_vertexshader*;
};

template <>
struct get_native_t<framebuffer*> {
    using type = class d3d11_framebuffer*;
};

template <typename T>
inline auto to_native(T* v) {
#if defined(_DEBUG)
    auto* ret = dynamic_cast<get_native_t<T*>::type>(v);
    assert(ret != nullptr || v == nullptr);
    return ret;
#else
    return reinterpret_cast<get_native_t<T*>::type>(v);
#endif
}

r2_end_