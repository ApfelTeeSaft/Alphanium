#include "Hooking.h"
#include "MinHook/include/MinHook.h"
#include "ImGuiLayer.h"
#include "SdkTypes.h"
#include "Features.h"
#include "Logger.h"
#include <d3d9.h>
#include <windows.h>

namespace {
using PresentFn = HRESULT(WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using EndSceneFn = HRESULT(WINAPI*)(IDirect3DDevice9*);
PresentFn g_originalPresent = nullptr;
EndSceneFn g_originalEndScene = nullptr;
void* g_presentTarget = nullptr;
void* g_endSceneTarget = nullptr;
using ProcessEventFnT = void(__thiscall*)(SDK::UObject*, SDK::UFunction*, void*);
ProcessEventFnT g_originalProcessEvent = nullptr;

LRESULT CALLBACK DummyWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

HWND CreateDummyWindow() {
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = GetModuleHandleA(nullptr);
    wc.lpszClassName = "AlphaniumDummyWindow";
    RegisterClassExA(&wc);
    return CreateWindowA(wc.lpszClassName, "Alphanium", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);
}

HRESULT WINAPI PresentHook(IDirect3DDevice9* device, const RECT* src, const RECT* dst, HWND hwnd, const RGNDATA* dirty) {
    static bool initialized = false;
    if (!initialized) {
        GetImGuiLayer().Initialize(device);
        GetFeatures().Initialize();
        initialized = true;
    }
    GetFeatures().Tick();
    GetImGuiLayer().RenderForPresent();
    return g_originalPresent(device, src, dst, hwnd, dirty);
}

HRESULT WINAPI EndSceneHook(IDirect3DDevice9* device) {
    static bool initialized = false;
    if (!initialized) {
        GetImGuiLayer().Initialize(device);
        GetFeatures().Initialize();
        initialized = true;
    }
    GetFeatures().Tick();
    GetImGuiLayer().RenderForEndScene();
    return g_originalEndScene(device);
}

void __fastcall ProcessEventHook(SDK::UObject* obj, void*, SDK::UFunction* func, void* params) {
    if (func) {
        std::string name = func->GetFullName();
        if (name.find("SpawnActor") != std::string::npos) {
            LogMessage("ProcessEvent: %s", name.c_str());
        }
    }
    if (g_originalProcessEvent) {
        g_originalProcessEvent(obj, func, params);
    }
}
}

bool InitializeHooks() {
    if (MH_Initialize() != MH_OK) {
        return false;
    }

    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) {
        return false;
    }
    HWND hwnd = CreateDummyWindow();
    D3DPRESENT_PARAMETERS params{};
    params.Windowed = TRUE;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = hwnd;

    IDirect3DDevice9* device = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                  &params, &device);
    if (FAILED(hr)) {
        d3d->Release();
        DestroyWindow(hwnd);
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(device);
    g_presentTarget = vtable[17];
    g_originalPresent = reinterpret_cast<PresentFn>(g_presentTarget);
    g_endSceneTarget = vtable[42];
    g_originalEndScene = reinterpret_cast<EndSceneFn>(g_endSceneTarget);

    if (MH_CreateHook(g_presentTarget, reinterpret_cast<void*>(&PresentHook), reinterpret_cast<void**>(&g_originalPresent)) == MH_OK) {
        MH_EnableHook(g_presentTarget);
        LogMessage("Hooked Present at %p", g_presentTarget);
    } else if (g_endSceneTarget) {
        if (MH_CreateHook(g_endSceneTarget, reinterpret_cast<void*>(&EndSceneHook), reinterpret_cast<void**>(&g_originalEndScene)) == MH_OK) {
            MH_EnableHook(g_endSceneTarget);
            LogMessage("Hooked EndScene at %p", g_endSceneTarget);
        } else {
            LogMessage("Failed to hook Present or EndScene");
        }
    }

    device->Release();
    d3d->Release();
    DestroyWindow(hwnd);

    const auto processEventAddr = SDK::InSDKUtils::GetImageBase() + SDK::Offsets::ProcessEvent;
    if (processEventAddr) {
        g_originalProcessEvent = reinterpret_cast<ProcessEventFnT>(processEventAddr);
        MH_CreateHook(reinterpret_cast<void*>(processEventAddr), reinterpret_cast<void*>(&ProcessEventHook), reinterpret_cast<void**>(&g_originalProcessEvent));
        MH_EnableHook(reinterpret_cast<void*>(processEventAddr));
    }

    return true;
}

void ShutdownHooks() {
    const auto processEventAddr = SDK::InSDKUtils::GetImageBase() + SDK::Offsets::ProcessEvent;
    if (processEventAddr) {
        MH_DisableHook(reinterpret_cast<void*>(processEventAddr));
    }
    if (g_endSceneTarget) {
        MH_DisableHook(g_endSceneTarget);
    }
    if (g_presentTarget) {
        MH_DisableHook(g_presentTarget);
    }
    MH_Uninitialize();
}
