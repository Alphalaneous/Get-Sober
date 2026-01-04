#pragma once

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

struct PickerState {
    std::function<void(geode::Result<std::filesystem::path>)> fileCallback;
    std::function<void(geode::Result<std::vector<std::filesystem::path>>)> filesCallback;
    std::function<void()> cancelledCallback;
};

class FileExplorer {
public:
    static FileExplorer* get();

    void setup();
    void setupHooks();
    void setupScript();
    void openFile(const std::string& startPath, PickMode pickMode, const std::vector<std::string>& filters);
    bool isPickerActive();
    void setPickerActive(bool active);
    void setState(std::shared_ptr<PickerState> state);
    void notifySelectedFileChange();

    std::shared_ptr<PickerState> getState();
    std::vector<std::string> generateExtensionStrings(const std::vector<geode::utils::file::FilePickOptions::Filter>& filters);

private:
    std::shared_ptr<PickerState> m_state;
    bool m_pickerActive = false;
};