#pragma once

#include "Config.hpp"
#include <Geode/loader/Log.hpp>
#include <Geode/loader/Types.hpp>
#include <filesystem>
#include <minwindef.h>
#include <processthreadsapi.h>
#include <string>
#include <asp/time/SystemTime.hpp>

namespace sobriety::utils {

    static auto convertTime(auto timePoint) {
        auto timeEpoch = std::chrono::system_clock::to_time_t(timePoint);
        return asp::localtime(timeEpoch);
    }

    static geode::Severity fromString(std::string_view severity) {
        if (severity == "debug") return geode::Severity::Debug;
        if (severity == "info") return geode::Severity::Info;
        if (severity == "warning") return geode::Severity::Warning;
        if (severity == "error") return geode::Severity::Error;
        return geode::Severity::Info;
    }

    static void createTempDir() {
        auto path = Config::get()->getUniquePath();
        if (!std::filesystem::exists(path)) {
            auto tmpDirRes = geode::utils::file::createDirectoryAll(path);
            if (!tmpDirRes) return geode::log::error("Failed to create {} directory", path);
        }
    }

    static void runCommand(const std::string& cmd) {
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

    static bool isWine() {
        static bool wine = [] -> bool {
            HMODULE hModule = GetModuleHandleA("ntdll.dll");
            if (!hModule) return false;
            FARPROC func = GetProcAddress(hModule, "wine_get_version");
            if (!func) return false;
            return true;
        }();
        
        return wine;
    }

    /*
        So this originally returned std::filesystem::path, but wine hijacks that and will turn my converted patch *BACK* into
        the path I passed in like some nerd, so I build it as a string instead.
    */
    static std::string wineToLinuxPath(const std::filesystem::path& winPath) {
        std::string s = geode::utils::string::pathToString(winPath);

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
}