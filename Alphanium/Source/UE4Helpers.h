#pragma once
#include "SdkTypes.h"

SDK::UObject* FindObjectByName(const std::wstring& name);
SDK::UFunction* FindFunction(const std::wstring& name);

SDK::APlayerController* GetLocalPlayerController();
SDK::UFortEngine* GetEngine();
SDK::UWorld* GetWorld();
SDK::ACharacter* SpawnDefaultCharacter(const SDK::FVector& location);
void PossessPawn(SDK::APlayerController* controller, SDK::APawn* pawn);

void SetActorLocation(SDK::AActor* actor, const SDK::FVector& location);
void DestroyActor(SDK::AActor* actor);
void ExecuteConsoleCommand(SDK::UObject* worldContext, const std::string& command);
