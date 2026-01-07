#include "Resolver.h"
#include "Memory.h"
#include "SdkTypes.h"
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

    const uintptr_t gObjects = module.base + SDK::Offsets::GObjects;
    const uintptr_t gNames = module.base + SDK::Offsets::GNames;
    const uintptr_t gWorld = module.base + SDK::Offsets::GWorld;
    const uintptr_t processEvent = module.base + SDK::Offsets::ProcessEvent;
    const uintptr_t appendString = module.base + SDK::Offsets::AppendString;

    SDK::UObject::GObjects.InitManually(reinterpret_cast<void*>(gObjects));
    SDK::FName::InitManually(reinterpret_cast<void*>(appendString));

    if (!IsReadableAddress(reinterpret_cast<void*>(gNames), sizeof(void*))) {
        oss << "GNames address not readable.\n";
    }
    if (!IsReadableAddress(reinterpret_cast<void*>(gObjects), sizeof(void*))) {
        oss << "GUObjectArray address not readable.\n";
    }
    if (!IsReadableAddress(reinterpret_cast<void*>(gWorld), sizeof(void*))) {
        oss << "GWorld address not readable.\n";
    }
    if (!IsReadableAddress(reinterpret_cast<void*>(processEvent), sizeof(void*))) {
        oss << "ProcessEvent address not readable.\n";
    }
    if (!IsReadableAddress(reinterpret_cast<void*>(appendString), sizeof(void*))) {
        oss << "AppendString address not readable.\n";
    }

    oss << "GNames (offset): 0x" << std::hex << gNames << "\n";
    oss << "GUObjectArray (offset): 0x" << std::hex << gObjects << "\n";
    oss << "GWorld (offset): 0x" << std::hex << gWorld << "\n";
    oss << "ProcessEvent (offset): 0x" << std::hex << processEvent << "\n";
    oss << "AppendString (offset): 0x" << std::hex << appendString << "\n";
    oss << "ProcessEventIdx (offset): 0x" << std::hex << SDK::Offsets::ProcessEventIdx << "\n";

    log = oss.str();
    return processEvent != 0 && gWorld != 0;
}
