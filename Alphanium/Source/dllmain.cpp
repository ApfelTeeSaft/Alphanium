#include <windows.h>
#include <string>
#include "Resolver.h"
#include "Hooking.h"
#include "StandaloneWindow.h"
#include "Logger.h"

LONG WINAPI CrashHandler(EXCEPTION_POINTERS* info) {
    LogMessage("Alphanium: unhandled exception 0x%08X at 0x%p",
               info->ExceptionRecord->ExceptionCode,
               info->ExceptionRecord->ExceptionAddress);
    FlushLog();
    return EXCEPTION_EXECUTE_HANDLER;
}

DWORD WINAPI MainThread(LPVOID) {
    InitializeConsole();
    SetUnhandledExceptionFilter(CrashHandler);
    LogMessage("Alphanium: initializing");
    std::string log;
    ResolveAllAddresses(log);
    LogMessage("%s", log.c_str());
    InitializeHooks();
    LogMessage("Alphanium: hooks initialized");
    StartStandaloneOverlay();
    LogMessage("Alphanium: standalone window started");
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        StopStandaloneOverlay();
        ShutdownHooks();
    }
    return TRUE;
}
