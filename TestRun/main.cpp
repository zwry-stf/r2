#include <string>
#include <thread>
#include <atomic>
#include <format>
#include <utility>
#include <memory>
#include <format>
#include "resources.h"


// platform dependent includes
#if defined(R2_BACKEND_D3D11)
#include <backend/d3d11/texture2d.h>
#endif

#if defined(R2_PLATFORM_WINDOWS)
#include <Windows.h>
#endif
#include <GLFW/glfw3.h>

#if defined(R2_PLATFORM_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <r2/renderer.h>
#include <r2/error.h>


// Global data
struct GlobalRenderData
{
#if defined(R2_BACKEND_D3D11)
    d3d_pointer<IDXGISwapChain> swapchain;
    d3d_pointer<ID3D11Device> device;
    d3d_pointer<ID3D11DeviceContext> context;
#endif
    std::unique_ptr<r2::textureview> render_target_view;
    std::unique_ptr<r2::framebuffer> render_target;
};

struct WindowData
{
    int fb_width{};
    int fb_height{};
    GLFWwindow* window{};
    std::atomic<bool> needs_resize{ false };
    int pending_resize_width{};
    int pending_resize_height{};
};

struct GlobalData
{
    std::atomic<bool> running{ false };
    WindowData window_data;
    GlobalRenderData render_data;
};

inline static GlobalData g_data;

inline static r2::renderer2d g_renderer;
inline static r2::font* g_font;


// function def
bool create_window(const std::string& title);
bool initialize_backend();
bool create_render_target();
template<typename... Args>
void show_error_and_exit(std::format_string<Args...> f, Args&&... args);
void on_resize(GLFWwindow* window, int width, int height);
void render_thread();
void destroy_backend();
void destroy_window();


// main
#if defined(R2_PLATFORM_WINDOWS)
int __stdcall WinMain(HINSTANCE /* instance */,
            HINSTANCE /* prev instance */,
            LPSTR /* cmd line */,
            int /* show/hide */)
{
#endif

    if (!create_window("r2"))
        show_error_and_exit("failed to create window");

    if (!initialize_backend())
        show_error_and_exit("failed to initialize backend");

    // r2
#if defined(R2_BACKEND_D3D11)
    r2::backend_init_data binit(g_data.render_data.swapchain.get());
#elif defined(R2_BACKEND_OPENGL)
    r2::backend_init_data binit;
#endif

#if defined(R2_PLATFORM_WINDOWS)
    r2::platform_init_data pinit(glfwGetWin32Window(g_data.window_data.window));
#endif

    try {
        g_renderer.init(pinit, binit);

        r2::font_cfg fcfg{};
        fcfg.size = 20u;
        fcfg.oversample_h = 2u;
        fcfg.oversample_v = 2u;
        fcfg.glow_radius = 10u;
        fcfg.glow_strength = 2.f;
        auto* f = g_renderer.add_font(fcfg);
        g_font = f;
        f->add_font(NotoSans_Medium, NotoSans_Medium_size);
        f->add_font(MPLUSRounded1c_Medium, MPLUSRounded1c_Medium_size);
        f->add_font(NotoEmoji_Medium, NotoEmoji_Medium_size);

        g_renderer.build_fonts();
        g_renderer.create_font_texture();
    }
    catch (const r2::error& e) {
        show_error_and_exit("renderer initialization failed: {}", e.to_string());
    }

    if (!create_render_target())
        show_error_and_exit("failed to create render target");

    // run
    glfwShowWindow(g_data.window_data.window);

#if defined(R2_BACKEND_OPENGL)
    glfwMakeContextCurrent(nullptr);
#endif

    g_data.running.store(true, std::memory_order_release);

    std::thread t(render_thread);

    while (g_data.running.load(std::memory_order_acquire)) {
        glfwPollEvents();

        if (glfwWindowShouldClose(g_data.window_data.window))
            break;
    }

    g_data.running.store(false, std::memory_order_release);

    if (t.joinable())
        t.join();

    g_renderer.destroy();

    destroy_backend();

    destroy_window();

    return 0;
}

