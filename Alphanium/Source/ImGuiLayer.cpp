#include "ImGuiLayer.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx9.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "Features.h"

namespace {
ImGuiLayer g_layer;
}

ImGuiLayer& GetImGuiLayer() {
    return g_layer;
}

bool ImGuiLayer::Initialize(IDirect3DDevice9* device) {
    if (!device) {
        return false;
    }
    device_ = device;
    D3DDEVICE_CREATION_PARAMETERS params{};
    if (FAILED(device_->GetCreationParameters(&params))) {
        return false;
    }
    hwnd_ = params.hFocusWindow;
    if (!contextInitialized_) {
        ImGui::CreateContext();
        contextInitialized_ = true;
    }
    if (!backendInitialized_) {
        ImGui_ImplWin32_Init(hwnd_);
        ImGui_ImplDX9_Init(device_);
        backendInitialized_ = true;
    }
    return true;
}

bool ImGuiLayer::InitializeStandalone(HWND hwnd) {
    if (!hwnd) {
        return false;
    }
    hwnd_ = hwnd;
    if (!contextInitialized_) {
        ImGui::CreateContext();
        contextInitialized_ = true;
    }
    return true;
}

bool ImGuiLayer::InitializeStandaloneDevice(HWND hwnd, IDirect3DDevice9* device) {
    if (!hwnd || !device) {
        return false;
    }
    hwnd_ = hwnd;
    device_ = device;
    if (!contextInitialized_) {
        ImGui::CreateContext();
        contextInitialized_ = true;
    }
    if (!backendInitialized_) {
        ImGui_ImplWin32_Init(hwnd_);
        ImGui_ImplDX9_Init(device_);
        backendInitialized_ = true;
    }
    return true;
}

void ImGuiLayer::Shutdown() {
    if (backendInitialized_) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        backendInitialized_ = false;
    }
    if (contextInitialized_) {
        ImGui::DestroyContext();
        contextInitialized_ = false;
    }
    device_ = nullptr;
    hwnd_ = nullptr;
}

void ImGuiLayer::Render() {
    if (!device_ || !hwnd_) {
        return;
    }
    std::lock_guard<std::mutex> lock(renderMutex_);
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    GetFeatures().RenderUI();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::RenderStandalone() {
    if (!device_ || !hwnd_) {
        return;
    }
    std::lock_guard<std::mutex> lock(renderMutex_);
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    GetFeatures().RenderUI();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}
