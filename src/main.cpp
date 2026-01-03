#include <Geode/Geode.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCMouseDispatcher.hpp>
#include "FileAppender.hpp"

using namespace geode::prelude;

static Mod* s_geode = Loader::get()->getLoadedMod("geode.loader");
static bool s_logMilliseconds = false;

void systemNoConsole(const std::string& cmd) {
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};

    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    if (!CreateProcessA(
            nullptr,
            const_cast<char*>(cmd.c_str()),
            nullptr, nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi
        )) {
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

auto convertTime(auto timePoint) {
    auto timeEpoch = std::chrono::system_clock::to_time_t(timePoint);
    return fmt::localtime(timeEpoch);
}

Severity fromString(std::string_view severity) {
    if (severity == "debug") return Severity::Debug;
    if (severity == "info") return Severity::Info;
    if (severity == "warning") return Severity::Warning;
    if (severity == "error") return Severity::Error;
    return Severity::Info;
}

static Severity getConsoleLogLevel() {
    static Severity level = fromString(s_geode->getSettingValue<std::string>("console-log-level"));
    listenForSettingChanges("console-log-level", [](std::string value) {
        level = fromString(value);
    }, s_geode);

    return level;
}

struct Log {
    Mod* mod;
    Severity severity = Severity::Info;
    std::string message;
    std::string threadName;
    std::tm time;
    long long milliseconds;
    bool newLine;
    int offset;
};

std::string buildLog(const Log& log) {
    std::string ret;

    if (s_logMilliseconds) {
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

static FileAppender s_logFile("/tmp/GeometryDash/console.ansi");

void vlogImpl_h(Severity severity, Mod* mod, fmt::string_view format, fmt::format_args args) {
    log::vlogImpl(severity, mod, format, args);

    if (!mod->isLoggingEnabled()) return;
    if (severity < mod->getLogLevel()) return;
    if (severity < getConsoleLogLevel()) return;

    auto time = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;

    Log log {
        mod,
        severity,
        fmt::vformat(format, args),
        thread::getName(),
        convertTime(time),
        ms.count()
    };

    int color = 0;
    int color2 = -1;
    switch (severity) {
        case Severity::Debug:
            color = 243;
            color2 = 250;
            break;
        case Severity::Info:
            color = 33;
            color2 = 254;
            break;
        case Severity::Warning:
            color = 229;
            color2 = 230;
            break;
        case Severity::Error:
            color = 9;
            color2 = 224;
            break;
        default:
            color = 7;
            break;
    }
    
    std::string_view sv{buildLog(log)};

    size_t colorEnd = sv.find_first_of('[') - 1;

    auto str = fmt::format("\x1b[38;5;{}m{}\x1b[0m{}\n", color, sv.substr(0, colorEnd), sv.substr(colorEnd));
    
    s_logFile.append(str);
}

static std::string wineToLinuxPath(const std::filesystem::path& winPath) {
    std::string s = utils::string::pathToString(winPath);

    if (s.size() < 2 || s[1] != ':')
        return s;

    char drive = std::tolower(s[0]);
    std::string rest = s.substr(2);
    for (auto& c : rest) if (c == '\\') c = '/';

    const char* prefixEnv = std::getenv("WINEPREFIX");
    const char* homeEnv = std::getenv("HOME");

    std::string prefix;
    if (prefixEnv) {
        prefix = prefixEnv;
    } else if (homeEnv) {
        prefix = std::string(homeEnv) + "/.wine";
    } else {
        prefix = "/.wine";
    }

    std::string drivePath;

    if (drive == 'z') {
        drivePath = "/";
    } else {
        drivePath = prefix + "/drive_" + drive;
    }

    std::string fullPath = drivePath;
    size_t start = 0;
    while (start < rest.size()) {
        size_t end = rest.find('/', start);
        if (end == std::string::npos) end = rest.size();
        std::string part = rest.substr(start, end - start);
        if (!part.empty()) {
            if (fullPath.back() != '/') fullPath += "/";
            fullPath += part;
        }
        start = end + 1;
    }

    return fullPath;
}

struct PickerState {
    std::function<void(Result<std::filesystem::path>)> fileCallback;
    std::function<void(Result<std::vector<std::filesystem::path>>)> filesCallback;
    std::function<void()> cancelledCallback;
};

static std::mutex s_stateMtx;
static std::shared_ptr<PickerState> s_state;
static std::atomic_bool s_pickerActive = false;

enum class PickMode {
    OpenFile,
    SaveFile,
    OpenFolder,
    OpenMultipleFiles,
    BrowseFiles
};

static void runOpenFileScript(const std::string& startPath, PickMode pickMode, const std::vector<std::string>& filters) {
    std::string command = "/tmp/GeometryDash/openFile.exe";

    command += " \"";
    command += startPath;
    command += "\"";

    command += " \"";
    switch (pickMode) {
        case PickMode::OpenFile: {
            command += "Select a file";
            break;
        }
        case PickMode::SaveFile: {
            command += "Save...";
            break;
        }
        case PickMode::OpenFolder: {
            command += "Select a folder";
            break;
        }
        case PickMode::BrowseFiles: {
            command += "Browse";
            break;
        }
        case PickMode::OpenMultipleFiles: {
            command += "Select files";
            break;
        }
    }
    command += "\"";

    command += " \"";
    switch (pickMode) {
        case PickMode::OpenFile: {
            command += "single";
            break;
        }
        case PickMode::SaveFile: {
            command += "save";
            break;
        }
        case PickMode::OpenFolder: {
            command += "dir";
            break;
        }
        case PickMode::BrowseFiles: {
            command += "browse";
            break;
        }
        case PickMode::OpenMultipleFiles: {
            command += "multi";
            break;
        }
    }
    command += "\"";

    for (const auto& param : filters) {
        command += " \"";
        command += param;
        command += "\"";
    }

    systemNoConsole(command.c_str());
}

std::vector<std::string> generateExtensionStrings(const std::vector<utils::file::FilePickOptions::Filter>& filters) {
    std::vector<std::string> strings;
    for (const auto& filter : filters) {
        std::string extStr = utils::string::trim(filter.description);
        extStr += "|";
        for (const auto& extension : filter.files) {
            extStr += utils::string::trim(extension);
            extStr += " ";
        }
        strings.push_back(utils::string::trim(extStr));
    }
    return strings;
}

bool file_openFolder_h(std::filesystem::path const& path) {
    if (std::filesystem::is_directory(path)) {
        runOpenFileScript(wineToLinuxPath(path), PickMode::BrowseFiles, {});
        return true;
    }
    return false;
}

Task<Result<std::filesystem::path>> file_pick_h(utils::file::PickMode mode, const utils::file::FilePickOptions& options) {
    using RetTask = Task<Result<std::filesystem::path>>;

    if (s_pickerActive.exchange(true))
        return RetTask::immediate(Err("File picker is already open"));

    auto state = std::make_shared<PickerState>();
    {
        std::lock_guard lock(s_stateMtx);
        s_state = state;
    }

    auto defaultPath =
        wineToLinuxPath(options.defaultPath.value_or(dirs::getGameDir()));

    runOpenFileScript(
        defaultPath,
        static_cast<PickMode>(mode),
        generateExtensionStrings(options.filters)
    );

    return RetTask::runWithCallback(
        [state](auto result, auto, auto cancelled) {
            state->fileCallback = result;
            state->cancelledCallback = cancelled;
        }
    );
}

Task<Result<std::vector<std::filesystem::path>>> file_pickMany_h(const utils::file::FilePickOptions& options) {
    using RetTask = Task<Result<std::vector<std::filesystem::path>>>;

    if (s_pickerActive.exchange(true))
        return RetTask::immediate(Err("File picker is already open"));

    auto state = std::make_shared<PickerState>();
    {
        std::lock_guard lock(s_stateMtx);
        s_state = state;
    }

    auto defaultPath =
        wineToLinuxPath(options.defaultPath.value_or(dirs::getGameDir()));

    runOpenFileScript(
        defaultPath,
        PickMode::OpenMultipleFiles,
        generateExtensionStrings(options.filters)
    );

    return RetTask::runWithCallback(
        [state](auto result, auto, auto cancelled) {
            state->filesCallback = result;
            state->cancelledCallback = cancelled;
        }
    );
}

static void notifySelectedFileChange() {
    const auto path = std::filesystem::path("/tmp/GeometryDash/selectedFile.txt");

    auto strRes = utils::file::readString(path);
    if (!strRes) return;

    std::string str = strRes.unwrap();
    utils::string::trimIP(str);


    if (str.empty()) return;

    std::shared_ptr<PickerState> state;
    {
        std::lock_guard lock(s_stateMtx);
        state = std::move(s_state);
    }

    if (!state) return;

    if (str == "-1") {
        if (state->cancelledCallback) state->cancelledCallback();
    }
    else if (state->fileCallback) {
        state->fileCallback(Ok(std::filesystem::path(str)));
    }
    else if (state->filesCallback) {
        auto parts = utils::string::split(str, "\n");

        std::vector<std::filesystem::path> paths;
        paths.reserve(parts.size());

        for (auto& p : parts) {
            if (!p.empty()) paths.emplace_back(p);
        }

        state->filesCallback(Ok(std::move(paths)));
    }

    s_pickerActive.store(false, std::memory_order_release);
}

static int s_heartbeatThreshold;
static bool s_hasConsole = false;
static std::atomic_bool s_heartbeatActive = false;

static void heartbeatThread() {
    if (!s_heartbeatActive.exchange(true)) {
        std::thread([]() {
            while (true) {
                auto heartbeatPath = std::filesystem::path("/tmp/GeometryDash/console.heartbeat");
                auto strRes = utils::file::readString(heartbeatPath);
                if (!strRes) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                } 

                auto str = strRes.unwrap();
                utils::string::trimIP(str);

                auto millisRes = numFromString<long long>(str);

                if (!millisRes) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                }

                auto millis = millisRes.unwrap();

                auto now = std::chrono::system_clock::now();
                auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()
                ).count();

                if (nowMs - millis > s_heartbeatThreshold) {
                    utils::game::exit(false);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }).detach();
    }
}

static void watcherThread() {
    const auto path = std::filesystem::path("Z:\\tmp\\GeometryDash");

    HANDLE handle = CreateFileW(
        path.c_str(),
        GENERIC_READ | FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (handle == INVALID_HANDLE_VALUE) {
        geode::log::error("Failed to open directory for watching: {}", GetLastError());
        return;
    }

    char buffer[1024];
    DWORD bytesReturned;

    while (true) {
        if (!ReadDirectoryChangesW(
            handle,
            buffer,
            sizeof(buffer),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_ATTRIBUTES |
            FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_CREATION,
            &bytesReturned,
            nullptr,
            nullptr
        )) {
            log::error("Failed to read directory changes: {}", GetLastError());
            continue;
        }

        auto change = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
        do {
            std::wstring wname(change->FileName, change->FileNameLength / sizeof(WCHAR));
            std::string name = utils::string::wideToUtf8(wname);

            if (name == "selectedFile.txt") {
                notifySelectedFileChange();
            }
            if (name == "console.heartbeat") {
                heartbeatThread();
            }

            change = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<char*>(change) + change->NextEntryOffset
            );
        } while (change->NextEntryOffset != 0);
    }
}

static std::string s_openFileScript = 
R"script(#!/bin/bash

TMP="/tmp/GeometryDash/selectedFile.txt"
> "$TMP"

START_PATH="$1"
shift
[ -z "$START_PATH" ] && START_PATH="$HOME"
[ -f "$START_PATH" ] && START_PATH="$(dirname "$START_PATH")"

TITLE="$1"
shift
[ -z "$TITLE" ] && TITLE="Select a file"

MODE="$1"
shift
[ -z "$MODE" ] && MODE="single"

FILTERS=("$@")

PICKER=""
DE="$XDG_CURRENT_DESKTOP"
if [[ "$DE" == *KDE* ]]; then
    PICKER="kdialog"
elif [[ "$DE" == *GNOME* ]]; then
    PICKER="zenity"
fi

if ! command -v "$PICKER" >/dev/null 2>&1; then
    if command -v kdialog >/dev/null 2>&1; then
        PICKER="kdialog"
    elif command -v zenity >/dev/null 2>&1; then
        PICKER="zenity"
    elif command -v yad >/dev/null 2>&1; then
        PICKER="yad"
    else
        PICKER="xdg-open"
    fi
fi

DEFAULT_FILE=""
if [ "$MODE" = "save" ] && [ "${#FILTERS[@]}" -gt 0 ]; then
    IFS='|' read -r desc exts <<< "${FILTERS[0]}"
    FIRST_EXT=$(echo "$exts" | awk '{print $1}')
    FIRST_EXT="${FIRST_EXT#\*}"
    DEFAULT_FILE="Untitled$FIRST_EXT"
fi

launch_picker() {
    FILE=""
    STATUS=0

    case "$PICKER" in
        zenity)
            CMD=(zenity --title="$TITLE" --filename="$START_PATH/$DEFAULT_FILE")
            case "$MODE" in
                single) CMD+=(--file-selection) ;;
                multi) CMD+=(--file-selection --multiple --separator=":") ;;
                dir) CMD+=(--file-selection --directory) ;;
                save) CMD+=(--file-selection --save) ;;
                browse) xdg-open "$START_PATH"; FILE=""; STATUS=0; return ;;
                *) CMD+=(--file-selection) ;;
            esac
            for f in "${FILTERS[@]}"; do
                IFS='|' read -r desc exts <<< "$f"
                CMD+=(--file-filter="$desc | $exts")
            done
            FILE=$("${CMD[@]}")
            STATUS=$?
            ;;
        kdialog)
            FILTER_STRING=""
            for f in "${FILTERS[@]}"; do
                IFS='|' read -r desc exts <<< "$f"
                [[ -n "$FILTER_STRING" ]] && FILTER_STRING+=" | "
                FILTER_STRING+="$exts | $desc"
            done
            case "$MODE" in
                single) FILE=$(kdialog --title "$TITLE" --getopenfilename "$START_PATH" "$FILTER_STRING") ;;
                multi) FILE=$(kdialog --title "$TITLE" --getopenfilenames "$START_PATH" "$FILTER_STRING") ;;
                dir) FILE=$(kdialog --title "$TITLE" --getexistingdirectory "$START_PATH") ;;
                save) FILE=$(kdialog --title "$TITLE" --getsavefilename "$START_PATH/$DEFAULT_FILE" "$FILTER_STRING") ;;
                browse) xdg-open "$START_PATH"; FILE=""; STATUS=0; return ;;
                *) FILE=$(kdialog --title "$TITLE" --getopenfilename "$START_PATH" "$FILTER_STRING") ;;
            esac
            STATUS=$?
            ;;
        yad)
            CMD=(yad --title="$TITLE" --filename="$START_PATH/$DEFAULT_FILE")
            case "$MODE" in
                single) CMD+=(--file-selection) ;;
                multi) CMD+=(--file-selection --multiple --separator=":") ;;
                dir) CMD+=(--file-selection --directory) ;;
                save) CMD+=(--file-selection --save) ;;
                browse) xdg-open "$START_PATH"; FILE=""; STATUS=0; return ;;
                *) CMD+=(--file-selection) ;;
            esac
            for f in "${FILTERS[@]}"; do
                IFS='|' read -r desc exts <<< "$f"
                CMD+=(--file-filter="$desc | $exts")
            done
            FILE=$("${CMD[@]}")
            STATUS=$?
            ;;
        xdg-open)
            xdg-open "$START_PATH"
            FILE=""
            STATUS=0
            ;;
    esac

    if [ -n "$FILE" ]; then
        case "$PICKER" in
            zenity|yad)
                if [ "$MODE" = "multi" ]; then
                    echo "$FILE" | tr ':' '\n' > "$TMP"
                else
                    echo "$FILE" > "$TMP"
                fi
                ;;
            kdialog)
                if [ "$MODE" = "multi" ]; then
                    echo "$FILE" | sed 's/"//g' | tr ' ' '\n' > "$TMP"
                else
                    echo "$FILE" > "$TMP"
                fi
                ;;
            xdg-open) ;;
        esac
    else
        [ "$STATUS" -ne 0 ] && echo "-1" > "$TMP"
    fi
}

