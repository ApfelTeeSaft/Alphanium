#include "Resolver.h"
#include "Memory.h"
#include "UE4Globals.h"
#include <windows.h>
#include <sstream>

namespace {
uintptr_t FindFunctionByStringXref(const ModuleInfo& module, const std::string& name) {
    uintptr_t strAddr = FindStringInModule(module, name);
    if (!strAddr) {
        return 0;
    }
    auto refs = FindAllReferences(module, reinterpret_cast<void*>(strAddr));
    for (auto ref : refs) {
        // Search for a function prolog before the reference.
        for (int back = 0; back < 0x200; ++back) {
            uintptr_t candidate = ref - back;
            if (candidate < module.base) {
                break;
            }
            uint8_t* bytes = reinterpret_cast<uint8_t*>(candidate);
            if (bytes[0] == 0x55 && bytes[1] == 0x8B && bytes[2] == 0xEC) {
                return candidate;
            }
        }
    }
    return 0;
}

uintptr_t FindFunctionByWideStringXref(const ModuleInfo& module, const std::wstring& name) {
    uintptr_t strAddr = FindWideStringInModule(module, name);
    if (!strAddr) {
        return 0;
    }
    auto refs = FindAllReferences(module, reinterpret_cast<void*>(strAddr));
    for (auto ref : refs) {
        for (int back = 0; back < 0x200; ++back) {
            uintptr_t candidate = ref - back;
            if (candidate < module.base) {
                break;
            }
            uint8_t* bytes = reinterpret_cast<uint8_t*>(candidate);
            if (bytes[0] == 0x55 && bytes[1] == 0x8B && bytes[2] == 0xEC) {
                return candidate;
            }
        }
    }
    return 0;
}

uintptr_t ResolveByPattern(const ModuleInfo& module, const std::vector<uint8_t>& pattern, const std::string& mask, int offset) {
    uintptr_t addr = FindPattern(module, pattern, mask);
    if (!addr) {
        return 0;
    }
    return *reinterpret_cast<uintptr_t*>(addr + offset);
}

bool IsInModule(uintptr_t value, const ModuleInfo& module) {
    return value >= module.base && value < (module.base + module.size);
}

bool IsLikelyUObject(uintptr_t candidate, const ModuleInfo& module) {
    if (!IsReadableAddress(reinterpret_cast<void*>(candidate), sizeof(void*) * 2)) {
        return false;
    }
    uintptr_t vtable = *reinterpret_cast<uintptr_t*>(candidate);
    return IsInModule(vtable, module);
}

uintptr_t FindGWorldHeuristic(const ModuleInfo& module, DWORD maxMs) {
    uintptr_t start = module.base;
    uintptr_t end = module.base + module.size;
    MEMORY_BASIC_INFORMATION mbi{};
    DWORD startTick = GetTickCount();
    for (uintptr_t addr = start; addr < end; addr += mbi.RegionSize) {
        if (GetTickCount() - startTick > maxMs) {
            break;
        }
        if (!VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
            break;
        }
        if (mbi.AllocationBase != reinterpret_cast<void*>(module.base)) {
            continue;
        }
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_READONLY)) || (mbi.Protect & PAGE_GUARD)) {
            continue;
        }
        auto regionStart = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        auto regionEnd = regionStart + mbi.RegionSize;
        for (uintptr_t ptrAddr = regionStart; ptrAddr + sizeof(uintptr_t) <= regionEnd; ptrAddr += sizeof(uintptr_t)) {
            if (GetTickCount() - startTick > maxMs) {
                break;
            }
            uintptr_t candidate = *reinterpret_cast<uintptr_t*>(ptrAddr);
            if (!IsLikelyUObject(candidate, module)) {
                continue;
            }
            auto persistentLevelOffset = offsetof(UWorld, PersistentLevel);
            auto levelsOffset = offsetof(UWorld, Levels);
            if (!IsReadableAddress(reinterpret_cast<void*>(candidate + persistentLevelOffset), sizeof(uintptr_t))) {
                continue;
            }
            uintptr_t levelPtr = *reinterpret_cast<uintptr_t*>(candidate + persistentLevelOffset);
            if (!IsLikelyUObject(levelPtr, module)) {
                continue;
            }
            auto levels = reinterpret_cast<TArray<ULevel*>*>(candidate + levelsOffset);
            if (!IsReadableAddress(levels, sizeof(TArray<ULevel*>))) {
                continue;
            }
            if (levels->Count < 0 || levels->Count > 256 || levels->Max < levels->Count) {
                continue;
            }
            return ptrAddr;
        }
    }
    return 0;
}

