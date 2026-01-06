#include "Resolver.h"
#include "Memory.h"
#include "UE4Globals.h"
#include <sstream>

bool ResolveAllAddresses(std::string& log) {
    std::ostringstream oss;
    ModuleInfo module = GetMainModuleInfo();
    if (!module.base) {
        oss << "Failed to read module info.\n";
        log = oss.str();
        return false;
    }
    oss << "Module base: 0x" << std::hex << module.base << " size: 0x" << module.size << "\n";

    constexpr uintptr_t OFFSET_GOBJECTS = 47987372;
    constexpr uintptr_t OFFSET_GNAMES = 47957416;
    constexpr uintptr_t OFFSET_GWORLD = 48663532;
    constexpr uintptr_t OFFSET_PROCESSEVENT = 10133104;
    constexpr uintptr_t OFFSET_APPENDSTRING = 9483872;

    g_ue4.GUObjectArray = module.base + OFFSET_GOBJECTS;
    g_ue4.GNames = module.base + OFFSET_GNAMES;
    g_ue4.GWorld = module.base + OFFSET_GWORLD;
    g_ue4.ProcessEvent = module.base + OFFSET_PROCESSEVENT;
    g_ue4.StaticFindObject = 0;

    if (!IsReadableAddress(reinterpret_cast<void*>(g_ue4.GNames), sizeof(void*))) {
        oss << "GNames address not readable, disabling.\n";
        g_ue4.GNames = 0;
    }
    if (!IsReadableAddress(reinterpret_cast<void*>(g_ue4.GUObjectArray), sizeof(void*))) {
        oss << "GUObjectArray address not readable, disabling.\n";
        g_ue4.GUObjectArray = 0;
    }
    if (!IsReadableAddress(reinterpret_cast<void*>(g_ue4.GWorld), sizeof(void*))) {
        oss << "GWorld address not readable, disabling.\n";
        g_ue4.GWorld = 0;
    }
    if (!IsReadableAddress(reinterpret_cast<void*>(g_ue4.ProcessEvent), sizeof(void*))) {
        oss << "ProcessEvent address not readable, disabling.\n";
        g_ue4.ProcessEvent = 0;
    }

    oss << "GNames (offset): 0x" << std::hex << g_ue4.GNames << "\n";
    oss << "GUObjectArray (offset): 0x" << std::hex << g_ue4.GUObjectArray << "\n";
    oss << "GWorld (offset): 0x" << std::hex << g_ue4.GWorld << "\n";
    oss << "ProcessEvent (offset): 0x" << std::hex << g_ue4.ProcessEvent << "\n";
    oss << "AppendString (offset): 0x" << std::hex << (module.base + OFFSET_APPENDSTRING) << "\n";

    log = oss.str();
    return g_ue4.ProcessEvent && g_ue4.GWorld;
}