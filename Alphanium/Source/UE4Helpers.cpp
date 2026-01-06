#include "UE4Helpers.h"
#include "UE4Globals.h"
#include "Memory.h"
#include "Logger.h"
#include <windows.h>
#include <algorithm>

UE4Globals g_ue4{};

namespace {
bool IsLikelyNameEntry(const FNameEntry* entry) {
    if (!IsReadableAddress(entry, sizeof(FNameEntry))) {
        return false;
    }
    const char* name = entry->AnsiName;
    if (!IsReadableAddress(name, 4)) {
        return false;
    }
    return name[0] != '\0';
}
}

FNameEntry* GetNameEntry(int32_t index) {
    if (!g_ue4.GNames) {
        return nullptr;
    }
    auto names = reinterpret_cast<FNameEntryArray*>(g_ue4.GNames);
    if (names && IsReadableAddress(names, sizeof(FNameEntryArray)) && names->Entries) {
        return names->Entries[index];
    }
    auto direct = reinterpret_cast<FNameEntry**>(g_ue4.GNames);
    if (IsReadableAddress(direct, sizeof(void*) * (index + 1))) {
        auto entry = direct[index];
        if (IsLikelyNameEntry(entry)) {
            return entry;
        }
    }
    return nullptr;
}

FUObjectArray* GetGUObjectArray() {
    if (!g_ue4.GUObjectArray) {
        return nullptr;
    }
    auto arr = reinterpret_cast<FUObjectArray*>(g_ue4.GUObjectArray);
    if (IsReadableAddress(arr, sizeof(FUObjectArray))) {
        return arr;
    }
    return nullptr;
}

UFortEngine* GetEngine() {
    return reinterpret_cast<UFortEngine*>(FindObjectByName(L"FortEngine Transient.FortEngine_1"));
}

UWorld* GetWorld() {
    if (g_ue4.GWorld && IsReadableAddress(reinterpret_cast<void*>(g_ue4.GWorld), sizeof(void*))) {
        auto worldPtr = *reinterpret_cast<UWorld**>(g_ue4.GWorld);
        if (IsReadableAddress(worldPtr, sizeof(UWorld))) {
            return worldPtr;
        }
    }
    UFortEngine* engine = GetEngine();
    if (!engine) {
        LogMessage("GetWorld: FortEngine not found");
        return nullptr;
    }
    if (!IsReadableAddress(engine, sizeof(UFortEngine))) {
        LogMessage("GetWorld: FortEngine unreadable");
        return nullptr;
    }
    if (!engine->GameViewport) {
        LogMessage("GetWorld: GameViewport null");
        return nullptr;
    }
    if (!IsReadableAddress(engine->GameViewport, sizeof(UGameViewportClient))) {
        LogMessage("GetWorld: GameViewport unreadable");
        return nullptr;
    }
    UWorld* world = engine->GameViewport->World;
    if (!IsReadableAddress(world, sizeof(UWorld))) {
        LogMessage("GetWorld: World unreadable from GameViewport");
        return nullptr;
    }
    return world;
}

ProcessEventFn GetProcessEvent() {
    return g_ue4.ProcessEvent ? reinterpret_cast<ProcessEventFn>(g_ue4.ProcessEvent) : nullptr;
}

StaticFindObjectFn GetStaticFindObject() {
    return g_ue4.StaticFindObject ? reinterpret_cast<StaticFindObjectFn>(g_ue4.StaticFindObject) : nullptr;
}

std::string UObject::GetName() const {
    if (g_ue4.AppendString && IsReadableAddress(reinterpret_cast<void*>(g_ue4.AppendString), 1)) {
        struct FString {
            wchar_t* Data;
            int32_t Count;
            int32_t Max;
        } temp{};
        std::wstring buffer(1024, L'\0');
        temp.Data = buffer.data();
        temp.Count = 0;
        temp.Max = static_cast<int32_t>(buffer.size());
        using AppendStringFn = void(__cdecl*)(const FName*, FString&);
        auto fn = reinterpret_cast<AppendStringFn>(g_ue4.AppendString);
        fn(&NamePrivate, temp);
        if (temp.Count > 0) {
            std::wstring ws(temp.Data, temp.Count);
            std::string result(ws.begin(), ws.end());
            return result;
        }
    }
    FNameEntry* entry = GetNameEntry(NamePrivate.ComparisonIndex);
    if (!entry) {
        return "None";
    }
    return entry->AnsiName;
}

std::string UObject::GetFullName() const {
    if (!ClassPrivate) {
        return GetName();
    }
    std::string result = ClassPrivate->GetName();
    result += " ";
    const UObject* current = this;
    std::string outerName;
    while (current) {
        outerName = current->GetName();
        if (current->OuterPrivate) {
            outerName = current->OuterPrivate->GetName() + "." + outerName;
        }
        break;
    }
    result += outerName;
    return result;
}

bool UObject::IsA(const UClass* cls) const {
    if (!cls) {
        return false;
    }
    const UClass* current = ClassPrivate;
    while (current) {
        if (current == cls) {
            return true;
        }
        current = current->SuperStruct ? reinterpret_cast<UClass*>(current->SuperStruct) : nullptr;
    }
    return false;
}

FVector AActor::GetActorLocation() const {
    auto* root = reinterpret_cast<const FVector*>(RootComponent);
    if (!root) {
        return {0.0f, 0.0f, 0.0f};
    }
    return *root;
}

