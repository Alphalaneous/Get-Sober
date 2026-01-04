#include <Geode/Geode.hpp>
#include "Config.hpp"
#include "Utils.hpp"

using namespace geode::prelude;

Config* Config::get() {
    static Config instance;
    return &instance;
}

Config::Config() {
    m_geode = Loader::get()->getLoadedMod("geode.loader");
    m_mod = Mod::get();
}

Severity Config::getConsoleLogLevel() {
    static auto setting = sobriety::utils::fromString(m_geode->getSettingValue<std::string>("console-log-level"));
    static auto listener = listenForSettingChanges("console-log-level", [this](std::string value) {
        setting = sobriety::utils::fromString(value);
    }, m_geode);

    return setting;
}

bool Config::shouldLogMillisconds() {
    static auto setting = m_geode->getSettingValue<bool>("log-milliseconds");
    static auto listener = listenForSettingChanges("log-milliseconds", [this](bool value) {
        setting = value;
    }, m_geode);

    return setting;
}

int Config::getHeartbeatThreshold() {
    static auto setting = m_mod->getSettingValue<int>("console-heartbeat-threshold");
    static auto listener = listenForSettingChanges("console-heartbeat-threshold", [this](int value) {
        setting = value;
    });

    return setting;
}

int Config::getFontSize() {
    static auto setting = m_mod->getSettingValue<int>("console-font-size");
    return setting;
}

bool Config::hasConsole() {
    static bool setting = m_geode->getSettingValue<bool>("show-platform-console");
    return setting;
}

bool Config::isDarkModeEnabled() {
    static auto setting = m_mod->getSettingValue<bool>("enable-dark-mode");
    static auto listener = listenForSettingChanges("enable-dark-mode", [this](bool value) {
        setting = value;
    });

    return setting;
}
