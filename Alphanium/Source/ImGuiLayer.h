#pragma once
#include <windows.h>
#include <d3d9.h>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <thread>

class ImGuiLayer {
public:
    bool Initialize(IDirect3DDevice9* device);
    bool InitializeStandalone(HWND hwnd);
    bool InitializeStandaloneDevice(HWND hwnd, IDirect3DDevice9* device);
    void Shutdown();
    void RenderForPresent();
    void RenderForEndScene();
    void RenderStandalone();
    bool IsInitialized() const { return contextInitialized_; }

private:
    void StartRenderThread();
    void StopRenderThread();
    void RequestFrame(bool beginScene);
    void RenderInternal(bool beginScene);

    IDirect3DDevice9* device_ = nullptr;
    HWND hwnd_ = nullptr;
    bool contextInitialized_ = false;
    bool backendInitialized_ = false;
    bool threadedRender_ = false;
    std::mutex renderMutex_;
    std::mutex frameMutex_;
    std::condition_variable frameCv_;
    std::thread renderThread_;
    std::atomic<bool> renderThreadRunning_{false};
    bool frameRequested_ = false;
    bool nextBeginScene_ = true;
    std::chrono::steady_clock::time_point lastRenderLog_{};
};

ImGuiLayer& GetImGuiLayer();
