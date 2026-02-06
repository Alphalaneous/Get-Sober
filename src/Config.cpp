#include <Geode/Geode.hpp>
#include "Config.hpp"
#include "Console.hpp"
#include "Utils.hpp"

using namespace geode::prelude;

Config* Config::get() {
    static Config instance;
    return &instance;
}

Config::Config() {
    m_geode = Loader::get()->getLoadedMod("geode.loader");
    m_mod = Mod::get();
    auto now = std::chrono::system_clock::now();
    auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    m_uniquePath = std::filesystem::path(fmt::format("/tmp/GeometryDash-{}/", nowMs));
}

Severity Config::getConsoleLogLevel() {
    static auto setting = sobriety::utils::fromString(m_geode->getSettingValue<std::string>("console-log-level"));
    static auto listener = listenForSettingChanges<std::string>("console-log-level", [](std::string value) {
        setting = sobriety::utils::fromString(value);
    }, m_geode);

    return setting;
}

bool Config::shouldLogMillisconds() {
    static auto setting = m_geode->getSettingValue<bool>("log-milliseconds");
    static auto listener = listenForSettingChanges<bool>("log-milliseconds", [](bool value) {
        setting = value;
    }, m_geode);

    return setting;
}

int Config::getHeartbeatThreshold() {
    static auto setting = m_mod->getSettingValue<int>("console-heartbeat-threshold");
    static auto listener = listenForSettingChanges<int>("console-heartbeat-threshold", [](int value) {
        setting = value;
    });

    return setting;
}

int Config::getFontSize() {
    static auto setting = m_mod->getSettingValue<int>("console-font-size");
    return setting;
}

cocos2d::ccColor3B Config::getConsoleForegroundColor() {
    static auto setting = m_mod->getSettingValue<ccColor3B>("console-foreground-color");
    static auto listener = listenForSettingChanges<ccColor3B>("console-foreground-color", [](ccColor3B value) {
        setting = value;
        Console::get()->setConsoleColors();
    });
    return setting;
}

cocos2d::ccColor3B Config::getConsoleBackgroundColor() {
    static auto setting = m_mod->getSettingValue<ccColor3B>("console-background-color");
    static auto listener = listenForSettingChanges<ccColor3B>("console-background-color", [](ccColor3B value) {
        setting = value;
        Console::get()->setConsoleColors();
    });
    return setting;
}

cocos2d::ccColor3B Config::getLogInfoColor() {
    static auto setting = m_mod->getSettingValue<ccColor3B>("console-log-info-color");
    static auto listener = listenForSettingChanges<ccColor3B>("console-log-info-color", [](ccColor3B value) {
        setting = value;
        Console::get()->setConsoleColors();
    });
    return setting;
}

cocos2d::ccColor3B Config::getLogWarnColor() {
    static auto setting = m_mod->getSettingValue<ccColor3B>("console-log-warn-color");
    static auto listener = listenForSettingChanges<ccColor3B>("console-log-warn-color", [](ccColor3B value) {
        setting = value;
        Console::get()->setConsoleColors();
    });
    return setting;
}

cocos2d::ccColor3B Config::getLogErrorColor() {
    static auto setting = m_mod->getSettingValue<ccColor3B>("console-log-error-color");
    static auto listener = listenForSettingChanges<ccColor3B>("console-log-error-color", [](ccColor3B value) {
        setting = value;
        Console::get()->setConsoleColors();
    });
    return setting;
}

cocos2d::ccColor3B Config::getLogDebugColor() {
    static auto setting = m_mod->getSettingValue<ccColor3B>("console-log-debug-color");
    static auto listener = listenForSettingChanges<ccColor3B>("console-log-debug-color", [](ccColor3B value) {
        setting = value;
        Console::get()->setConsoleColors();
    });
    return setting;
}

bool Config::hasConsole() {
    static bool setting = m_geode->getSettingValue<bool>("show-platform-console");
    return setting;
}

const std::filesystem::path& Config::getUniquePath() {
    return m_uniquePath;
}