// implementations
bool create_window(const std::string& title)
{
    int res = glfwInit();
    if (res != GLFW_TRUE)
        return false;

#if defined(R2_BACKEND_D3D11)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#elif defined(R2_BACKEND_OPENGL)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    g_data.window_data.window = glfwCreateWindow(1400, 900, title.c_str(), nullptr, nullptr);
    if (g_data.window_data.window == nullptr)
        return false;

#if defined(R2_BACKEND_OPENGL)
    glfwMakeContextCurrent(g_data.window_data.window);
#endif

    glfwGetFramebufferSize(g_data.window_data.window,
        &g_data.window_data.fb_width, &g_data.window_data.fb_height);

    glfwSetFramebufferSizeCallback(g_data.window_data.window, on_resize);

    return true;
}

bool initialize_backend()
{
#if defined(R2_BACKEND_D3D11)
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount                        = 2;
    sd.BufferDesc.Width                   = 0;
    sd.BufferDesc.Height                  = 0;
    sd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    sd.OutputWindow                       = glfwGetWin32Window(g_data.window_data.window);
    sd.SampleDesc.Count                   = 1;
    sd.SampleDesc.Quality                 = 0;
    sd.Windowed                           = TRUE;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT create_device_flags = 0;
#ifdef _DEBUG
    create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_level;
    const D3D_FEATURE_LEVEL feature_level_array[2] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        create_device_flags,
        feature_level_array,
        2,
        D3D11_SDK_VERSION,
        &sd,
        g_data.render_data.swapchain.address_of(),
        g_data.render_data.device.address_of(),
        &feature_level,
        g_data.render_data.context.address_of()
    );

    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            create_device_flags,
            feature_level_array,
            2,
            D3D11_SDK_VERSION,
            &sd,
            g_data.render_data.swapchain.address_of(),
            g_data.render_data.device.address_of(),
            &feature_level,
            g_data.render_data.context.address_of()
        );
    }

    if (FAILED(res))
        return false;
#endif

    return true;
}

bool create_render_target()
{
    auto b = g_renderer.context()->get_backbuffer();

    r2::textureview_desc rdesc{};
    rdesc.usage = r2::view_usage::render_target;

    g_data.render_data.render_target_view = g_renderer.context()->create_textureview(
        b, rdesc);
    if (g_data.render_data.render_target_view->has_error())
        return false;

    r2::framebuffer_desc fdesc{};
    fdesc.color_attachment.view = g_data.render_data.render_target_view.get();
    g_data.render_data.render_target = g_renderer.context()->create_framebuffer(fdesc);
    if (g_data.render_data.render_target->has_error())
        return false;
    
    return true;
}

template<typename ...Args>
void show_error_and_exit(std::format_string<Args...> f, Args && ...args)
{
    std::string msg = std::format(f, std::forward<Args>(args)...);

#if defined(R2_PLATFORM_WINDOWS)
    MessageBoxA(NULL, msg.c_str(), "r2 - error", MB_OK | MB_ICONERROR);
#endif

    std::abort();
}

void on_resize(GLFWwindow* window, int width, int height)
{
    if (window != g_data.window_data.window)
        return;

    if (g_data.window_data.fb_width == width &&
        g_data.window_data.fb_height == height)
        return;

    if (width == 0 &&
        height == 0)
        return;

    g_data.window_data.pending_resize_width = width;
    g_data.window_data.pending_resize_height = height;

    g_data.window_data.needs_resize.store(true, std::memory_order_release);
}

bool resize(int width, int height)
{
#if defined(R2_BACKEND_D3D11)
    g_data.render_data.context->ClearState();
    g_data.render_data.context->Flush();
#endif

    g_data.render_data.render_target_view.reset();
    g_data.render_data.render_target.reset();

    g_renderer.pre_resize();

#if defined(R2_BACKEND_D3D11)
    HRESULT hr = g_data.render_data.swapchain->ResizeBuffers(
        0u,
        static_cast<UINT>(width),
        static_cast<UINT>(height),
        DXGI_FORMAT_UNKNOWN,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
    );
    if (FAILED(hr))
        return false;
#endif

    g_renderer.post_resize();

    if (!create_render_target())
        return false;

    g_data.window_data.fb_width = width;
    g_data.window_data.fb_height = height;

    return true;
}

