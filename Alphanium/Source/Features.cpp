#include "Features.h"
#include "UE4Globals.h"
#include "UE4Helpers.h"
#include "Strings.h"
#include "imgui/imgui.h"
#include "Logger.h"
#include <windows.h>
#include <algorithm>
#include <unordered_set>
#include <filesystem>
#include <thread>
#include <cstring>

namespace {
Features g_features;

int32_t FindNameIndexByString(const std::string& name) {
    auto* names = reinterpret_cast<FNameEntryArray*>(g_ue4.GNames);
    if (!names || !names->Entries) {
        return -1;
    }
    for (int32_t i = 0; i < 1024 * 1024; ++i) {
        auto* entry = names->Entries[i];
        if (!entry) {
            continue;
        }
        if (name == entry->AnsiName) {
            return i;
        }
    }
    return -1;
}

void ConsoleCommand(APlayerController* controller, const std::string& command) {
    if (!controller) {
        return;
    }
    auto func = FindFunction(L"Function Engine.PlayerController.ConsoleCommand");
    if (!func) {
        return;
    }
    struct FString {
        wchar_t* Data;
        int32_t Count;
        int32_t Max;
    } cmd{};
    std::wstring wide(command.begin(), command.end());
    cmd.Data = const_cast<wchar_t*>(wide.c_str());
    cmd.Count = static_cast<int32_t>(wide.size() + 1);
    cmd.Max = cmd.Count;
    struct Params {
        FString Command;
        bool bWriteToLog;
        FString ReturnValue;
    } params{};
    params.Command = cmd;
    params.bWriteToLog = false;

    if (auto process = GetProcessEvent()) {
        process(reinterpret_cast<UObject*>(controller), func, &params);
    }
}
}

Features& GetFeatures() {
    return g_features;
}

void Features::Initialize() {
    StartContentScan();
}

void Features::Tick() {
    ApplyMovementCheats();
    ProcessScanResults();
}