UObject* FindObjectByName(const std::wstring& name) {
    if (auto fn = GetStaticFindObject()) {
        return fn(nullptr, nullptr, name.c_str(), false);
    }
    auto* objects = GetGUObjectArray();
    if (!objects || !objects->Objects) {
        return nullptr;
    }
    for (int32_t i = 0; i < objects->NumElements; ++i) {
        auto& item = objects->Objects[i];
        if (!item.Object) {
            continue;
        }
        std::string full = item.Object->GetFullName();
        std::wstring full_w(full.begin(), full.end());
        if (full_w.find(name) != std::wstring::npos) {
            return item.Object;
        }
    }
    return nullptr;
}

UFunction* FindFunction(const std::wstring& name) {
    auto obj = FindObjectByName(name);
    return obj ? reinterpret_cast<UFunction*>(obj) : nullptr;
}

APlayerController* GetLocalPlayerController() {
    auto* objects = GetGUObjectArray();
    if (!objects || !objects->Objects) {
        return nullptr;
    }
    for (int32_t i = 0; i < objects->NumElements; ++i) {
        auto& item = objects->Objects[i];
        if (!item.Object) {
            continue;
        }
        std::string name = item.Object->GetFullName();
        if (name.find("PlayerController") != std::string::npos) {
            return reinterpret_cast<APlayerController*>(item.Object);
        }
    }
    return nullptr;
}

ACharacter* SpawnDefaultCharacter(const FVector& location) {
    UWorld* world = GetWorld();
    if (!world) {
        return nullptr;
    }
    auto spawnFunc = FindFunction(L"Function Engine.World.SpawnActor");
    if (!spawnFunc) {
        return nullptr;
    }
    UObject* characterClassObj = FindObjectByName(L"Class Engine.Character");
    if (!characterClassObj) {
        characterClassObj = FindObjectByName(L"Class Engine.DefaultPawn");
    }
    if (!characterClassObj) {
        return nullptr;
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
    params.Class = reinterpret_cast<UClass*>(characterClassObj);
    params.Location = location;
    params.Rotation = {0.0f, 0.0f, 0.0f};
    params.Owner = nullptr;
    params.Instigator = nullptr;
    params.bNoCollisionFail = true;
    params.bRemoteOwned = false;
    params.ReturnValue = nullptr;

    if (auto process = GetProcessEvent()) {
        process(reinterpret_cast<UObject*>(world), spawnFunc, &params);
        return reinterpret_cast<ACharacter*>(params.ReturnValue);
    }
    return nullptr;
}

void PossessPawn(APlayerController* controller, APawn* pawn) {
    if (!controller || !pawn) {
        return;
    }
    auto possessFunc = FindFunction(L"Function Engine.Controller.Possess");
    if (!possessFunc) {
        return;
    }
    struct Params { APawn* Pawn; } params{pawn};
    if (auto process = GetProcessEvent()) {
        process(reinterpret_cast<UObject*>(controller), possessFunc, &params);
    }
}

void SetActorLocation(AActor* actor, const FVector& location) {
    if (!actor) {
        return;
    }
    auto func = FindFunction(L"Function Engine.Actor.SetActorLocation");
    if (!func) {
        return;
    }
    struct Params {
        FVector NewLocation;
        bool bSweep;
        void* SweepHitResult;
        bool bTeleport;
        bool ReturnValue;
    } params{};
    params.NewLocation = location;
    params.bSweep = false;
    params.bTeleport = true;
    if (auto process = GetProcessEvent()) {
        process(reinterpret_cast<UObject*>(actor), func, &params);
    }
}

void DestroyActor(AActor* actor) {
    if (!actor) {
        return;
    }
    auto func = FindFunction(L"Function Engine.Actor.K2_DestroyActor");
    if (!func) {
        return;
    }
    if (auto process = GetProcessEvent()) {
        process(reinterpret_cast<UObject*>(actor), func, nullptr);
    }
}

void ExecuteConsoleCommand(UObject* worldContext, const std::string& command) {
    if (!worldContext) {
        LogMessage("ExecuteConsoleCommand: world context null for command '%s'", command.c_str());
        return;
    }
    auto func = FindFunction(L"Function Engine.KismetSystemLibrary.ExecuteConsoleCommand");
    if (!func) {
        LogMessage("ExecuteConsoleCommand: KismetSystemLibrary.ExecuteConsoleCommand not found");
        return;
    }
    UObject* kismetObj = FindObjectByName(L"Default__KismetSystemLibrary");
    if (!kismetObj) {
        kismetObj = FindObjectByName(L"Class Engine.KismetSystemLibrary");
    }
    if (!kismetObj) {
        LogMessage("ExecuteConsoleCommand: KismetSystemLibrary object not found");
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
        UObject* WorldContext;
        FString Command;
        APlayerController* SpecificPlayer;
    } params{};
    params.WorldContext = worldContext;
    params.Command = cmd;
    params.SpecificPlayer = nullptr;
    if (auto process = GetProcessEvent()) {
        LogMessage("ExecuteConsoleCommand: '%s' (Kismet=%p Function=%p)", command.c_str(), kismetObj, func);
        process(kismetObj, func, &params);
    } else {
        LogMessage("ExecuteConsoleCommand: ProcessEvent not resolved");
    }
}
