#pragma once

#include "sync_manager.h"
#include <Windows.h>
#include <thread>
#include <atomic>

namespace KenshiMP {

/// Runs sync tick in background thread (game has no exposed main loop)
class GameLoop {
public:
    GameLoop();
    ~GameLoop();

    void Start();
    void Stop();

private:
    void Loop();

    SyncManager m_sync;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
};

} // namespace KenshiMP
