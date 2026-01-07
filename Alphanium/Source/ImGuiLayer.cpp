#include "ImGuiLayer.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx9.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "Features.h"
#include "Logger.h"
#include <chrono>

namespace {
ImGuiLayer g_layer;
}

ImGuiLayer& GetImGuiLayer() {
    return g_layer;
}

bool ImGuiLayer::Initialize(IDirect3DDevice9* device) {
    if (!device) {
        LogMessage("ImGuiLayer::Initialize: device null");
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
        LogMessage("ImGuiLayer::Initialize: backend initialized hwnd=%p device=%p", hwnd_, device_);
    }
    threadedRender_ = false;
    return true;
}

bool ImGuiLayer::InitializeStandalone(HWND hwnd) {
    if (!hwnd) {
        LogMessage("ImGuiLayer::InitializeStandalone: hwnd null");
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
        LogMessage("ImGuiLayer::InitializeStandaloneDevice: invalid hwnd/device hwnd=%p device=%p", hwnd, device);
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
        LogMessage("ImGuiLayer::InitializeStandaloneDevice: backend initialized hwnd=%p device=%p", hwnd_, device_);
    }
    threadedRender_ = false;
    return true;
}

void ImGuiLayer::Shutdown() {
    LogMessage("ImGuiLayer::Shutdown: start");
    StopRenderThread();
    if (backendInitialized_) {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        backendInitialized_ = false;
        LogMessage("ImGuiLayer::Shutdown: backend shutdown");
    }
    if (contextInitialized_) {
        ImGui::DestroyContext();
        contextInitialized_ = false;
        LogMessage("ImGuiLayer::Shutdown: context destroyed");
    }
    device_ = nullptr;
    hwnd_ = nullptr;
}

void ImGuiLayer::RenderForPresent() {
    RequestFrame(true);
}

void ImGuiLayer::RenderForEndScene() {
    RequestFrame(false);
}

void ImGuiLayer::RenderStandalone() {
    RenderInternal(false);
}

void ImGuiLayer::StartRenderThread() {
    if (renderThreadRunning_.load()) {
        LogMessage("ImGuiLayer::StartRenderThread: already running");
        return;
    }
    renderThreadRunning_.store(true);
    LogMessage("ImGuiLayer::StartRenderThread: starting");
    renderThread_ = std::thread([this]() {
        LogMessage("ImGuiLayer::RenderThread: started");
        while (renderThreadRunning_.load()) {
            bool beginScene = true;
            {
                std::unique_lock<std::mutex> lock(frameMutex_);
                frameCv_.wait(lock, [this]() { return frameRequested_ || !renderThreadRunning_.load(); });
                if (!renderThreadRunning_.load()) {
                    break;
                }
                frameRequested_ = false;
                beginScene = nextBeginScene_;
            }
            RenderInternal(beginScene);
        }
        LogMessage("ImGuiLayer::RenderThread: exiting");
    });
}

void ImGuiLayer::StopRenderThread() {
    if (!renderThreadRunning_.load()) {
        LogMessage("ImGuiLayer::StopRenderThread: not running");
        return;
    }
    renderThreadRunning_.store(false);
    frameCv_.notify_all();
    if (renderThread_.joinable()) {
        renderThread_.join();
    }
    LogMessage("ImGuiLayer::StopRenderThread: stopped");
}

void ImGuiLayer::RequestFrame(bool beginScene) {
    if (!threadedRender_) {
        RenderInternal(beginScene);
        return;
    }
    if (!renderThreadRunning_.load()) {
        StartRenderThread();
    }
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        frameRequested_ = true;
        nextBeginScene_ = beginScene;
    }
    frameCv_.notify_one();
}

void ImGuiLayer::RenderInternal(bool beginScene) {
    if (!device_ || !hwnd_) {
        LogMessage("ImGuiLayer::RenderInternal: device/hwnd missing device=%p hwnd=%p", device_, hwnd_);
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(renderMutex_);
    if (beginScene && FAILED(device_->BeginScene())) {
        LogMessage("ImGuiLayer::RenderInternal: BeginScene failed");
        return;
    }
    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    GetFeatures().RenderUI();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    if (beginScene) {
        device_->EndScene();
    }
    const auto end = std::chrono::steady_clock::now();
    if (lastRenderLog_ == std::chrono::steady_clock::time_point{}
        || (end - lastRenderLog_) > std::chrono::seconds(1)) {
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        LogMessage("ImGuiLayer::RenderInternal: frame done beginScene=%d duration=%lldms",
                   beginScene ? 1 : 0,
                   static_cast<long long>(elapsedMs));
        lastRenderLog_ = end;
    }
}
