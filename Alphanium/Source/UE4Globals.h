#pragma once
#include <cstdint>
#include "UE4Types.h"

struct UE4Globals {
    uintptr_t GNames = 0;
    uintptr_t GUObjectArray = 0;
    uintptr_t GWorld = 0;
    uintptr_t ProcessEvent = 0;
    uintptr_t StaticFindObject = 0;
};

extern UE4Globals g_ue4;

FNameEntry* GetNameEntry(int32_t index);
FUObjectArray* GetGUObjectArray();
UWorld* GetWorld();

using ProcessEventFn = void(__thiscall*)(UObject* obj, UFunction* function, void* params);
using StaticFindObjectFn = UObject* (__cdecl*)(UClass* cls, UObject* outer, const wchar_t* name, bool exact);

ProcessEventFn GetProcessEvent();
StaticFindObjectFn GetStaticFindObject();