uintptr_t FindGUObjectArrayHeuristic(const ModuleInfo& module, DWORD maxMs) {
    uintptr_t start = module.base;
    uintptr_t end = module.base + module.size;
    MEMORY_BASIC_INFORMATION mbi{};
    DWORD startTick = GetTickCount();
    for (uintptr_t addr = start; addr < end; addr += mbi.RegionSize) {
        if (GetTickCount() - startTick > maxMs) {
            break;
        }
        if (!VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
            break;
        }
        if (mbi.AllocationBase != reinterpret_cast<void*>(module.base)) {
            continue;
        }
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_READONLY)) || (mbi.Protect & PAGE_GUARD)) {
            continue;
        }
        auto regionStart = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        auto regionEnd = regionStart + mbi.RegionSize;
        for (uintptr_t ptrAddr = regionStart; ptrAddr + sizeof(FUObjectArray) <= regionEnd; ptrAddr += sizeof(uintptr_t)) {
            if (GetTickCount() - startTick > maxMs) {
                break;
            }
            auto* arr = reinterpret_cast<FUObjectArray*>(ptrAddr);
            if (!IsReadableAddress(arr, sizeof(FUObjectArray))) {
                continue;
            }
            if (arr->NumElements <= 0 || arr->NumElements > 5000000 || arr->MaxElements < arr->NumElements) {
                continue;
            }
            if (!IsReadableAddress(arr->Objects, sizeof(FUObjectItem) * 4)) {
                continue;
            }
            if (!IsReadableAddress(&arr->Objects[0], sizeof(FUObjectItem))) {
                continue;
            }
            if (!arr->Objects[0].Object) {
                continue;
            }
            if (!IsLikelyUObject(reinterpret_cast<uintptr_t>(arr->Objects[0].Object), module)) {
                continue;
            }
            return ptrAddr;
        }
    }
    return 0;
}

uintptr_t FindGNamesHeuristic(const ModuleInfo& module, DWORD maxMs) {
    const char* needle = "None";
    uintptr_t start = module.base;
    uintptr_t end = module.base + module.size;
    MEMORY_BASIC_INFORMATION mbi{};
    DWORD startTick = GetTickCount();
    for (uintptr_t addr = start; addr < end; addr += mbi.RegionSize) {
        if (GetTickCount() - startTick > maxMs) {
            break;
        }
        if (!VirtualQuery(reinterpret_cast<void*>(addr), &mbi, sizeof(mbi))) {
            break;
        }
        if (!(mbi.Protect & (PAGE_READWRITE | PAGE_READONLY)) || (mbi.Protect & PAGE_GUARD)) {
            continue;
        }
        auto regionStart = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        auto regionEnd = regionStart + mbi.RegionSize;
        for (uintptr_t ptrAddr = regionStart; ptrAddr + sizeof(uintptr_t) <= regionEnd; ptrAddr += sizeof(uintptr_t)) {
            if (GetTickCount() - startTick > maxMs) {
                break;
            }
            auto entryPtr = *reinterpret_cast<uintptr_t*>(ptrAddr);
            if (!IsReadableAddress(reinterpret_cast<void*>(entryPtr), 32)) {
                continue;
            }
            const char* name0 = reinterpret_cast<const char*>(entryPtr);
            const char* name10 = reinterpret_cast<const char*>(entryPtr + 0x10);
            if (IsReadableAddress(name0, 8) && memcmp(name0, needle, 4) == 0) {
                return ptrAddr;
            }
            if (IsReadableAddress(name10, 8) && memcmp(name10, needle, 4) == 0) {
                return ptrAddr;
            }
        }
    }
    return 0;
}
}

