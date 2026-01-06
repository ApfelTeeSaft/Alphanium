#pragma once
#include <cstdint>
#include <string>

struct FNameEntry {
    int32_t Index;
    char AnsiName[1024];
};

struct FName {
    int32_t ComparisonIndex;
    int32_t Number;
};

template <typename T>
struct TArray {
    T* Data;
    int32_t Count;
    int32_t Max;

    T& operator[](int32_t idx) { return Data[idx]; }
    const T& operator[](int32_t idx) const { return Data[idx]; }
};

struct UObject;
struct UClass;
struct UFunction;

struct UObject {
    void** VTable;
    int32_t ObjectFlags;
    int32_t InternalIndex;
    UClass* ClassPrivate;
    FName NamePrivate;
    UObject* OuterPrivate;

    std::string GetName() const;
    std::string GetFullName() const;
    bool IsA(const UClass* cls) const;
};

struct UField : UObject {
    UField* Next;
};

struct UStruct : UField {
    UStruct* SuperStruct;
    UField* Children;
    int32_t PropertySize;
    int32_t MinAlignment;
};

struct UFunction : UStruct {
    int32_t FunctionFlags;
    int16_t RepOffset;
    int16_t NumParms;
    int16_t ParmsSize;
    int16_t ReturnValueOffset;
};

struct UClass : UStruct {
    void* ClassConstructor;
    void* ClassVTableHelperCtorCaller;
    void* ClassAddReferencedObjects;
    int32_t ClassFlags;
};

struct FVector {
    float X;
    float Y;
    float Z;
};

struct FRotator {
    float Pitch;
    float Yaw;
    float Roll;
};

struct FTransform {
    FVector Translation;
    FRotator Rotation;
    FVector Scale3D;
};

struct AActor;
struct UWorld;
struct ULevel;
struct APlayerController;
struct APawn;
struct ACharacter;
struct AGameModeBase;
struct AGameStateBase;

struct AActor : UObject {
    void* RootComponent;
    FVector GetActorLocation() const;
};

struct APlayerController : AActor {
    APawn* AcknowledgedPawn;
};

struct APawn : AActor {};
struct ACharacter : APawn {};

struct ULevel : UObject {
    TArray<AActor*> Actors;
};

struct UWorld : UObject {
    ULevel* PersistentLevel;
    AGameModeBase* AuthorityGameMode;
    AGameStateBase* GameState;
    TArray<ULevel*> Levels;
};

struct AGameModeBase : AActor {};
struct AGameStateBase : AActor {};

struct UGameViewportClient : UObject {
    char UnknownData00[0x58];
    UWorld* World;
};

struct UEngine : UObject {
    char UnknownData00[0x410];
    UGameViewportClient* GameViewport;
};

struct UGameEngine : UEngine {
    char UnknownData01[0x14C];
};

struct UFortEngine : UGameEngine {};

struct FNameEntryArray {
    FNameEntry** Entries;
};

struct FUObjectItem {
    UObject* Object;
    int32_t Flags;
    int32_t ClusterIndex;
    int32_t SerialNumber;
};

struct FUObjectArray {
    FUObjectItem* Objects;
    int32_t MaxElements;
    int32_t NumElements;
};
