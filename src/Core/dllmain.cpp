#include "logger.h"
#include "memory.h"
#include "game_memory.h"
#include "game_loop.h"
#include "exception_handler.h"
#include <Windows.h>

static KenshiMP::GameLoop* s_game_loop = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        KenshiMP::InstallExceptionHandler();
        KenshiMP::Logger::Init();
        KenshiMP::Logger::Info("KenshiMP_Core loaded successfully (injection verified)");
        KenshiMP::Memory::Init();
        KenshiMP::GameMemory::Init();
        s_game_loop = new KenshiMP::GameLoop();
        s_game_loop->Start();
        break;

    case DLL_PROCESS_DETACH:
        if (s_game_loop) {
            s_game_loop->Stop();
            delete s_game_loop;
            s_game_loop = nullptr;
        }
        KenshiMP::Logger::Info("KenshiMP_Core unloading");
        KenshiMP::GameMemory::Shutdown();
        KenshiMP::Memory::Shutdown();
        KenshiMP::Logger::Shutdown();
        KenshiMP::UninstallExceptionHandler();
        break;
    }
    return TRUE;
}
