# r2 – Lightweight 2D Renderer

Modern immediate-mode 2D renderer with font atlas, Unicode support, rounded shapes, shadows, multi-color gradients, clipping and thread-safe font updates.

### Features

- Immediate-mode API
- High-quality font rendering (subpixel, glow, oversampling)
- Unicode + emoji support via font stacking
- Rects, rounded rects, lines, convex polygons, images
- Multi-color gradients & vertex color shading
- Drop shadows (rect & convex)
- Path builder (arcs, rounded rects, fill/stroke)
- Clip rectangle stack
- Texture & font stack
- D3D11 / OpenGL backend support
- Window resize handling
- Thread-safe font atlas updates

### Quick Example

```cpp
r2::renderer2d renderer;
renderer.init(platform_data, backend_data);

// Add font with fallback + emoji
auto* font = renderer.add_font({ .size = 20, .oversample_h = 2, .glow_radius = 8 });
font->add_font(NotoSans_Regular, NotoSans_Regular_size);
font->add_font(NotoEmoji_Regular, NotoEmoji_Regular_size);
renderer.build_fonts();
renderer.create_font_texture();

while (running) {
    renderer.reset_render_data();

    renderer.add_rect_filled(
        { 100,100 }, /* min */
        { 400,300 }, /* max */
        r2::color::white().interp(r2::color::blue(), 0.5f), /* color */
        12.f /* rounding */
    );
    renderer.add_text(
        { 120, 140}, /* pos */
        r2::color::white(), /* color */
        std::u8string_view("Hello р2 😎🚀"), /* test, any string type supported */
        false /* blurred */
    );

    renderer.render();
    renderer.update_fonts_on_frame();
}
```

### Basic Usage Flow

```cpp
renderer.init(...);
auto* font = renderer.add_font(cfg);
font->add_font(...);           // can call multiple times
renderer.build_fonts();
renderer.create_font_texture();

// per frame
renderer.reset_render_data();
renderer.setup_render_state();

// draw calls ...
renderer.add_rect_filled(...);
renderer.add_text(...);
renderer.add_image(...);
...

// submit
renderer.render();
renderer.update_fonts_on_frame();   // call every frame (must be called from render thread)
```

### Important Methods

- `init()`, `destroy()`
- `add_font()`, `build_fonts()`, `create_font_texture()`
- `reset_render_data()`, `render()`
- `update_fonts_on_frame()`
- `push/pop_clip_rect()`, `push/pop_font()`, `push/pop_texture_id()`
- `add_rect_filled()`, `add_rect()`, `add_line()`, `add_convex_filled()`
- `add_text()`, `add_text_outlined()`, `add_text_faded()`
- `path_clear()`, `path_rect()`, `path_fill_convex()`, `path_stroke()`

```
