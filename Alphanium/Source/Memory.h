#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ModuleInfo {
    uintptr_t base = 0;
    size_t size = 0;
};

ModuleInfo GetMainModuleInfo();
uintptr_t FindPattern(const ModuleInfo& module, const std::vector<uint8_t>& pattern, const std::string& mask);
std::vector<uintptr_t> FindAllReferences(const ModuleInfo& module, const void* target, size_t max_results = 32);
uintptr_t FindStringInModule(const ModuleInfo& module, const std::string& str);
uintptr_t FindWideStringInModule(const ModuleInfo& module, const std::wstring& str);
bool IsReadableAddress(const void* address, size_t size = 1);
uintptr_t AlignDown(uintptr_t value, size_t alignment);
