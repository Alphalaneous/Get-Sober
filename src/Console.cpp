#include <Geode/Geode.hpp>
#include "Console.hpp"
#include "FileAppender.hpp"
#include "Utils.hpp"
#include "Config.hpp"
#include "FileWatcher.hpp"

using namespace geode::prelude;

Console* Console::get() {
    static Console instance;
    return &instance;
}

static LONG WINAPI exceptionHandler(LPEXCEPTION_POINTERS info) {
    auto exitPath = Config::get()->getUniquePath() / "console.exit";
    auto res = utils::file::writeString(exitPath, "");
    if (!res) log::error("Failed to create console exit file");

    auto originalUEF = Console::get()->getOriginalUEF();

    return originalUEF ? originalUEF(info) : EXCEPTION_CONTINUE_SEARCH;
}

void Console::setup() {
    sobriety::utils::createTempDir();
    if (Config::get()->hasConsole()) {
        auto watcher = FileWatcher::getForDirectory(Config::get()->getUniquePath());
        watcher->watch("console.heartbeat", [this] {
            setupHeartbeat();
        });

        setupLogFile();
        setupScript();
        setupEvents();

        FreeConsole();
        sobriety::utils::runCommand(fmt::format("{}/openConsole.exe {} {} {} {}", Config::get()->getUniquePath(), 
            Config::get()->getUniquePath(), 
            Config::get()->getFontSize(), 
            "#" + cc3bToHexString(Config::get()->getConsoleForegroundColor()), 
            "#" + cc3bToHexString(Config::get()->getConsoleBackgroundColor())
        ));

        m_originalUEF = SetUnhandledExceptionFilter(exceptionHandler);
    }
}

LPTOP_LEVEL_EXCEPTION_FILTER Console::getOriginalUEF() {
    return m_originalUEF;
}

void Console::setConsoleColors() {

    auto appender = Console::get()->getLogAppender();
    if (appender) {
        appender->append(fmt::format("\033]10;#{}\007", cc3bToHexString(Config::get()->getConsoleForegroundColor())));
        appender->append(fmt::format("\033]11;#{}\007", cc3bToHexString(Config::get()->getConsoleBackgroundColor())));

        appender->append(fmt::format("\033]4;33;#{}\007", cc3bToHexString(Config::get()->getLogInfoColor())));
        appender->append(fmt::format("\033]4;229;#{}\007", cc3bToHexString(Config::get()->getLogWarnColor())));
        appender->append(fmt::format("\033]4;9;#{}\007", cc3bToHexString(Config::get()->getLogErrorColor())));
        appender->append(fmt::format("\033]4;243;#{}\007", cc3bToHexString(Config::get()->getLogDebugColor())));
    
        appender->append("\033[A\033[B"); // forces a refresh
    }
}

void Console::setupEvents() {
    log::LogEvent().listen([] (log::BorrowedLog const& log) {
        if (log.m_mod) {
            if (!log.m_mod->isLoggingEnabled()) return;
            if (log.m_severity < log.m_mod->getLogLevel()) return;
        }
        if (log.m_severity < Config::get()->getConsoleLogLevel()) return;

        StringBuffer<> buffer;
        log.formatTo(buffer, Config::get()->shouldLogMillisconds());

        int color = 0;
        switch (log.m_severity) {
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

        size_t colorEnd = buffer.view().find_first_of('[') - 1;

        auto str = fmt::format("\033[38;5;{}m{}\033[0m{}\n", color, buffer.view().substr(0, colorEnd), buffer.view().substr(colorEnd));

        auto appender = Console::get()->getLogAppender();
        if (appender) appender->append(str);
    }).leak();
}

void Console::setupLogFile() {
    auto path = Config::get()->getUniquePath() / "console.ansi";;
    auto res = utils::file::writeString(path, "");
    if (!res) return log::error("Failed to create console ansi file");

    m_logAppender = std::make_shared<FileAppender>(path);
}

void Console::setupScript() {
    static std::string script = 
R"script(#!/bin/bash

UNIQUE_PATH="${1}"
FONT_SIZE="${2:-10}"
FG_COLOR="${3:-#ffffff}"
BG_COLOR="${4:-#000000}"

CONSOLE_FILE="$UNIQUE_PATH/console.ansi"
HEARTBEAT_FILE="$UNIQUE_PATH/console.heartbeat"
EXIT_FILE="$UNIQUE_PATH/console.exit"

/usr/bin/xterm \
  -fa "Monospace" \
  -bg "$BG_COLOR" \
  -fg "$FG_COLOR" \
  -T "Geometry Dash" \
  -fs "$FONT_SIZE" \
  -xrm "XTerm*VT100.Translations: #override Ctrl Shift <Key>C: copy-selection(CLIPBOARD)" \
  -e tail -F "$CONSOLE_FILE" &

TERM_PID=$!

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

    auto path = Config::get()->getUniquePath() / "openConsole.exe";
    auto res = utils::file::writeString(path, script);
    if (!res) return log::error("Failed to create openConsole script");
}

void Console::setupHeartbeat() {
    if (!m_hearbeatActive) {
        setConsoleColors();

        std::thread([] {
            auto heartbeatPath = Config::get()->getUniquePath() / "console.heartbeat";
            while (true) {
                auto strRes = utils::file::readString(heartbeatPath);
                if (!strRes) {
                    continue;
                } 

                auto str = strRes.unwrap();
                utils::string::trimIP(str);

                auto millisRes = numFromString<long long>(str);

                if (!millisRes) {
                    continue;
                }

                auto millis = millisRes.unwrap();

                auto now = std::chrono::system_clock::now();
                auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()
                ).count();

                if (nowMs - millis > Config::get()->getHeartbeatThreshold()) {
                    queueInMainThread([] {
                        utils::game::exit(false);
                    });
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }).detach();
        m_hearbeatActive = true;
    }
}

std::shared_ptr<FileAppender> Console::getLogAppender() {
    return m_logAppender;
}