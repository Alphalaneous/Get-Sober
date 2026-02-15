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
    void setupEvents();
    void setupScript();
    void setupLogFile();
    void setupHeartbeat();
    void setConsoleColors();
    std::string buildLog(const Log& log);
    std::shared_ptr<FileAppender> getLogAppender();
    LPTOP_LEVEL_EXCEPTION_FILTER getOriginalUEF();

private:
    bool m_hearbeatActive;
    LPTOP_LEVEL_EXCEPTION_FILTER m_originalUEF;
    std::shared_ptr<FileAppender> m_logAppender;
};