void render_frame()
{
    static bool changed = false;
    static bool change_font = false;
    if (!changed && change_font) {
        g_renderer.remove_font(g_font);

        changed = true;
        r2::font_cfg fcfg{};
        fcfg.size = 50u;
        fcfg.oversample_h = 2u;
        fcfg.oversample_v = 2u;
        fcfg.glow_radius = 10u;
        fcfg.glow_strength = 2.f;
        auto* f = g_renderer.add_font(fcfg);

        f->add_font(NotoSans_Medium, NotoSans_Medium_size);
        f->add_font(MPLUSRounded1c_Medium, MPLUSRounded1c_Medium_size);
        f->add_font(NotoEmoji_Medium, NotoEmoji_Medium_size);

        f->build();
    }

    g_renderer.reset_render_data();
    g_renderer.setup_render_state();

    // test
    {
        {
            g_renderer.push_clip_rect(
                r2::rect(
                    1000, 200,
                    1200,
                    static_cast<std::int32_t>(g_renderer.get_render_size().y)
                )
            );

            g_renderer.add_quad_filled_multicolor(
                r2::vec2{ 800.f, 100.f },
                r2::vec2{ 800.f, 500.f },
                r2::vec2{ 1300.f, 500.f },
                r2::vec2{ 1300.f, 100.f },
                r2::color::white(),
                r2::color::red(),
                r2::color::blue(),
                r2::color::blue()
            );

            g_renderer.pop_clip_rect();
        }

        g_renderer.add_line(r2::vec2(200.f, 200.f), r2::vec2(700.f, 600.f), r2::color::black(), 6.f);
        g_renderer.add_line(r2::vec2(200.f, 200.f), r2::vec2(700.f, 600.f), r2::color::white(), 4.f);

        g_renderer.add_line(r2::vec2(200.f, 200.f), r2::vec2(200.f, 600.f), r2::color::black(), 3.f);
        g_renderer.add_line(r2::vec2(200.f, 200.f), r2::vec2(200.f, 600.f), r2::color::white(), 1.f);

        const r2::vec2 points[] = {
            r2::vec2(500.f, 500.f),
            r2::vec2(800.f, 450.f),
            r2::vec2(780.f, 200.f),
            r2::vec2(600.f, 130.f),
            r2::vec2(450.f, 240.f),
        };

        g_renderer.add_convex_filled(
            points, sizeof(points) / sizeof(points[0]),
            r2::color::cyan().interp(r2::color::black(), 0.5f).interp(r2::color::white(), 0.3f).alpha(0.3f)
        );

        auto vtx_index = g_renderer.vertex_ptr();
        g_renderer.add_shadow_rect_filled(
            r2::vec2(600.f, 400.f),
            r2::vec2(900.f, 600.f),
            r2::color::white(),
            20.f
        );
        g_renderer.shade_vertices_col(
            vtx_index,
            g_renderer.vertex_ptr(),
            r2::vec2(500.f, 300.f),
            r2::vec2(1000.f, 700.f),
            r2::color::white(),
            r2::color::blue(),
            r2::color::cyan(),
            r2::color::purple()
        );

        vtx_index = g_renderer.vertex_ptr();
        g_renderer.add_rect(
            r2::vec2(600.f, 700.f),
            r2::vec2(900.f, 900.f),
            r2::color::white(),
            2.f,
            20.f,
            r2::e_rounding_flags::rounding_top | r2::e_rounding_flags::rounding_bottomright
        );
        g_renderer.shade_vertices_col(
            vtx_index,
            g_renderer.vertex_ptr(),
            r2::vec2(500.f, 600.f),
            r2::vec2(1000.f, 1000.f),
            r2::color::white(),
            r2::color::blue(),
            r2::color::cyan(),
            r2::color::purple()
        );

        g_renderer.add_quad_filled(
            r2::vec2(300.f, 300.f),
            r2::vec2(400.f, 700.f),
            r2::vec2(1000.f, 800.f),
            r2::vec2(900.f, 400.f),
            (r2::color::green() + r2::color::blue()).interp(r2::color::black(), 0.5f).alpha(0.2f)
        );

        auto test_str = std::u8string_view(u8"Hello World! abcikawhfioawhf");
        float width = g_renderer.get_text_width(test_str);
        (void)width;
        g_renderer.add_text_faded(
            r2::vec2(500.f, 300.f),
            r2::color::blue().interp(r2::color::white(), 0.4f).interp(r2::color::green(), 0.3f),
            r2::color::red(),
            500.f, 800.f,
            test_str,
            true
        );

        g_renderer.add_text(
            r2::vec2(300.f, 300.f),
            r2::color::blue().interp(r2::color::white(), 0.4f).interp(r2::color::green(), 0.3f),
            std::u8string_view(u8"Ä*+**''Ä")
        );

        g_renderer.add_line_multicolor(
            r2::vec2(200.f, 200.f),
            r2::vec2(700.f, 500.f),
            r2::color::red(),
            r2::color::blue(),
            20.f
        );
    }

    // fps 
    {
        constexpr float kUpdateTime = 0.5f;
        static int counted = 0;
        static float current = 0.f;
        static std::chrono::steady_clock::time_point last_update;

        auto time_now = std::chrono::steady_clock::now();
        const float elapsed = std::chrono::duration<float>(time_now - last_update).count();
        if (elapsed > kUpdateTime) {
            current = static_cast<float>(counted) / elapsed;

            counted = 0;
            last_update = time_now;
        }

        counted++;

        auto s = std::format("{:.2f}", current);

        g_renderer.add_text(
            r2::vec2(10.f, 10.f),
            r2::color::cyan(),
            s
        );
    }

    if (GetAsyncKeyState(VK_F7) & 0x8000) {
        g_renderer.add_text(
            r2::vec2(750.f, 200.f),
            r2::color::yellow(),
            std::u8string_view(u8"→😭😂🤓😘→")
        );
    }

    if (GetAsyncKeyState(VK_F8) & 0x8000) {
        g_renderer.add_text(
            r2::vec2(750.f, 200.f),
            r2::color::yellow(),
            std::u8string_view(u8"💔💔🤑🐒")
        );
        change_font = true;
    }

    g_renderer.render();

    g_renderer.update_fonts_on_frame();
}

