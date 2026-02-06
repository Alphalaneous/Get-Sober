#pragma once

#include "WaitingPopup.hpp"
#include <Geode/Result.hpp>
#include <Geode/utils/file.hpp>
#include <vector>

enum class PickMode {
    OpenFile,
    SaveFile,
    OpenFolder,
    OpenMultipleFiles,
    BrowseFiles
};

class WaitingPopup;

class FileExplorer {
public:
    static FileExplorer* get();

    void setup();
    void setupHooks();
    void setupScript();
    void openFile(const std::string& startPath, PickMode pickMode, const std::vector<std::string>& filters);
    bool isPickerActive();
    void setPickerActive(bool active);
    void notifySelectedFileChange();
    std::optional<std::filesystem::path> getPath();
    std::optional<std::vector<std::filesystem::path>> getPaths();

    std::vector<std::string> generateExtensionStrings(std::vector<geode::utils::file::FilePickOptions::Filter> filters);

    arc::Notify m_notify;
    std::optional<std::filesystem::path> m_path;
    std::optional<std::vector<std::filesystem::path>> m_paths;
    WaitingPopup* m_waitingPopup;
private:
    bool m_pickerActive = false;
};
