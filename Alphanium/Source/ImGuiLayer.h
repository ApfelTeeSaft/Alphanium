#pragma once
#include <windows.h>
#include <d3d9.h>
#include <mutex>

class ImGuiLayer {
public:
    bool Initialize(IDirect3DDevice9* device);
    bool InitializeStandalone(HWND hwnd);
    bool InitializeStandaloneDevice(HWND hwnd, IDirect3DDevice9* device);
    void Shutdown();
    void Render();
    void RenderStandalone();
    bool IsInitialized() const { return contextInitialized_; }

private:
    IDirect3DDevice9* device_ = nullptr;
    HWND hwnd_ = nullptr;
    bool contextInitialized_ = false;
    bool backendInitialized_ = false;
    std::mutex renderMutex_;
};

ImGuiLayer& GetImGuiLayer();
