#include "Memory.h"
#include <windows.h>
#include <psapi.h>

ModuleInfo GetMainModuleInfo() {
    HMODULE module = GetModuleHandleA(nullptr);
    MODULEINFO info{};
    if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info))) {
        return {};
    }
    ModuleInfo result;
    result.base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
    result.size = info.SizeOfImage;
    return result;
}

uintptr_t AlignDown(uintptr_t value, size_t alignment) {
    return value & ~(alignment - 1);
}

uintptr_t FindPattern(const ModuleInfo& module, const std::vector<uint8_t>& pattern, const std::string& mask) {
    if (pattern.empty() || pattern.size() != mask.size()) {
        return 0;
    }
    const uint8_t* start = reinterpret_cast<const uint8_t*>(module.base);
    size_t size = module.size;
    for (size_t i = 0; i + pattern.size() <= size; ++i) {
        bool matched = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (mask[j] == 'x' && start[i + j] != pattern[j]) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return module.base + i;
        }
    }
    return 0;
}

std::vector<uintptr_t> FindAllReferences(const ModuleInfo& module, const void* target, size_t max_results) {
    std::vector<uintptr_t> results;
    uintptr_t value = reinterpret_cast<uintptr_t>(target);
    const uint8_t* start = reinterpret_cast<const uint8_t*>(module.base);
    size_t size = module.size;
    for (size_t i = 0; i + sizeof(uintptr_t) <= size; i += 1) {
        uintptr_t current = *reinterpret_cast<const uintptr_t*>(start + i);
        if (current == value) {
            results.push_back(module.base + i);
            if (results.size() >= max_results) {
                break;
            }
        }
    }
    return results;
}

uintptr_t FindStringInModule(const ModuleInfo& module, const std::string& str) {
    if (str.empty()) {
        return 0;
    }
    const uint8_t* start = reinterpret_cast<const uint8_t*>(module.base);
    size_t size = module.size;
    for (size_t i = 0; i + str.size() <= size; ++i) {
        if (memcmp(start + i, str.data(), str.size()) == 0) {
            return module.base + i;
        }
    }
    return 0;
}

uintptr_t FindWideStringInModule(const ModuleInfo& module, const std::wstring& str) {
    if (str.empty()) {
        return 0;
    }
    const uint8_t* start = reinterpret_cast<const uint8_t*>(module.base);
    size_t size = module.size;
    const uint8_t* needle = reinterpret_cast<const uint8_t*>(str.data());
    size_t bytes = str.size() * sizeof(wchar_t);
    for (size_t i = 0; i + bytes <= size; ++i) {
        if (memcmp(start + i, needle, bytes) == 0) {
            return module.base + i;
        }
    }
    return 0;
}

bool IsReadableAddress(const void* address, size_t size) {
    if (!address) {
        return false;
    }
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(address, &mbi, sizeof(mbi))) {
        return false;
    }
    if (mbi.State != MEM_COMMIT) {
        return false;
    }
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) {
        return false;
    }
    uintptr_t start = reinterpret_cast<uintptr_t>(address);
    uintptr_t end = start + size;
    uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    return end <= regionEnd;
}