void render_thread()
{
#if defined(R2_BACKEND_OPENGL)
    glfwMakeContextCurrent(g_data.window_data.window);
#endif 

#if defined(_DEBUG)
    g_renderer.set_render_thread(std::this_thread::get_id());
#endif

    while (g_data.running.load(std::memory_order_acquire)) {
        // resize
        if (g_data.window_data.needs_resize.load(std::memory_order_acquire)) {
            bool ret = resize(
                g_data.window_data.pending_resize_width,
                g_data.window_data.pending_resize_height
            );

            if (!ret) {
                g_data.running.store(false, std::memory_order_release);
                return;
            }

            g_data.window_data.needs_resize.store(false, std::memory_order_release);
        }

        // viewport
        g_renderer.context()->set_framebuffer(
            g_data.render_data.render_target.get());

        g_renderer.context()->set_viewport({
            0.f, 0.f,
            static_cast<float>(g_data.window_data.fb_width),
            static_cast<float>(g_data.window_data.fb_height)
            }
        );
        
        // clear
        g_renderer.context()->clear_framebuffer(
            g_data.render_data.render_target.get());

        // render
        render_frame();

        // present
#if defined(R2_BACKEND_D3D11)
        g_data.render_data.swapchain->Present(0u, DXGI_PRESENT_ALLOW_TEARING);
#elif defined(R2_BACKEND_OPENGL)
        glfwSwapBuffers(g_data.window_data.window);
#endif
    }

    g_data.running.store(false, std::memory_order_release);
}

void destroy_backend()
{
#if defined(R2_BACKEND_D3D11)
    if (g_data.render_data.context) {
        g_data.render_data.context->ClearState();
        g_data.render_data.context->Flush();

        g_data.render_data.context.reset();
    }
    g_data.render_data.device.reset();
    g_data.render_data.swapchain.reset();
#endif
}

void destroy_window()
{
    if (g_data.window_data.window != nullptr) {
        glfwDestroyWindow(g_data.window_data.window);

        g_data.window_data.window = nullptr;
    }

    glfwTerminate();
}