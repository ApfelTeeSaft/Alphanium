#pragma once
#include "UE4Types.h"

UObject* FindObjectByName(const std::wstring& name);
UFunction* FindFunction(const std::wstring& name);

APlayerController* GetLocalPlayerController();
UFortEngine* GetEngine();
ACharacter* SpawnDefaultCharacter(const FVector& location);
void PossessPawn(APlayerController* controller, APawn* pawn);

void SetActorLocation(AActor* actor, const FVector& location);
void DestroyActor(AActor* actor);
void ExecuteConsoleCommand(UObject* worldContext, const std::string& command);
