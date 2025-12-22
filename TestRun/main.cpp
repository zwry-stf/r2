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
	std::unique_ptr<r2::texture2d> back_buffer;
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

	if (!create_window("vane - v3"))
		show_error_and_exit("failed to create window");

	if (!initialize_backend())
		show_error_and_exit("failed to initialize backend");

	// vane
#if defined(R2_BACKEND_D3D11)
	r2::context_init_data init_data(g_data.render_data.swapchain.get());
#elif defined(R2_BACKEND_OPENGL)
	r2::context_init_data init_data;
#endif

	try {
		g_renderer.init(init_data);

		r2::font_cfg fcfg{};
		fcfg.size = 50u;
		fcfg.oversample_h = 2;
		fcfg.oversample_v = 2;
		fcfg.glow_radius = 10u;
		fcfg.glow_strength = 2.f;
		auto* f = g_renderer.add_font(fcfg);

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

	g_renderer.update_display_size(r2::vec2(
		static_cast<float>(g_data.window_data.fb_width),
		static_cast<float>(g_data.window_data.fb_height)
	));

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
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
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
	sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	sd.OutputWindow                       = glfwGetWin32Window(g_data.window_data.window);
	sd.SampleDesc.Count                   = 1;
	sd.SampleDesc.Quality                 = 0;
	sd.Windowed                           = TRUE;
	sd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

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
#if defined(R2_BACKEND_D3D11)
	// render target
	d3d_pointer<ID3D11Texture2D> back_buffer;
	HRESULT res = g_data.render_data.swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&back_buffer);
	if (FAILED(res))
		return false;

	g_data.render_data.back_buffer = r2::d3d11_texture2d::from_existing(
		reinterpret_cast<r2::d3d11_context*>(g_renderer.context()), back_buffer.get());
	if (g_data.render_data.back_buffer->has_error())
		return false;
#endif

	r2::textureview_desc rdesc{};
	rdesc.usage = r2::view_usage::render_target;

	g_data.render_data.render_target_view = g_renderer.context()->create_textureview(
		g_data.render_data.back_buffer.get(), rdesc);
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
	MessageBoxA(NULL, msg.c_str(), "Vane - Error", MB_OK | MB_ICONERROR);
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

	g_data.render_data.back_buffer.reset();
	g_data.render_data.render_target_view.reset();
	g_data.render_data.render_target.reset();

	g_renderer.pre_resize();

#if defined(R2_BACKEND_D3D11)
	HRESULT hr = g_data.render_data.swapchain->ResizeBuffers(
		0u,
		static_cast<UINT>(width),
		static_cast<UINT>(height),
		DXGI_FORMAT_UNKNOWN,
		0u
	);
	if (FAILED(hr))
		return false;
#endif

	if (!create_render_target())
		return false;

	g_renderer.post_resize();

	g_renderer.update_display_size(r2::vec2(
		static_cast<float>(width),
		static_cast<float>(height)
	));

	g_data.window_data.fb_width = width;
	g_data.window_data.fb_height = height;

	return true;
}

void render_frame()
{
	g_renderer.on_frame();

	g_renderer.reset_render_data();
	g_renderer.setup_render_state();

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
			r2::vec2(700.f, 200.f),
			r2::color::yellow(),
			std::u8string_view(u8"→😭😂🤓😘→")
		);
	}

	if (GetAsyncKeyState(VK_F8) & 0x8000) {
		g_renderer.add_text(
			r2::vec2(700.f, 200.f),
			r2::color::yellow(),
			std::u8string_view(u8"💔💔🤑🐒")
		);
	}

	g_renderer.render();
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
		g_data.render_data.swapchain->Present(0u, 0u);
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