#include "game_loop.h"
#include "mp_config.h"
#include "mod_check.h"
#include "logger.h"
#include <chrono>

namespace KenshiMP {

GameLoop::GameLoop() = default;

GameLoop::~GameLoop() {
    Stop();
}

void GameLoop::Start() {
    auto config = LoadMPConfig();
    if (!config.enabled) {
        Logger::Info("Multiplayer disabled in config");
        return;
    }

    m_running = true;

    uint64_t mod_hash = 0;
    if (IsModListCheckEnabled()) {
        auto game_dir = GetGameDirectory();
        auto mods = ReadGameModList(game_dir);
        mod_hash = ComputeModListHash64(mods);
        Logger::Info("MOD list hash: " + std::to_string(mod_hash) + " (" + std::to_string(mods.size()) + " mods)");
    }

    if (config.role == "host") {
        m_sync.StartHost(config.port, mod_hash);
    } else {
        m_sync.StartClient(config.host, config.port, mod_hash, config.player_name);
    }

    m_thread = std::thread(&GameLoop::Loop, this);
    Logger::Info("GameLoop started as " + config.role);
}

void GameLoop::Stop() {
    m_running = false;
    m_sync.Stop();
    if (m_thread.joinable())
        m_thread.join();
}

void GameLoop::Loop() {
    while (m_running) {
        m_sync.Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));  // ~20 Hz
    }
}

} // namespace KenshiMP
