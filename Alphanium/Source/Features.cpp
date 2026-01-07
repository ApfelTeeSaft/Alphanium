#include "Features.h"
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
#include <cctype>

namespace {
Features g_features;

void ConsoleCommand(SDK::APlayerController* controller, const std::string& command) {
    if (!controller) {
        return;
    }
    auto func = FindFunction(L"Function Engine.PlayerController.ConsoleCommand");
    if (!func) {
        return;
    }
    std::wstring wide(command.begin(), command.end());
    SDK::FString cmd(wide.c_str());
    struct Params {
        SDK::FString Command;
        bool bWriteToLog;
        SDK::FString ReturnValue;
    } params{};
    params.Command = cmd;
    params.bWriteToLog = false;

    controller->ProcessEvent(func, &params);
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
    ProcessPendingCommands();
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
    std::unordered_set<std::string> seen;
    std::vector<const std::string*> allMaps;
    allMaps.reserve(kMapNames.size() + scannedMaps_.size());
    for (const auto& map : kMapNames) {
        if (seen.insert(map).second) {
            allMaps.push_back(&map);
        }
    }
    for (const auto& map : scannedMaps_) {
        if (seen.insert(map).second) {
            allMaps.push_back(&map);
        }
    }

    ImGui::InputText("Search", mapSearchBuffer_.data(), mapSearchBuffer_.size());
    mapSearch_ = mapSearchBuffer_.data();

    std::vector<const std::string*> filteredMaps;
    const bool hasSearch = !mapSearch_.empty();
    if (hasSearch) {
        filteredMaps.reserve(allMaps.size());
        std::string query = mapSearch_;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);
        for (const auto* map : allMaps) {
            std::string name = *map;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name.find(query) != std::string::npos) {
                filteredMaps.push_back(map);
            }
        }
    }
    std::vector<const std::string*> displayMaps = hasSearch ? filteredMaps : allMaps;
    if (!hasSearch && mapDisplayLimit_ < displayMaps.size()) {
        displayMaps.resize(mapDisplayLimit_);
    }

    ImGui::Text("Discovered Maps:");
    ImGui::BeginChild("MapList", ImVec2(0.0f, 220.0f), true);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(displayMaps.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const std::string& map = *displayMaps[i];
            bool selected = (selectedMap_ == map);
            if (ImGui::Selectable(map.c_str(), selected)) {
                selectedMap_ = map;
            }
        }
    }
    ImGui::EndChild();
    if (!hasSearch && mapDisplayLimit_ < allMaps.size()) {
        if (ImGui::Button("Load More")) {
            mapDisplayLimit_ += 50;
        }
    }
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
    if (ImGui::Button("Refresh Actors") || (autoRefreshActors_ && ShouldRefreshActors())) {
        RefreshActorCache();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto Refresh", &autoRefreshActors_);

    ImGui::InputText("Search Actors", actorSearchBuffer_.data(), actorSearchBuffer_.size());
    actorSearch_ = actorSearchBuffer_.data();

    std::vector<SDK::AActor*> displayActors;
    const bool hasSearch = !actorSearch_.empty();
    if (hasSearch) {
        displayActors.reserve(cachedActors_.size());
        std::string query = actorSearch_;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);
        for (auto* actor : cachedActors_) {
            if (!actor) {
                continue;
            }
            std::string name = actor->GetName();
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            if (name.find(query) != std::string::npos) {
                displayActors.push_back(actor);
            }
        }
    } else {
        displayActors = cachedActors_;
        if (actorDisplayLimit_ < displayActors.size()) {
            displayActors.resize(actorDisplayLimit_);
        }
    }

    ImGui::Text("Actors: %d", static_cast<int>(displayActors.size()));
    ImGui::BeginChild("ActorList", ImVec2(0.0f, 260.0f), true);
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(displayActors.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            SDK::AActor* actor = displayActors[i];
            if (!actor) {
                continue;
            }
            std::string label = actor->GetName();
            ImGui::PushID(reinterpret_cast<void*>(actor));
            if (ImGui::Selectable(label.c_str(), selectedActor_.Actor == actor)) {
                selectedActor_.Actor = actor;
                selectedActor_.Location = actor->K2_GetActorLocation();
            }
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (!hasSearch && actorDisplayLimit_ < cachedActors_.size()) {
        if (ImGui::Button("Load More Actors")) {
            actorDisplayLimit_ += 50;
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

bool Features::ShouldRefreshActors() const {
    if (lastActorRefresh_ == std::chrono::steady_clock::time_point{}) {
        return true;
    }
    return (std::chrono::steady_clock::now() - lastActorRefresh_) > std::chrono::seconds(1);
}

void Features::RefreshActorCache() {
    std::vector<SDK::AActor*> foundActors;
    std::unordered_set<uintptr_t> seen;

    if (SDK::UWorld* world = GetWorld()) {
        if (world->PersistentLevel) {
            auto& actors = world->PersistentLevel->Actors;
            for (int32_t i = 0; i < actors.Num(); ++i) {
                SDK::AActor* actor = actors[i];
                if (actor && seen.insert(reinterpret_cast<uintptr_t>(actor)).second) {
                    foundActors.push_back(actor);
                }
            }
        }
        for (int32_t l = 0; l < world->Levels.Num(); ++l) {
            SDK::ULevel* level = world->Levels[l];
            if (!level) {
                continue;
            }
            auto& actors = level->Actors;
            for (int32_t i = 0; i < actors.Num(); ++i) {
                SDK::AActor* actor = actors[i];
                if (actor && seen.insert(reinterpret_cast<uintptr_t>(actor)).second) {
                    foundActors.push_back(actor);
                }
            }
        }
    }

    SDK::UClass* actorClass = reinterpret_cast<SDK::UClass*>(FindObjectByName(L"Class Engine.Actor"));
    auto* objects = SDK::UObject::GObjects.GetTypedPtr();
    if (objects) {
        for (int32_t i = 0; i < objects->Num(); ++i) {
            SDK::UObject* obj = objects->GetByIndex(i);
            if (!obj) {
                continue;
            }
            if (actorClass && !obj->IsA(actorClass)) {
                continue;
            }
            SDK::AActor* actor = reinterpret_cast<SDK::AActor*>(obj);
            if (seen.insert(reinterpret_cast<uintptr_t>(actor)).second) {
                foundActors.push_back(actor);
            }
        }
    }

    cachedActors_ = std::move(foundActors);
    lastActorRefresh_ = std::chrono::steady_clock::now();
}

void Features::RenderSpawningTab() {
    ImGui::Text("Husk Spawning:");
    static const char* kHuskOptions[] = {
        "WerewolfPawn_C",
        "FlingerPawn_C",
        "TakerPawn_C",
        "TrollPawn_C",
        "BlasterPawn_C",
        "HuskPawn_Beehive_C",
        "HuskPawn_Bombshell_Poison_C",
        "HuskPawn_Bombshell_C",
        "HuskPawn_Dwarf_Fire_C",
        "HuskPawn_Dwarf_Ice_C",
        "HuskPawn_Dwarf_Lightning_C",
        "HuskPawn_Dwarf_C",
        "HuskPawn_Fire_C",
        "HuskPawn_Husky_Fire_C",
        "HuskPawn_Husky_Ice_C",
        "HuskPawn_Husky_Lightning_C",
        "HuskPawn_Husky_C",
        "HuskPawn_Ice_C",
        "HuskPawn_Lightning_C",
        "HuskPawn_Pitcher_C",
        "HuskPawn_Sploder_C",
        "HuskPawn_Trashman_C",
        "HuskPawn_C",
        "SmasherPawn_Boss_C",
        "SmasherPawn_Fire_C",
        "SmasherPawn_Ice_C",
        "SmasherPawn_Lightning_C",
        "SmasherPawn_C",
        "SmasherPawn_BossRampage_C",
    };
    static int selectedHusk = 0;
    ImGui::Combo("Husk Type", &selectedHusk, kHuskOptions, static_cast<int>(std::size(kHuskOptions)));
    static bool withAI = true;
    ImGui::Checkbox("With AI", &withAI);
    if (ImGui::Button("Spawn Husk")) {
        SpawnHusk(kHuskOptions[selectedHusk], withAI);
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
                SetActorLocation(reinterpret_cast<SDK::AActor*>(pawn), teleportTarget_);
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
    SDK::UObject* world = reinterpret_cast<SDK::UObject*>(GetWorld());
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
    SDK::FVector spawnLoc{0.0f, 0.0f, 300.0f};
    auto character = SpawnDefaultCharacter(spawnLoc);
    if (character) {
        PossessPawn(controller, reinterpret_cast<SDK::APawn*>(character));
    } else {
        LogMessage("QuickLoadMap: failed to spawn default character");
    }
}

void Features::FullLoadMap(const std::string& mapName) {
    QuickLoadMap(mapName);
    SDK::UWorld* world = GetWorld();
    if (!world) {
        return;
    }
    auto gameModeClass = FindObjectByName(L"Class Engine.GameMode");
    auto gameStateClass = FindObjectByName(L"Class Engine.GameState");
    if (gameModeClass) {
        SDK::FVector loc{0.0f, 0.0f, 0.0f};
        auto gm = SpawnDefaultCharacter(loc);
        world->AuthorityGameMode = reinterpret_cast<SDK::AGameMode*>(gm);
    }
    if (gameStateClass) {
        SDK::FVector loc{0.0f, 0.0f, 0.0f};
        auto gs = SpawnDefaultCharacter(loc);
        world->GameState = reinterpret_cast<SDK::AGameState*>(gs);
    }
}

void Features::UnloadMap() {
    SDK::UObject* world = reinterpret_cast<SDK::UObject*>(GetWorld());
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
    SDK::UObject* world = reinterpret_cast<SDK::UObject*>(GetWorld());
    if (!world) {
        LogMessage("ApplyMovementCheats: world is null");
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    const bool settingsChanged = fly_ != lastFly_
        || noclip_ != lastNoclip_
        || gravity_ != lastGravity_
        || walkSpeedMultiplier_ != lastWalkSpeedMultiplier_;

    if (!settingsChanged && lastCheatApply_ != std::chrono::steady_clock::time_point{}
        && (now - lastCheatApply_) < std::chrono::milliseconds(500)) {
        return;
    }

    if (fly_) {
        EnqueueCommand("fly");
    }
    if (noclip_) {
        EnqueueCommand("ghost");
    }
    if (!gravity_) {
        EnqueueCommand("setgravity 0");
    } else {
        EnqueueCommand("setgravity 1");
    }
    if (walkSpeedMultiplier_ != 1.0f) {
        EnqueueCommand("slomo " + std::to_string(walkSpeedMultiplier_));
    }
    lastCheatApply_ = now;
    lastFly_ = fly_;
    lastNoclip_ = noclip_;
    lastGravity_ = gravity_;
    lastWalkSpeedMultiplier_ = walkSpeedMultiplier_;
}

void Features::EnqueueCommand(const std::string& command) {
    if (command.empty()) {
        return;
    }
    if (!pendingCommands_.empty() && pendingCommands_.back() == command) {
        return;
    }
    pendingCommands_.push_back(command);
}

void Features::ProcessPendingCommands() {
    if (pendingCommands_.empty()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (lastCommandRun_ != std::chrono::steady_clock::time_point{}
        && (now - lastCommandRun_) < std::chrono::milliseconds(200)) {
        return;
    }
    SDK::UObject* world = reinterpret_cast<SDK::UObject*>(GetWorld());
    if (!world) {
        return;
    }
    const std::string command = std::move(pendingCommands_.front());
    pendingCommands_.pop_front();
    ExecuteConsoleCommand(world, command);
    lastCommandRun_ = now;
}

void Features::SpawnHusk(const std::string& huskClassName, bool withAI) {
    auto controller = GetLocalPlayerController();
    if (!controller) {
        return;
    }
    if (huskClassName.empty()) {
        return;
    }
    std::wstring className(huskClassName.begin(), huskClassName.end());
    std::wstring fullClassName = L"Class FortniteGame." + className;
    SDK::UObject* huskClass = FindObjectByName(fullClassName);
    if (!huskClass) {
        huskClass = FindObjectByName(className);
    }
    if (!huskClass) {
        return;
    }
    SDK::UWorld* world = GetWorld();
    if (!world) {
        return;
    }
    auto spawnFunc = FindFunction(L"Function Engine.World.SpawnActor");
    if (!spawnFunc) {
        return;
    }
    SDK::FVector spawnLoc{0.0f, 0.0f, 300.0f};
    if (controller->AcknowledgedPawn) {
        auto pawn = reinterpret_cast<SDK::AActor*>(controller->AcknowledgedPawn);
        spawnLoc = pawn->K2_GetActorLocation();
        SDK::FVector forward = pawn->GetActorForwardVector();
        spawnLoc.X += forward.X * 10.0f;
        spawnLoc.Y += forward.Y * 10.0f;
        spawnLoc.Z += forward.Z * 10.0f;
    }
    struct SpawnParams {
        SDK::UClass* Class;
        SDK::FVector Location;
        SDK::FRotator Rotation;
        SDK::AActor* Owner;
        SDK::APawn* Instigator;
        bool bNoCollisionFail;
        bool bRemoteOwned;
        SDK::AActor* ReturnValue;
    } params{};
    params.Class = reinterpret_cast<SDK::UClass*>(huskClass);
    params.Location = spawnLoc;
    params.Rotation = {0.0f, 0.0f, 0.0f};
    params.bNoCollisionFail = true;

    world->ProcessEvent(spawnFunc, &params);
    SDK::AActor* husk = params.ReturnValue;
    if (!husk || !withAI) {
        return;
    }

    SDK::UObject* aiClass = FindObjectByName(L"Class Engine.AIController");
    if (!aiClass) {
        return;
    }
    SpawnParams aiParams{};
    aiParams.Class = reinterpret_cast<SDK::UClass*>(aiClass);
    aiParams.Location = spawnLoc;
    aiParams.Rotation = {0.0f, 0.0f, 0.0f};
    aiParams.bNoCollisionFail = true;
    world->ProcessEvent(spawnFunc, &aiParams);
    SDK::AActor* ai = aiParams.ReturnValue;
    if (!ai) {
        return;
    }
    auto possessFunc = FindFunction(L"Function Engine.Controller.Possess");
    if (possessFunc) {
        struct PossessParams { SDK::APawn* Pawn; } possess{reinterpret_cast<SDK::APawn*>(husk)};
        ai->ProcessEvent(possessFunc, &possess);
    }
}

void Features::SetupGameplay() {
    auto controller = GetLocalPlayerController();
    if (!controller) {
        return;
    }
    auto giveItemFunc = FindFunction(L"Function FortniteGame.FortPlayerController.GiveItem");
    for (const auto& weapon : kWeaponIds) {
        if (!giveItemFunc) {
            break;
        }
        SDK::UObject* itemDef = FindObjectByName(std::wstring(weapon.begin(), weapon.end()));
        if (!itemDef) {
            continue;
        }
        struct Params {
            SDK::UObject* ItemDef;
            int32_t Count;
            int32_t Level;
            bool bSilent;
        } params{};
        params.ItemDef = itemDef;
        params.Count = 1;
        params.Level = 1;
        params.bSilent = true;
        controller->ProcessEvent(giveItemFunc, &params);
    }

    auto buildFunc = FindFunction(L"Function FortniteGame.FortPlayerController.ToggleBuildMode");
    if (buildFunc) {
        controller->ProcessEvent(buildFunc, nullptr);
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
