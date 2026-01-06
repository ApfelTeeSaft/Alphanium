#include "Logger.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>

namespace {
bool g_consoleReady = false;
}

void InitializeConsole() {
    if (g_consoleReady) {
        return;
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
    if (!g_consoleReady) {
        return;
    }
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    printf("%s\n", buffer);
}