bool ResolveAllAddresses(std::string& log) {
    std::ostringstream oss;
    ModuleInfo module = GetMainModuleInfo();
    if (!module.base) {
        oss << "Failed to read module info.\n";
        log = oss.str();
        return false;
    }
    oss << "Module base: 0x" << std::hex << module.base << " size: 0x" << module.size << "\n";

    g_ue4.ProcessEvent = FindFunctionByStringXref(module, "ProcessEvent");
    if (!g_ue4.ProcessEvent) {
        g_ue4.ProcessEvent = FindFunctionByWideStringXref(module, L"ProcessEvent");
    }
    if (!g_ue4.ProcessEvent) {
        oss << "ProcessEvent not found via string xref.\n";
    } else {
        oss << "ProcessEvent resolved at 0x" << std::hex << g_ue4.ProcessEvent << " (string xref).\n";
    }

    g_ue4.StaticFindObject = FindFunctionByStringXref(module, "StaticFindObject");
    if (!g_ue4.StaticFindObject) {
        g_ue4.StaticFindObject = FindFunctionByWideStringXref(module, L"StaticFindObject");
    }
    if (!g_ue4.StaticFindObject) {
        oss << "StaticFindObject not found via string xref.\n";
    } else {
        oss << "StaticFindObject resolved at 0x" << std::hex << g_ue4.StaticFindObject << " (string xref).\n";
    }

    // GNames resolver: multiple UE4.12 x86 patterns. all don't work tho?
    // Signature A: A1 ?? ?? ?? ?? 8B 0C 85 ?? ?? ?? ?? 85 C9 74 0C
    {
        std::vector<uint8_t> pattern = {0xA1, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x0C, 0x85, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC9, 0x74, 0x0C};
        std::string mask = "x????xxx????xxxx";
        g_ue4.GNames = ResolveByPattern(module, pattern, mask, 1);
        if (!g_ue4.GNames) {
            // Signature B: 8B 0D ?? ?? ?? ?? 8B 04 85 ?? ?? ?? ?? 85 C0
            std::vector<uint8_t> pattern2 = {0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x04, 0x85, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0};
            std::string mask2 = "xx????xxx????xx";
            g_ue4.GNames = ResolveByPattern(module, pattern2, mask2, 2);
        }
        if (!g_ue4.GNames) {
            // Signature C: A1 ?? ?? ?? ?? 8B 04 85 ?? ?? ?? ?? 85 C0 74 0B
            std::vector<uint8_t> pattern3 = {0xA1, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x04, 0x85, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x0B};
            std::string mask3 = "x????xxx????xxxx";
            g_ue4.GNames = ResolveByPattern(module, pattern3, mask3, 1);
        }
        if (g_ue4.GNames) {
            oss << "GNames resolved at 0x" << std::hex << g_ue4.GNames << " (pattern).\n";
        } else {
            oss << "GNames pattern failed.\n";
        }
    }
    if (!g_ue4.GNames) {
        oss << "GNames heuristic scan start.\n";
        g_ue4.GNames = FindGNamesHeuristic(module, 3000);
        if (g_ue4.GNames) {
            oss << "GNames resolved at 0x" << std::hex << g_ue4.GNames << " (heuristic).\n";
        } else {
            oss << "GNames heuristic failed or timed out.\n";
        }
    }

    // GUObjectArray resolver: multiple UE4.12 x86 patterns. all don't work here too lol
    // Signature A: A1 ?? ?? ?? ?? 8B 0C 85 ?? ?? ?? ?? 8B 04 8D
    {
        std::vector<uint8_t> pattern = {0xA1, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x0C, 0x85, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x04, 0x8D};
        std::string mask = "x????xxx????xxx";
        g_ue4.GUObjectArray = ResolveByPattern(module, pattern, mask, 1);
        if (!g_ue4.GUObjectArray) {
            // Signature B: 8B 0D ?? ?? ?? ?? 8B 04 8D ?? ?? ?? ?? 8B 40 08
            std::vector<uint8_t> pattern2 = {0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x04, 0x8D, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x40, 0x08};
            std::string mask2 = "xx????xxx????xxx";
            g_ue4.GUObjectArray = ResolveByPattern(module, pattern2, mask2, 2);
        }
        if (!g_ue4.GUObjectArray) {
            // Signature C: A1 ?? ?? ?? ?? 8B 04 85 ?? ?? ?? ?? 8B 40 10 8B 0D
            std::vector<uint8_t> pattern3 = {0xA1, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x04, 0x85, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x40, 0x10, 0x8B, 0x0D};
            std::string mask3 = "x????xxx????xxxx";
            g_ue4.GUObjectArray = ResolveByPattern(module, pattern3, mask3, 1);
        }
        if (g_ue4.GUObjectArray) {
            oss << "GUObjectArray resolved at 0x" << std::hex << g_ue4.GUObjectArray << " (pattern).\n";
        } else {
            oss << "GUObjectArray pattern failed.\n";
        }
    }
    if (!g_ue4.GUObjectArray) {
        oss << "GUObjectArray heuristic scan start.\n";
        g_ue4.GUObjectArray = FindGUObjectArrayHeuristic(module, 3000);
        if (g_ue4.GUObjectArray) {
            oss << "GUObjectArray resolved at 0x" << std::hex << g_ue4.GUObjectArray << " (heuristic).\n";
        } else {
            oss << "GUObjectArray heuristic failed or timed out.\n";
        }
    }

    // GWorld resolver: signature based on UE4.12 x86 global world pointer usage. still brokeeeen
    // Signature A: A1 ?? ?? ?? ?? 8B 40 30 85 C0 74 10
    {
        std::vector<uint8_t> pattern = {0xA1, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x40, 0x30, 0x85, 0xC0, 0x74, 0x10};
        std::string mask = "x????xxxxxxx";
        g_ue4.GWorld = ResolveByPattern(module, pattern, mask, 1);
        if (!g_ue4.GWorld) {
            // Signature B: 8B 0D ?? ?? ?? ?? 8B 81 ?? ?? ?? ?? 85 C0
            std::vector<uint8_t> pattern2 = {0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x81, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0};
            std::string mask2 = "xx????xx????xx";
            g_ue4.GWorld = ResolveByPattern(module, pattern2, mask2, 2);
        }
        if (!g_ue4.GWorld) {
            // Signature C: A1 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 85 C0 74 0C
            std::vector<uint8_t> pattern3 = {0xA1, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x80, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x0C};
            std::string mask3 = "x????xx????xxxx";
            g_ue4.GWorld = ResolveByPattern(module, pattern3, mask3, 1);
        }
        if (g_ue4.GWorld) {
            oss << "GWorld resolved at 0x" << std::hex << g_ue4.GWorld << " (pattern).\n";
        } else {
            oss << "GWorld pattern failed.\n";
        }
    }
    if (!g_ue4.GWorld) {
        oss << "GWorld heuristic scan start.\n";
        g_ue4.GWorld = FindGWorldHeuristic(module, 3000);
        if (g_ue4.GWorld) {
            oss << "GWorld resolved at 0x" << std::hex << g_ue4.GWorld << " (heuristic).\n";
        } else {
            oss << "GWorld heuristic failed or timed out.\n";
        }
    }

    log = oss.str();
    return g_ue4.ProcessEvent && g_ue4.GWorld;
}
