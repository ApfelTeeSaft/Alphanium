#include "Strings.h"
#include <windows.h>
#include <fstream>
#include <unordered_set>
#include "Logger.h"

namespace {
std::string GetModuleDir() {
    char buffer[MAX_PATH] = {};
    GetModuleFileNameA(reinterpret_cast<HMODULE>(&GetModuleDir), buffer, MAX_PATH);
    std::string path = buffer;
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return ".";
    }
    return path.substr(0, pos);
}

std::string GetCurrentDir() {
    char buffer[MAX_PATH] = {};
    GetCurrentDirectoryA(MAX_PATH, buffer);
    return buffer;
}

std::string NormalizeName(const std::string& name) {
    std::string result;
    for (char c : name) {
        if (c == '\r' || c == '\n' || c == '\t') {
            continue;
        }
        result.push_back(c);
    }
    return result;
}
}