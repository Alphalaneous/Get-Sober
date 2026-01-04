#include <Geode/Geode.hpp>
#include <Geode/modify/CCTouchDispatcher.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/CCMouseDispatcher.hpp>

#include "FileExplorer.hpp"
#include "FileWatcher.hpp"
#include "Utils.hpp"
#include "Config.hpp"

using namespace geode::prelude;

FileExplorer* FileExplorer::get() {
    static FileExplorer instance;
    return &instance;
}

void FileExplorer::setup() {
    sobriety::utils::createTempDir();

    auto watcher = FileWatcher::getForDirectory("/tmp/GeometryDash");
    watcher->watch("selectedFile.txt", [this] {
        notifySelectedFileChange();
    });
    setupScript();
    setupHooks();
}

bool file_openFolder_h(std::filesystem::path const& path) {
    if (std::filesystem::is_directory(path)) {
        FileExplorer::get()->openFile(sobriety::utils::wineToLinuxPath(path), PickMode::BrowseFiles, {});
        return true;
    }
    return false;
}

Task<Result<std::filesystem::path>> file_pick_h(utils::file::PickMode mode, const utils::file::FilePickOptions& options) {
    using RetTask = Task<Result<std::filesystem::path>>;

    if (FileExplorer::get()->isPickerActive()) return RetTask::immediate(Err("File picker is already open"));

    auto state = std::make_shared<PickerState>();
    FileExplorer::get()->setState(state);
    FileExplorer::get()->setPickerActive(true);
    
    auto defaultPath = sobriety::utils::wineToLinuxPath(options.defaultPath.value_or(dirs::getGameDir()));

    FileExplorer::get()->openFile(
        defaultPath,
        static_cast<PickMode>(mode),
        FileExplorer::get()->generateExtensionStrings(options.filters)
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

    if (FileExplorer::get()->isPickerActive()) return RetTask::immediate(Err("File picker is already open"));

    auto state = std::make_shared<PickerState>();
    FileExplorer::get()->setState(state);
    FileExplorer::get()->setPickerActive(true);

    auto defaultPath = sobriety::utils::wineToLinuxPath(options.defaultPath.value_or(dirs::getGameDir()));

    FileExplorer::get()->openFile(
        defaultPath,
        PickMode::OpenMultipleFiles,
        FileExplorer::get()->generateExtensionStrings(options.filters)
    );

    return RetTask::runWithCallback(
        [state](auto result, auto, auto cancelled) {
            state->filesCallback = result;
            state->cancelledCallback = cancelled;
        }
    );
}

/*
    I can't lie, half of this script is AI assisted, it's so tedious...
    It works and grabs the right *visible* default. Running GD through steam does block access to some files,
    meaning that it likely wont always grab the right default and falls back to GTK.
*/
void FileExplorer::setupScript() {
    static std::string script = 
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

/*
    I only tested this on linux mint using cinnamon, as it's what I personally daily drive.
    If it doesn't work on other distros/desktop environments, sorry.
    (from what it looks like dark mode probably already worked on plasma without this?)
*/
DARK_MODE="$1"
shift
[ -z "$DARK_MODE" ] && DARK_MODE="light"

FILTERS=("$@")

# Set dark mode environment variables if enabled
if [ "$DARK_MODE" = "dark" ]; then
    # Non-invasive dark-mode hints for GTK/Qt dialogs
    export GTK_SETTINGS_VARIANT=dark
    export QT_STYLE_OVERRIDE=Adwaita-Dark
    export GTK_THEME="Adwaita:dark"

    # Try to select an installed dark GTK theme
    THEME=""
    if command -v gsettings >/dev/null 2>&1; then
        THEME=$(gsettings get org.cinnamon.desktop.interface gtk-theme 2>/dev/null | tr -d "'")
    fi

    # Prefer a "*Dark*" variant of the current theme
    if [ -n "$THEME" ]; then
        shopt -s nullglob
        for root in "/usr/share/themes" "$HOME/.themes" "$HOME/.local/share/themes"; do
            for path in "$root/${THEME}"* "$root/${THEME%-Dark}"* "$root/${THEME%-dark}"*; do
                name=$(basename "$path")
                if [ -d "$path" ] && [[ "$name" =~ [Dd]ark ]]; then
                    export GTK_THEME="$name"
                    break 2
                fi
            done
        done
        shopt -u nullglob
    fi

    # If we still didn't find a dark theme, try common theme names
    if [ -z "${GTK_THEME:-}" ] || [ "$GTK_THEME" = "Adwaita:dark" ]; then
        for fallback in "Mint-Y-Dark" "Mint-X-Dark" "Mint-L-Dark" "Adwaita:dark" "Adwaita-dark"; do
            base="${fallback%%:*}"
            if [ -d "/usr/share/themes/$base" ] || [ -d "$HOME/.themes/$base" ] || [ -d "$HOME/.local/share/themes/$base" ]; then
                export GTK_THEME="$fallback"
                break
            fi
        done
    fi
fi

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

    /* 
        Normally, writing a bash script to a file and running it cannot be done via wine, as the file needs
        to be marked as executable. But, wine wants to run exes, so simply making the script have an "exe" file
        extension will allow it to be ran without being set as executable. This means we can bypass the limitation 
        and properly bridge between some linux based script and wine.
    */

    auto path = std::filesystem::path("/tmp/GeometryDash/openFile.exe");
    auto res = utils::file::writeString(path, script);
    if (!res) return log::error("Failed to create openFile script");
}

void FileExplorer::setupHooks() {
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
}

void FileExplorer::openFile(const std::string& startPath, PickMode pickMode, const std::vector<std::string>& filters) {
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

    // IMPORTANT: The bash script expects DARK_MODE before FILTERS.
    command += " \"";
    command += Config::get()->isDarkModeEnabled() ? "dark" : "light";
    command += "\"";

    for (const auto& param : filters) {
        command += " \"";
        command += param;
        command += "\"";
    }

    sobriety::utils::runCommand(command.c_str());
}

bool FileExplorer::isPickerActive() {
    return m_pickerActive;
}

void FileExplorer::setPickerActive(bool active) {
    m_pickerActive = active;
}

void FileExplorer::setState(std::shared_ptr<PickerState> state) {
    m_state = state;
}

std::shared_ptr<PickerState> FileExplorer::getState() {
    return m_state;
}

std::vector<std::string> FileExplorer::generateExtensionStrings(const std::vector<utils::file::FilePickOptions::Filter>& filters) {
    std::vector<std::string> strings;

    auto& _filters = const_cast<std::vector<utils::file::FilePickOptions::Filter>&>(filters);

    _filters.push_back({"All Files", {"*.*"}});

    for (const auto& filter : _filters) {
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

void FileExplorer::notifySelectedFileChange() {
    const auto path = std::filesystem::path("/tmp/GeometryDash/selectedFile.txt");

    auto strRes = utils::file::readString(path);
    if (!strRes) return;

    std::string str = strRes.unwrap();
    utils::string::trimIP(str);

    if (str.empty()) return;

    if (!m_state) return;

    if (str == "-1") {
        if (m_state->cancelledCallback) m_state->cancelledCallback();
    }
    else if (m_state->fileCallback) {
        m_state->fileCallback(Ok(std::filesystem::path(str)));
    }
    else if (m_state->filesCallback) {
        auto parts = utils::string::split(str, "\n");

        std::vector<std::filesystem::path> paths;
        paths.reserve(parts.size());

        for (auto& p : parts) {
            if (!p.empty()) paths.emplace_back(p);
        }

        m_state->filesCallback(Ok(std::move(paths)));
    }

    m_pickerActive = false;
}

/*
    These block inputs when file picker is active to mimic windows behavior. We do not want to actually
    block the main thread.
*/

class $modify(CCTouchDispatcher) {
    void touches(CCSet *pTouches, CCEvent *pEvent, unsigned int uIndex) {
        if (FileExplorer::get()->isPickerActive()) {
            if (uIndex == 0) MessageBeep(MB_ICONWARNING);
            return;
        }
        CCTouchDispatcher::touches(pTouches, pEvent, uIndex);
    }
};

class $modify(CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat) {
        if (FileExplorer::get()->isPickerActive()) {
            if (isKeyDown && !isKeyRepeat) MessageBeep(MB_ICONWARNING);
            return false;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, isKeyDown, isKeyRepeat);
    }
};

class $modify(CCMouseDispatcher) {
    bool dispatchScrollMSG(float x, float y) {
        if (FileExplorer::get()->isPickerActive()) {
            return false;
        }
        return CCMouseDispatcher::dispatchScrollMSG(x, y);
    }
};