launch_picker &

)script";

static std::string s_openConsoleScript = 
R"script(#!/bin/bash

FONT_SIZE="${1:-10}"

xterm \
  -fa Monospace \
  -bg black -fg white \
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

$execute {

    HMODULE hModule = GetModuleHandleA("ntdll.dll");
    if (hModule) {
        FARPROC func = GetProcAddress(hModule, "wine_get_version");
        if (func) {
            /* 
                Normally, writing a bash script to a file and running it cannot be done via wine, as the file needs
                to be marked as executable. But, wine wants to run exes, so simply making the script have an "exe" file
                extension will allow it to be ran without being set as executable. This means we can bypass the limitation 
                and properly bridge between some linux based script and wine.
            */
            (void) utils::file::createDirectory("/tmp/GeometryDash/");
            auto filesScriptPath = std::filesystem::path("/tmp/GeometryDash/openFile.exe");
            (void) utils::file::writeString(filesScriptPath, s_openFileScript);

            auto consoleScriptPath = std::filesystem::path("/tmp/GeometryDash/openConsole.exe");
            (void) utils::file::writeString(consoleScriptPath, s_openConsoleScript);

            auto consolePath = std::filesystem::path("/tmp/GeometryDash/console.ansi");
            (void) utils::file::writeString(consolePath, "");

            std::thread(watcherThread).detach();

            (void) Mod::get()->hook(
                reinterpret_cast<void*>(addresser::getNonVirtual(&utils::file::pick)),
                &file_pick_h,
                "utils::file::pick"
            );
            (void) Mod::get()->hook(
                reinterpret_cast<void*>(addresser::getNonVirtual(&utils::file::pickMany)),
                &file_pickMany_h,
                "utils::file::pickMany"
            );
            (void) Mod::get()->hook(
                reinterpret_cast<void*>(addresser::getNonVirtual(&utils::file::openFolder)),
                &file_openFolder_h,
                "utils::file::openFolder"
            );
            (void) Mod::get()->hook(
                reinterpret_cast<void*>(addresser::getNonVirtual(&log::vlogImpl)),
                &vlogImpl_h,
                "log::vlogImpl"
            );

            auto exitPath = std::filesystem::path("/tmp/GeometryDash/console.exit");
            std::filesystem::remove(exitPath);

            if (s_geode->getSettingValue<bool>("show-platform-console")) {
                s_heartbeatThreshold = Mod::get()->getSettingValue<int>("console-heartbeat-threshold");
                listenForSettingChanges<int>("console-heartbeat-threshold", [](int setting) {
                    s_heartbeatThreshold = setting;
                });

                s_logMilliseconds = s_geode->getSettingValue<bool>("log-milliseconds");
                listenForSettingChanges<bool>("log-milliseconds", [](bool setting) {
                    s_logMilliseconds = setting;
                }, s_geode);

                s_hasConsole = true;
                FreeConsole();

                int fontSize = Mod::get()->getSettingValue<int>("console-font-size");
                systemNoConsole(fmt::format("/tmp/GeometryDash/openConsole.exe {}", fontSize).c_str());

                std::atexit([] {
                    auto exitPath = std::filesystem::path("/tmp/GeometryDash/console.exit");
                    (void) utils::file::writeString(exitPath, "");
                });
            }
            
        }
        else {
            (void) Mod::get()->uninstall();
        }
    }
}   

/*
    These block inputs when file picker is active to mimic windows behavior. We do not want to actually
    block the main thread.
*/

class $modify(CCTouchDispatcher) {
    void touches(CCSet *pTouches, CCEvent *pEvent, unsigned int uIndex) {
        if (s_pickerActive) {
            if (uIndex == 0) MessageBeep(MB_ICONWARNING);
            return;
        }
        CCTouchDispatcher::touches(pTouches, pEvent, uIndex);
    }
};

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat) {
        if (s_pickerActive) {
            if (isKeyDown && !isKeyRepeat) MessageBeep(MB_ICONWARNING);
            return false;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat);
    }
};

class $modify(CCMouseDispatcher) {
    bool dispatchScrollMSG(float x, float y) {
        if (s_pickerActive) {
            return false;
        }
        return CCMouseDispatcher::dispatchScrollMSG(x, y);
    }
};