#pragma once

#include <unordered_map>

class FileWatcher {
public:
    FileWatcher(const std::filesystem::path& directory);
    ~FileWatcher();

    static FileWatcher* getForDirectory(const std::filesystem::path& directory);
    static void removeDirectory(const std::filesystem::path& directory);

    void watch(const std::string& name, std::function<void()>&& method);

private:
    std::string m_id;
    std::filesystem::path m_directory;
    std::unordered_map<std::string, std::function<void()>> m_filesToWatch;

    HANDLE m_handle;
    char m_buffer[1024];
    DWORD m_bytesReturned;

    static std::unordered_map<std::filesystem::path, std::shared_ptr<FileWatcher>> s_watchers;
};