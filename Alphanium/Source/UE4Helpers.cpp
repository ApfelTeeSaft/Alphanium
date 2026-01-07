#include "UE4Helpers.h"
#include "Memory.h"
#include "Logger.h"
#include <windows.h>
#include <algorithm>

namespace {
SDK::TUObjectArray* GetObjectArray() {
    auto* objects = SDK::UObject::GObjects.GetTypedPtr();
    if (!objects) {
        return nullptr;
    }
    if (!IsReadableAddress(objects, sizeof(SDK::TUObjectArray))) {
        return nullptr;
    }
    return objects;
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) {
        return {};
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) {
        return {};
    }
    std::string output(static_cast<size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, output.data(), size, nullptr, nullptr);
    return output;
}
}

SDK::UFortEngine* GetEngine() {
    return reinterpret_cast<SDK::UFortEngine*>(FindObjectByName(L"FortEngine Transient.FortEngine_1"));
}

SDK::UWorld* GetWorld() {
    SDK::UEngine* engine = SDK::UEngine::GetEngine();
    if (!engine) {
        return nullptr;
    }
    SDK::UGameViewportClient* viewport = engine->GameViewport;
    if (!viewport) {
        return nullptr;
    }
    return viewport->World;
}

SDK::UObject* FindObjectByName(const std::wstring& name) {
    const std::string query = WideToUtf8(name);
    if (!query.empty()) {
        if (auto exact = SDK::UObject::FindObject(query)) {
            return exact;
        }
    }
    auto* objects = GetObjectArray();
    if (!objects) {
        return nullptr;
    }
    for (int32_t i = 0; i < objects->Num(); ++i) {
        SDK::UObject* obj = objects->GetByIndex(i);
        if (!obj) {
            continue;
        }
        std::string full = obj->GetFullName();
        if (!query.empty() && full.find(query) == std::string::npos) {
            continue;
        }
        if (query.empty()) {
            std::wstring full_w(full.begin(), full.end());
            if (full_w.find(name) == std::wstring::npos) {
                continue;
            }
        }
        return obj;
    }
    return nullptr;
}

SDK::UFunction* FindFunction(const std::wstring& name) {
    auto obj = FindObjectByName(name);
    return obj ? reinterpret_cast<SDK::UFunction*>(obj) : nullptr;
}

SDK::APlayerController* GetLocalPlayerController() {
    auto* objects = GetObjectArray();
    if (!objects) {
        return nullptr;
    }
    for (int32_t i = 0; i < objects->Num(); ++i) {
        SDK::UObject* obj = objects->GetByIndex(i);
        if (!obj) {
            continue;
        }
        std::string name = obj->GetFullName();
        if (name.find("PlayerController") != std::string::npos) {
            return reinterpret_cast<SDK::APlayerController*>(obj);
        }
    }
    return nullptr;
}

SDK::ACharacter* SpawnDefaultCharacter(const SDK::FVector& location) {
    SDK::UWorld* world = GetWorld();
    if (!world) {
        return nullptr;
    }
    auto spawnFunc = FindFunction(L"Function Engine.World.SpawnActor");
    if (!spawnFunc) {
        return nullptr;
    }
    SDK::UObject* characterClassObj = FindObjectByName(L"Class Engine.Character");
    if (!characterClassObj) {
        characterClassObj = FindObjectByName(L"Class Engine.DefaultPawn");
    }
    if (!characterClassObj) {
        return nullptr;
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
    params.Class = reinterpret_cast<SDK::UClass*>(characterClassObj);
    params.Location = location;
    params.Rotation = {0.0f, 0.0f, 0.0f};
    params.Owner = nullptr;
    params.Instigator = nullptr;
    params.bNoCollisionFail = true;
    params.bRemoteOwned = false;
    params.ReturnValue = nullptr;

    world->ProcessEvent(spawnFunc, &params);
    return reinterpret_cast<SDK::ACharacter*>(params.ReturnValue);
}

void PossessPawn(SDK::APlayerController* controller, SDK::APawn* pawn) {
    if (!controller || !pawn) {
        return;
    }
    auto possessFunc = FindFunction(L"Function Engine.Controller.Possess");
    if (!possessFunc) {
        return;
    }
    struct Params { SDK::APawn* Pawn; } params{pawn};
    controller->ProcessEvent(possessFunc, &params);
}

void SetActorLocation(SDK::AActor* actor, const SDK::FVector& location) {
    if (!actor) {
        return;
    }
    actor->K2_SetActorLocation(location, false, nullptr, true);
}

void DestroyActor(SDK::AActor* actor) {
    if (!actor) {
        return;
    }
    actor->K2_DestroyActor();
}

void ExecuteConsoleCommand(SDK::UObject* worldContext, const std::string& command) {
    if (!worldContext) {
        LogMessage("ExecuteConsoleCommand: world context null for command '%s'", command.c_str());
        return;
    }
    auto func = FindFunction(L"Function Engine.KismetSystemLibrary.ExecuteConsoleCommand");
    if (!func) {
        LogMessage("ExecuteConsoleCommand: KismetSystemLibrary.ExecuteConsoleCommand not found");
        return;
    }
    SDK::UObject* kismetObj = FindObjectByName(L"Default__KismetSystemLibrary");
    if (!kismetObj) {
        kismetObj = FindObjectByName(L"Class Engine.KismetSystemLibrary");
    }
    if (!kismetObj) {
        LogMessage("ExecuteConsoleCommand: KismetSystemLibrary object not found");
        return;
    }
    std::wstring wide(command.begin(), command.end());
    SDK::FString cmd(wide.c_str());
    struct Params {
        SDK::UObject* WorldContext;
        SDK::FString Command;
        SDK::APlayerController* SpecificPlayer;
    } params{};
    params.WorldContext = worldContext;
    params.Command = cmd;
    params.SpecificPlayer = nullptr;
    LogMessage("ExecuteConsoleCommand: '%s' (Kismet=%p Function=%p)", command.c_str(), kismetObj, func);
    kismetObj->ProcessEvent(func, &params);
}
