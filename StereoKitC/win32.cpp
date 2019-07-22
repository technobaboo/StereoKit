#include "win32.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_2.h>

#include "stereokit.h"

#include "render.h"
#include "rendertarget.h"
#include "d3d.h"
#include "input.h"

HWND             win32_window = nullptr;
rendertarget_t   win32_target = {};
IDXGISwapChain1 *win32_swapchain = {};
int              win32_input_pointers[2];

void win32_resize(int width, int height) {
	if (width == d3d_screen_width || height == d3d_screen_height)
		return;
	d3d_screen_width  = width;
	d3d_screen_height = height;

	if (win32_swapchain != nullptr) {
		rendertarget_release(win32_target);
		win32_swapchain->ResizeBuffers(0, (UINT)d3d_screen_width, (UINT)d3d_screen_height, DXGI_FORMAT_UNKNOWN, 0);
		ID3D11Texture2D *back_buffer;
		win32_swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
		rendertarget_set_surface(win32_target, back_buffer);
	}
}

bool win32_init(const char *app_name) {
	MSG      msg     = {0};
	WNDCLASS wc      = {0}; 
	wc.lpfnWndProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		switch(message) {
		case WM_CLOSE:     sk_run     = false; PostQuitMessage(0); break;
		case WM_SETFOCUS:  sk_focused = true;  break;
		case WM_KILLFOCUS: sk_focused = false; break;
		case WM_SIZE:       if (wParam != SIZE_MINIMIZED) win32_resize((UINT)LOWORD(lParam), (UINT)HIWORD(lParam)); break;
		case WM_SYSCOMMAND: if ((wParam & 0xfff0) == SC_KEYMENU) return (LRESULT)0; // Disable alt menu
		default: return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return (LRESULT)0;
	};
	wc.hInstance     = GetModuleHandle(NULL);
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	wc.lpszClassName = app_name;
	if( !RegisterClass(&wc) ) return false;
	win32_window = CreateWindow(wc.lpszClassName, app_name, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 20, 20, 640, 480, 0, 0, wc.hInstance, nullptr);
	if( !win32_window ) return false;

	// Create a swapchain for the window
	DXGI_SWAP_CHAIN_DESC1 sd = { };
	sd.BufferCount = 2;
	sd.Width       = d3d_screen_width;
	sd.Height      = d3d_screen_height;
	sd.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
	sd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.SampleDesc.Count = 1;
	
	IDXGIDevice2  *dxgi_device;  d3d_device  ->QueryInterface(__uuidof(IDXGIDevice2),  (void **)&dxgi_device);
	IDXGIAdapter  *dxgi_adapter; dxgi_device ->GetParent     (__uuidof(IDXGIAdapter),  (void **)&dxgi_adapter);
	IDXGIFactory2 *dxgi_factory; dxgi_adapter->GetParent     (__uuidof(IDXGIFactory2), (void **)&dxgi_factory);

	dxgi_factory->CreateSwapChainForHwnd(d3d_device, win32_window, &sd, nullptr, nullptr, &win32_swapchain);
	ID3D11Texture2D *back_buffer;
	win32_swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
	rendertarget_set_surface(win32_target, back_buffer);

	dxgi_factory->Release();
	dxgi_adapter->Release();
	dxgi_device ->Release();

	win32_input_pointers[0] = input_add_pointer((pointer_source_)(pointer_source_gaze | pointer_source_gaze_cursor | pointer_source_can_press));
	win32_input_pointers[1] = input_add_pointer((pointer_source_)(pointer_source_gaze | pointer_source_gaze_head));
	return true;
}
void win32_shutdown() {
	rendertarget_release(win32_target);
	win32_swapchain->Release();
}

void win32_step_begin() {
	MSG msg = {0};
	if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage (&msg);
	}

	pointer_t *pointer_cursor = input_get_pointer(win32_input_pointers[0]);
	pointer_t *pointer_head   = input_get_pointer(win32_input_pointers[1]);

	camera_t    *cam = nullptr;
	transform_t *cam_tr = nullptr;
	render_get_cam(&cam, &cam_tr);

	if (cam != nullptr) {
		pointer_head->available = true;
		pointer_head->ray.pos = cam_tr->_position;
		pointer_head->ray.dir = transform_forward(*cam_tr);

		POINT cursor_pos;
		if (GetCursorPos(&cursor_pos) && ScreenToClient(win32_window, &cursor_pos))
		{
			float x = (((cursor_pos.x / (float)d3d_screen_width ) - 0.5f) * 2.f);
			float y = (((cursor_pos.y / (float)d3d_screen_height) - 0.5f) * 2.f);
			if (x >= -1 && y >= -1 && x <= 1 && y <= 1) {
				pointer_cursor->available = true;
				pointer_cursor->ray.pos = cam_tr->_position;

				// convert screen pos to world ray
				DirectX::XMMATRIX mat;
				camera_viewproj(*cam, *cam_tr, mat);
				DirectX::XMMATRIX inv = DirectX::XMMatrixInverse(nullptr, mat);
				DirectX::XMVECTOR cursor_vec = DirectX::XMVectorSet(x, y, 1.0f, 0.0f);
				cursor_vec = DirectX::XMVector3Transform(cursor_vec, inv);
				DirectX::XMStoreFloat3((DirectX::XMFLOAT3 *) & pointer_cursor->ray.dir, cursor_vec);
			} else {
				pointer_cursor->available = false;
			}
		}
	} else {
		pointer_cursor->available = false;
		pointer_head  ->available = false;
	}
}
void win32_step_end() {
	// Set up where on the render target we want to draw, the view has a 
	D3D11_VIEWPORT viewport = CD3D11_VIEWPORT(0.f, 0.f, d3d_screen_width, d3d_screen_height);
	d3d_context->RSSetViewports(1, &viewport);

	// Wipe our swapchain color and depth target clean, and then set them up for rendering!
	float clear[] = { 0, 0, 0, 1 };
	rendertarget_clear(win32_target, clear);
	rendertarget_set_active(win32_target);

	render_draw();

	win32_swapchain->Present(1, 0);

	render_clear();
}