#pragma once

#include <Geode/loader/Mod.hpp>

class Config {
public:
    Config();

    static Config* get();

    geode::Severity getConsoleLogLevel();
    bool shouldLogMillisconds();
    int getHeartbeatThreshold();
    int getFontSize();
    bool hasConsole();
    bool isDarkModeEnabled();

private:
    geode::Mod* m_geode = nullptr;
    geode::Mod* m_mod = nullptr;
};