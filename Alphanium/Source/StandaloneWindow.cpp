#include "StandaloneWindow.h"
#include "ImGuiLayer.h"
#include "Features.h"
#include "Logger.h"
#include <atomic>
#include <d3d9.h>
#include "imgui/imgui.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
);
#include "imgui/backends/imgui_impl_win32.h"
#include "imgui/backends/imgui_impl_dx9.h"

namespace {
std::atomic<bool> g_running{false};
HWND g_hwnd = nullptr;
HANDLE g_thread = nullptr;
IDirect3D9* g_d3d = nullptr;
IDirect3DDevice9* g_device = nullptr;
D3DPRESENT_PARAMETERS g_d3dpp = {};

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }
    switch (msg) {
        case WM_SIZE:
            if (g_device && wparam != SIZE_MINIMIZED) {
                g_d3dpp.BackBufferWidth = LOWORD(lparam);
                g_d3dpp.BackBufferHeight = HIWORD(lparam);
                ImGui_ImplDX9_InvalidateDeviceObjects();
                if (SUCCEEDED(g_device->Reset(&g_d3dpp))) {
                    ImGui_ImplDX9_CreateDeviceObjects();
                }
            }
            return 0;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, msg, wparam, lparam);
    }
}

bool CreateDevice(HWND hwnd) {
    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_d3d) {
        return false;
    }
    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.hDeviceWindow = hwnd;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    RECT rect{};
    GetClientRect(hwnd, &rect);
    g_d3dpp.BackBufferWidth = rect.right - rect.left;
    g_d3dpp.BackBufferHeight = rect.bottom - rect.top;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    return SUCCEEDED(g_d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                         D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                         &g_d3dpp, &g_device));
}

void CleanupDevice() {
    if (g_device) {
        g_device->Release();
        g_device = nullptr;
    }
    if (g_d3d) {
        g_d3d->Release();
        g_d3d = nullptr;
    }
}

DWORD WINAPI OverlayThread(LPVOID) {
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AlphaniumOverlayWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExA(&wc);

    g_hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        "Alphanium",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        720,
        540,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);

    if (!g_hwnd) {
        g_running = false;
        return 0;
    }

    ShowWindow(g_hwnd, SW_SHOW);

    GetFeatures().Initialize();
    if (!CreateDevice(g_hwnd)) {
        LogMessage("Alphanium: failed to create D3D9 device for standalone window");
        g_running = false;
        CleanupDevice();
        return 0;
    }
    GetImGuiLayer().InitializeStandaloneDevice(g_hwnd, g_device);

    MSG msg{};
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        GetFeatures().Tick();
        if (g_device) {
            g_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(20, 20, 20), 1.0f, 0);
            if (SUCCEEDED(g_device->BeginScene())) {
                GetImGuiLayer().RenderStandalone();
                g_device->EndScene();
            }
            HRESULT result = g_device->Present(nullptr, nullptr, nullptr, nullptr);
            if (result == D3DERR_DEVICELOST && g_device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET) {
                ImGui_ImplDX9_InvalidateDeviceObjects();
                if (SUCCEEDED(g_device->Reset(&g_d3dpp))) {
                    ImGui_ImplDX9_CreateDeviceObjects();
                }
            }
        }
        Sleep(16);
    }

    GetImGuiLayer().Shutdown();
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = nullptr;
    }
    CleanupDevice();
    return 0;
}
}

bool StartStandaloneOverlay() {
    if (g_running) {
        return true;
    }
    g_running = true;
    g_thread = CreateThread(nullptr, 0, OverlayThread, nullptr, 0, nullptr);
    return g_thread != nullptr;
}

void StopStandaloneOverlay() {
    g_running = false;
    if (g_thread) {
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
}
