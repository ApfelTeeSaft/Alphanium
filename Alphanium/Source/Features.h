#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <array>
#include <chrono>
#include <deque>
#include "SdkTypes.h"
#include "Strings.h"

struct SelectedActorInfo {
    SDK::AActor* Actor = nullptr;
    SDK::FVector Location{0.0f, 0.0f, 0.0f};
};

class Features {
public:
    void Initialize();
    void Tick();
    void RenderUI();

private:
    void RenderMapsTab();
    void RenderObjectsTab();
    void RenderSpawningTab();
    void RenderCheatsTab();
    void RenderGameplayTab();

    void RefreshActorCache();
    bool ShouldRefreshActors() const;
    void EnqueueCommand(const std::string& command);
    void ProcessPendingCommands();

    void QuickLoadMap(const std::string& mapName);
    void FullLoadMap(const std::string& mapName);
    void UnloadMap();
    void ReloadMap();

    void ApplyMovementCheats();
    void SpawnHusk(bool withAI);
    void SetupGameplay();
    void StartContentScan();
    void ProcessScanResults();
    std::string BuildMapCommand(const std::string& mapName) const;

    std::string selectedMap_;
    std::string mapSearch_;
    std::array<char, 128> mapSearchBuffer_{};
    size_t mapDisplayLimit_ = 50;
    SelectedActorInfo selectedActor_;
    std::vector<std::string> scannedMaps_;
    std::vector<std::string> pendingMaps_;
    std::thread scanThread_;
    std::mutex scanMutex_;
    std::atomic<bool> scanning_{false};

    std::vector<SDK::AActor*> cachedActors_;
    std::chrono::steady_clock::time_point lastActorRefresh_{};
    bool autoRefreshActors_ = true;
    std::string actorSearch_;
    std::array<char, 128> actorSearchBuffer_{};
    size_t actorDisplayLimit_ = 50;

    std::chrono::steady_clock::time_point lastCheatApply_{};
    bool lastFly_ = false;
    bool lastNoclip_ = false;
    bool lastGravity_ = true;
    float lastWalkSpeedMultiplier_ = 1.0f;
    std::deque<std::string> pendingCommands_;
    std::chrono::steady_clock::time_point lastCommandRun_{};

    bool showMenu_ = true;
    bool fly_ = false;
    bool noclip_ = false;
    bool gravity_ = true;
    float walkSpeedMultiplier_ = 1.0f;
    SDK::FVector teleportTarget_{0.0f, 0.0f, 0.0f};
};

Features& GetFeatures();
