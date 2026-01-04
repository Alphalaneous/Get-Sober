#include <Geode/Geode.hpp>
#include "Console.hpp"
#include "FileAppender.hpp"
#include "Scheduler.hpp"
#include "Utils.hpp"
#include "Config.hpp"
#include "FileWatcher.hpp"

using namespace geode::prelude;

Console* Console::get() {
    static Console instance;
    return &instance;
}

LONG WINAPI VectoredHandler(EXCEPTION_POINTERS* info) {
    if (info->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) {
        auto exitPath = std::filesystem::path("/tmp/GeometryDash/console.exit");
        auto exitRes = utils::file::writeString(exitPath, "");
        if (!exitRes) log::error("Failed to create console exit file");
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void Console::setup() {
    sobriety::utils::createTempDir();
    if (Config::get()->hasConsole()) {
        /*
            this exit thing is what the console listens to to see if the game has closed, without it, it wont close alongside the game
            brecause it isn't actually tied to it, but rather a separate process.
        */
        auto exitPath = std::filesystem::path("/tmp/GeometryDash/console.exit");
        std::filesystem::remove(exitPath);

        auto watcher = FileWatcher::getForDirectory("/tmp/GeometryDash");
        watcher->watch("console.heartbeat", [this] {
            setupHeartbeat();
        });

        setupLogFile();
        setupScript();
        setupHooks();

        FreeConsole();
        sobriety::utils::runCommand(fmt::format("/tmp/GeometryDash/openConsole.exe {} {}", Config::get()->getFontSize(), Config::get()->isDarkModeEnabled() ? "dark" : "light").c_str());

        /*
            if this fails, the console wont exit, it shouldn't fail, but if it does, it isn't a big deal, as the user can close it themselves still
            imo a skill issue if writing to /tmp fails for any of these.
        */
        std::atexit([] {
            auto exitPath = std::filesystem::path("/tmp/GeometryDash/console.exit");
            auto exitRes = utils::file::writeString(exitPath, "");
            if (!exitRes) return log::error("Failed to create console exit file");
        });

        AddVectoredExceptionHandler(0, VectoredHandler);
    }
}

/*
    I manually remake the logs since I don't have access to internal geode methods or something smh, these don't 
    have nesting support yet, I really could care less adding that back, but probably will at some point.
*/
void vlogImpl_h(Severity severity, Mod* mod, fmt::string_view format, fmt::format_args args) {
    log::vlogImpl(severity, mod, format, args);

    if (!mod->isLoggingEnabled()) return;
    if (severity < mod->getLogLevel()) return;
    if (severity < Config::get()->getConsoleLogLevel()) return;

    auto time = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;

    Log log {
        mod,
        severity,
        fmt::vformat(format, args),
        thread::getName(),
        sobriety::utils::convertTime(time),
        ms.count()
    };

    int color = 0;
    switch (severity) {
        case Severity::Debug:
            color = 243;
            break;
        case Severity::Info:
            color = 33;
            break;
        case Severity::Warning:
            color = 229;
            break;
        case Severity::Error:
            color = 9;
            break;
        default:
            color = 7;
            break;
    }
    
    std::string_view sv{Console::get()->buildLog(log)};

    size_t colorEnd = sv.find_first_of('[') - 1;

    auto str = fmt::format("\x1b[38;5;{}m{}\x1b[0m{}\n", color, sv.substr(0, colorEnd), sv.substr(colorEnd));
    
    auto appender = Console::get()->getLogAppender();
    if (appender) appender->append(str);
}

void Console::setupHooks() {
    (void) Mod::get()->hook(
        reinterpret_cast<void*>(addresser::getNonVirtual(&log::vlogImpl)),
        &vlogImpl_h,
        "log::vlogImpl"
    );
}

void Console::setupLogFile() {
    auto path = std::filesystem::path("/tmp/GeometryDash/console.ansi");
    auto res = utils::file::writeString(path, "");
    if (!res) return log::error("Failed to create console ansi file");

    m_logAppender = std::make_shared<FileAppender>("/tmp/GeometryDash/console.ansi");
}

void Console::setupScript() {
    static std::string script = 
R"script(#!/bin/bash

FONT_SIZE="${1:-10}"
DARK_MODE="${2:-light}"

BG_COLOR="black"
FG_COLOR="white"

if [ "$DARK_MODE" = "dark" ]; then
    BG_COLOR="#1e1e1e"
    FG_COLOR="#e0e0e0"
else
    BG_COLOR="white"
    FG_COLOR="black"
fi

xterm \
  -fa Monospace \
  -bg "$BG_COLOR" -fg "$FG_COLOR" \
  -T "Geometry Dash" \
  -fs "$FONT_SIZE" \
  -xrm "XTerm*VT100.Translations: #override Ctrl Shift <Key>C: copy-selection(CLIPBOARD)" \
  -e tail -F /tmp/GeometryDash/console.ansi &

TERM_PID=$!

HEARTBEAT_FILE="/tmp/GeometryDash/console.heartbeat"
EXIT_FILE="/tmp/GeometryDash/console.exit"

while [ ! -f "$EXIT_FILE" ]; do
    if ! kill -0 "$TERM_PID" 2>/dev/null; then
        break
    fi

    date +%s%3N > "$HEARTBEAT_FILE"
    sleep 0.016667
done

kill "$TERM_PID" 2>/dev/null
rm -f "$EXIT_FILE"

)script";

    auto path = std::filesystem::path("/tmp/GeometryDash/openConsole.exe");
    auto res = utils::file::writeString(path, script);
    if (!res) return log::error("Failed to create openConsole script");
}

void Console::setupHeartbeat() {
    if (!m_hearbeatActive) {
        Scheduler::get()->schedule("console-heartbeat", [] {
            std::thread([] {
                auto heartbeatPath = std::filesystem::path("/tmp/GeometryDash/console.heartbeat");
                auto strRes = utils::file::readString(heartbeatPath);
                if (!strRes) {
                    return;
                } 

                auto str = strRes.unwrap();
                utils::string::trimIP(str);

                auto millisRes = numFromString<long long>(str);

                if (!millisRes) {
                    return;
                }

                auto millis = millisRes.unwrap();

                auto now = std::chrono::system_clock::now();
                auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()
                ).count();

                if (nowMs - millis > Config::get()->getHeartbeatThreshold()) {
                    queueInMainThread([] {
                        utils::game::exit(false);
                        Scheduler::get()->unschedule("console-heartbeat");
                    });
                }
            }).detach();
        }, std::chrono::milliseconds(50));
        m_hearbeatActive = true;
    }
}

std::string Console::buildLog(const Log& log) {
    std::string ret;

    if (Config::get()->shouldLogMillisconds()) {
        ret = fmt::format("{:%H:%M:%S}.{:03}", log.time, log.milliseconds);
    }
    else {
        ret = fmt::format("{:%H:%M:%S}", log.time);
    }

    switch (log.severity.m_value) {
        case Severity::Debug:
            ret += " DEBUG";
            break;
        case Severity::Info:
            ret += " INFO ";
            break;
        case Severity::Warning:
            ret += " WARN ";
            break;
        case Severity::Error:
            ret += " ERROR";
            break;
        default:
            ret += " ?????";
            break;
    }

    if (log.threadName.empty())
        ret += fmt::format(" [{}]: ", log.mod->getName());
    else
        ret += fmt::format(" [{}] [{}]: ", log.threadName, log.mod->getName());

    ret += log.message;

    return ret;
}

std::shared_ptr<FileAppender> Console::getLogAppender() {
    return m_logAppender;
}