void Features::RenderUI() {
    if (!showMenu_) {
        return;
    }

    ImGui::SetNextWindowSize({700.0f, 500.0f});
    if (!ImGui::Begin("Alphanium")) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("Maps")) {
            RenderMapsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("GameObjects")) {
            RenderObjectsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Spawning")) {
            RenderSpawningTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Cheats / Utilities")) {
            RenderCheatsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Gameplay Reconstruction")) {
            RenderGameplayTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void Features::RenderMapsTab() {
    ImGui::Text("Discovered Maps:");
    ImGui::BeginChild("MapList", ImVec2(0.0f, 220.0f), true);
    for (const auto& map : stringDump_.maps) {
        bool selected = (selectedMap_ == map);
        if (ImGui::Selectable(map.c_str(), selected)) {
            selectedMap_ = map;
        }
    }
    for (const auto& map : scannedMaps_) {
        bool selected = (selectedMap_ == map);
        if (ImGui::Selectable(map.c_str(), selected)) {
            selectedMap_ = map;
        }
    }
    ImGui::EndChild();
    if (!selectedMap_.empty()) {
        ImGui::Text("Selected: %s", selectedMap_.c_str());
    }
    if (ImGui::Button("Quick Load (Sandbox)")) {
        QuickLoadMap(selectedMap_);
    }
    if (ImGui::Button("Full Load (Gameplay)")) {
        FullLoadMap(selectedMap_);
    }
    if (ImGui::Button("Unload")) {
        UnloadMap();
    }
    if (ImGui::Button("Reload")) {
        ReloadMap();
    }
}

void Features::RenderObjectsTab() {
    std::vector<AActor*> foundActors;
    std::unordered_set<uintptr_t> seen;

    if (UWorld* world = GetWorld()) {
        if (world->PersistentLevel) {
            auto& actors = world->PersistentLevel->Actors;
            for (int32_t i = 0; i < actors.Count; ++i) {
                AActor* actor = actors[i];
                if (actor && seen.insert(reinterpret_cast<uintptr_t>(actor)).second) {
                    foundActors.push_back(actor);
                }
            }
        }
        for (int32_t l = 0; l < world->Levels.Count; ++l) {
            ULevel* level = world->Levels[l];
            if (!level) {
                continue;
            }
            auto& actors = level->Actors;
            for (int32_t i = 0; i < actors.Count; ++i) {
                AActor* actor = actors[i];
                if (actor && seen.insert(reinterpret_cast<uintptr_t>(actor)).second) {
                    foundActors.push_back(actor);
                }
            }
        }
    }

    UClass* actorClass = reinterpret_cast<UClass*>(FindObjectByName(L"Class Engine.Actor"));
    if (auto* objects = GetGUObjectArray()) {
        for (int32_t i = 0; i < objects->NumElements; ++i) {
            auto& item = objects->Objects[i];
            if (!item.Object) {
                continue;
            }
            UObject* obj = item.Object;
            if (actorClass && !obj->IsA(actorClass)) {
                continue;
            }
            AActor* actor = reinterpret_cast<AActor*>(obj);
            if (seen.insert(reinterpret_cast<uintptr_t>(actor)).second) {
                foundActors.push_back(actor);
            }
        }
    }

    ImGui::Text("Actors: %d", static_cast<int>(foundActors.size()));
    for (auto* actor : foundActors) {
        if (!actor) {
            continue;
        }
        std::string label = actor->GetName();
        if (ImGui::Button(label.c_str())) {
            selectedActor_.Actor = actor;
            selectedActor_.Location = actor->GetActorLocation();
        }
    }

    if (selectedActor_.Actor) {
        ImGui::Text("Selected: %s", selectedActor_.Actor->GetFullName().c_str());
        float loc[3] = {selectedActor_.Location.X, selectedActor_.Location.Y, selectedActor_.Location.Z};
        ImGui::InputFloat3("Location", loc);
        if (ImGui::Button("Apply Transform")) {
            selectedActor_.Location = {loc[0], loc[1], loc[2]};
            SetActorLocation(selectedActor_.Actor, selectedActor_.Location);
        }
        if (ImGui::Button("Destroy")) {
            DestroyActor(selectedActor_.Actor);
            selectedActor_.Actor = nullptr;
        }
    }
}

void Features::RenderSpawningTab() {
    ImGui::Text("Husk Spawning:");
    if (ImGui::Button("Spawn Husk (No AI / T-Pose)")) {
        SpawnHusk(false);
    }
    if (ImGui::Button("Spawn Husk (With AI)")) {
        SpawnHusk(true);
    }
}

void Features::RenderCheatsTab() {
    ImGui::Checkbox("Fly", &fly_);
    ImGui::Checkbox("Noclip", &noclip_);
    ImGui::Checkbox("Gravity", &gravity_);
    ImGui::SliderFloat("Walk Speed", &walkSpeedMultiplier_, 0.1f, 5.0f);
    float tp[3] = {teleportTarget_.X, teleportTarget_.Y, teleportTarget_.Z};
    ImGui::InputFloat3("Teleport Target", tp);
    teleportTarget_ = {tp[0], tp[1], tp[2]};
    if (ImGui::Button("Teleport")) {
        if (auto controller = GetLocalPlayerController()) {
            auto pawn = controller->AcknowledgedPawn;
            if (pawn) {
                SetActorLocation(reinterpret_cast<AActor*>(pawn), teleportTarget_);
            }
        }
    }
}

void Features::RenderGameplayTab() {
    ImGui::Text("Reconstruct offline gameplay systems.");
    if (ImGui::Button("Load / Setup Gameplay")) {
        SetupGameplay();
    }
}

void Features::QuickLoadMap(const std::string& mapName) {
    if (mapName.empty()) {
        return;
    }
    UObject* world = reinterpret_cast<UObject*>(GetWorld());
    if (!world) {
        LogMessage("QuickLoadMap: world is null");
        return;
    }
    std::string command = BuildMapCommand(mapName);
    LogMessage("QuickLoadMap: executing '%s'", command.c_str());
    ExecuteConsoleCommand(world, command);

    auto controller = GetLocalPlayerController();
    if (!controller) {
        LogMessage("QuickLoadMap: PlayerController not found");
        return;
    }
    FVector spawnLoc{0.0f, 0.0f, 300.0f};
    auto character = SpawnDefaultCharacter(spawnLoc);
    if (character) {
        PossessPawn(controller, reinterpret_cast<APawn*>(character));
    } else {
        LogMessage("QuickLoadMap: failed to spawn default character");
    }
}

void Features::FullLoadMap(const std::string& mapName) {
    QuickLoadMap(mapName);
    UWorld* world = GetWorld();
    if (!world) {
        return;
    }
    auto gameModeClass = FindObjectByName(L"Class Engine.GameMode");
    auto gameStateClass = FindObjectByName(L"Class Engine.GameState");
    if (gameModeClass) {
        FVector loc{0.0f, 0.0f, 0.0f};
        auto gm = SpawnDefaultCharacter(loc);
        world->AuthorityGameMode = reinterpret_cast<AGameModeBase*>(gm);
    }
    if (gameStateClass) {
        FVector loc{0.0f, 0.0f, 0.0f};
        auto gs = SpawnDefaultCharacter(loc);
        world->GameState = reinterpret_cast<AGameStateBase*>(gs);
    }
}

void Features::UnloadMap() {
    UObject* world = reinterpret_cast<UObject*>(GetWorld());
    if (world) {
        LogMessage("UnloadMap: executing open /Engine/Maps/Entry");
        ExecuteConsoleCommand(world, "open /Engine/Maps/Entry");
    } else {
        LogMessage("UnloadMap: world is null");
    }
}

void Features::ReloadMap() {
    if (!selectedMap_.empty()) {
        QuickLoadMap(selectedMap_);
    }
}

void Features::ApplyMovementCheats() {
    auto controller = GetLocalPlayerController();
    if (!controller) {
        return;
    }
    UObject* world = reinterpret_cast<UObject*>(GetWorld());
    if (!world) {
        LogMessage("ApplyMovementCheats: world is null");
        return;
    }
    if (fly_) {
        ExecuteConsoleCommand(world, "fly");
    }
    if (noclip_) {
        ExecuteConsoleCommand(world, "ghost");
    }
    if (!gravity_) {
        ExecuteConsoleCommand(world, "setgravity 0");
    } else {
        ExecuteConsoleCommand(world, "setgravity 1");
    }
    if (walkSpeedMultiplier_ != 1.0f) {
        std::string cmd = "slomo " + std::to_string(walkSpeedMultiplier_);
        ExecuteConsoleCommand(world, cmd);
    }
}

void Features::SpawnHusk(bool withAI) {
    auto controller = GetLocalPlayerController();
    if (!controller) {
        return;
    }
    UObject* huskClass = FindObjectByName(L"Class FortniteGame.FortAIPawn_Husk");
    if (!huskClass) {
        huskClass = FindObjectByName(L"Husk");
    }
    if (!huskClass) {
        return;
    }
    UWorld* world = GetWorld();
    if (!world) {
        return;
    }
    auto spawnFunc = FindFunction(L"Function Engine.World.SpawnActor");
    if (!spawnFunc) {
        return;
    }
    FVector spawnLoc{0.0f, 0.0f, 300.0f};
    if (controller->AcknowledgedPawn) {
        spawnLoc = controller->AcknowledgedPawn->GetActorLocation();
        spawnLoc.X += 200.0f;
    }
    struct SpawnParams {
        UClass* Class;
        FVector Location;
        FRotator Rotation;
        AActor* Owner;
        APawn* Instigator;
        bool bNoCollisionFail;
        bool bRemoteOwned;
        AActor* ReturnValue;
    } params{};
    params.Class = reinterpret_cast<UClass*>(huskClass);
    params.Location = spawnLoc;
    params.Rotation = {0.0f, 0.0f, 0.0f};
    params.bNoCollisionFail = true;

    if (auto process = GetProcessEvent()) {
        process(reinterpret_cast<UObject*>(world), spawnFunc, &params);
    }
    AActor* husk = params.ReturnValue;
    if (!husk || !withAI) {
        return;
    }

    UObject* aiClass = FindObjectByName(L"Class Engine.AIController");
    if (!aiClass) {
        return;
    }
    SpawnParams aiParams{};
    aiParams.Class = reinterpret_cast<UClass*>(aiClass);
    aiParams.Location = spawnLoc;
    aiParams.Rotation = {0.0f, 0.0f, 0.0f};
    aiParams.bNoCollisionFail = true;
    if (auto process = GetProcessEvent()) {
        process(reinterpret_cast<UObject*>(world), spawnFunc, &aiParams);
    }
    AActor* ai = aiParams.ReturnValue;
    if (!ai) {
        return;
    }
    auto possessFunc = FindFunction(L"Function Engine.Controller.Possess");
    if (possessFunc) {
        struct PossessParams { APawn* Pawn; } possess{reinterpret_cast<APawn*>(husk)};
        if (auto process = GetProcessEvent()) {
            process(reinterpret_cast<UObject*>(ai), possessFunc, &possess);
        }
    }
}

void Features::SetupGameplay() {
    auto controller = GetLocalPlayerController();
    if (!controller) {
        return;
    }
    auto giveItemFunc = FindFunction(L"Function FortniteGame.FortPlayerController.GiveItem");
    for (const auto& weapon : stringDump_.weapon_defs) {
        if (!giveItemFunc) {
            break;
        }
        UObject* itemDef = FindObjectByName(std::wstring(weapon.begin(), weapon.end()));
        if (!itemDef) {
            continue;
        }
        struct Params {
            UObject* ItemDef;
            int32_t Count;
            int32_t Level;
            bool bSilent;
        } params{};
        params.ItemDef = itemDef;
        params.Count = 1;
        params.Level = 1;
        params.bSilent = true;
        if (auto process = GetProcessEvent()) {
            process(reinterpret_cast<UObject*>(controller), giveItemFunc, &params);
        }
    }

    auto buildFunc = FindFunction(L"Function FortniteGame.FortPlayerController.ToggleBuildMode");
    if (buildFunc) {
        if (auto process = GetProcessEvent()) {
            process(reinterpret_cast<UObject*>(controller), buildFunc, nullptr);
        }
    }
}

std::string Features::BuildMapCommand(const std::string& mapName) const {
    if (mapName.find("/Game/") == 0 || mapName.find("/Engine/") == 0) {
        return "open " + mapName;
    }
    std::string name = mapName;
    const std::string ext = ".umap";
    if (name.size() > ext.size() && name.substr(name.size() - ext.size()) == ext) {
        name = name.substr(0, name.size() - ext.size());
    }
    if (name.find("Content/") == 0) {
        name = name.substr(strlen("Content/"));
    }
    return "open /Game/" + name;
}

void Features::StartContentScan() {
    if (scanning_) {
        return;
    }
    scanning_ = true;
    scanThread_ = std::thread([this]() {
        char modulePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
        std::filesystem::path exePath(modulePath);
        auto root = exePath.parent_path().parent_path().parent_path();
        auto content = root / "Content";
        LogMessage("Alphanium: scanning content at %s", content.string().c_str());
        if (!std::filesystem::exists(content)) {
            LogMessage("Alphanium: Content folder not found");
            scanning_ = false;
            return;
        }
        std::vector<std::string> found;
        for (auto& entry : std::filesystem::recursive_directory_iterator(content)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            auto path = entry.path();
            if (path.extension() == ".umap") {
                auto rel = std::filesystem::relative(path, content);
                std::string name = rel.string();
                std::replace(name.begin(), name.end(), '\\', '/');
                found.push_back(name);
            }
        }
        {
            std::lock_guard<std::mutex> lock(scanMutex_);
            pendingMaps_.swap(found);
        }
        scanning_ = false;
    });
    scanThread_.detach();
}

void Features::ProcessScanResults() {
    std::vector<std::string> pending;
    {
        std::lock_guard<std::mutex> lock(scanMutex_);
        pending.swap(pendingMaps_);
    }
    if (pending.empty()) {
        return;
    }
    LogMessage("Alphanium: content scan found %zu maps", pending.size());
    scannedMaps_ = std::move(pending);
    if (selectedMap_.empty() && !scannedMaps_.empty()) {
        selectedMap_ = scannedMaps_.front();
    }
}
