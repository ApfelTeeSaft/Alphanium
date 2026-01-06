#pragma once
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include "UE4Types.h"
#include "Strings.h"

struct SelectedActorInfo {
    AActor* Actor = nullptr;
    FVector Location{0.0f, 0.0f, 0.0f};
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

    StringDump stringDump_;
    std::string selectedMap_;
    SelectedActorInfo selectedActor_;
    std::vector<std::string> scannedMaps_;
    std::vector<std::string> pendingMaps_;
    std::thread scanThread_;
    std::mutex scanMutex_;
    std::atomic<bool> scanning_{false};

    bool showMenu_ = true;
    bool fly_ = false;
    bool noclip_ = false;
    bool gravity_ = true;
    float walkSpeedMultiplier_ = 1.0f;
    FVector teleportTarget_{0.0f, 0.0f, 0.0f};
};

Features& GetFeatures();
