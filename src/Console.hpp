#pragma once

#include <Geode/loader/Mod.hpp>
#include <memory>
#include "FileAppender.hpp"

struct Log {
    geode::Mod* mod;
    geode::Severity severity = geode::Severity::Info;
    std::string message;
    std::string threadName;
    std::tm time;
    long long milliseconds;
    bool newLine;
    int offset;
};

class Console {
public:
    static Console* get();

    void setup();
    void setupHooks();
    void setupScript();
    void setupLogFile();
    void setupHeartbeat();
    std::string buildLog(const Log& log);
    std::shared_ptr<FileAppender> getLogAppender();

private:
    bool m_hearbeatActive;
    std::shared_ptr<FileAppender> m_logAppender;
};