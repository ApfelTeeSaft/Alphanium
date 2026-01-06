#include "Logger.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <mutex>

namespace {
    bool g_consoleReady = false;
    FILE* g_logFile = nullptr;
    std::mutex g_logMutex;

    std::string GetLogPath() {
        char buffer[MAX_PATH] = {};
        GetModuleFileNameA(reinterpret_cast<HMODULE>(&GetLogPath), buffer, MAX_PATH);
        std::string path = buffer;
        size_t pos = path.find_last_of("\\/");
        if (pos != std::string::npos) {
            path = path.substr(0, pos);
        }
        else {
            path = ".";
        }
        path += "\\Alphanium.log";
        return path;
    }
}

void InitializeConsole() {
    if (g_consoleReady) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        std::string logPath = GetLogPath();
        fopen_s(&g_logFile, logPath.c_str(), "w");
    }
    if (AllocConsole()) {
        FILE* out = nullptr;
        freopen_s(&out, "CONOUT$", "w", stdout);
        FILE* err = nullptr;
        freopen_s(&err, "CONOUT$", "w", stderr);
        g_consoleReady = true;
    }
}

void LogMessage(const char* fmt, ...) {
    if (!g_consoleReady && !g_logFile) {
        return;
    }
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_logFile) {
            fprintf(g_logFile, "%s\n", buffer);
            fflush(g_logFile);
        }
    }
    if (g_consoleReady) {
        printf("%s\n", buffer);
    }
}

void FlushLog() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
        fflush(g_logFile);
    }
}