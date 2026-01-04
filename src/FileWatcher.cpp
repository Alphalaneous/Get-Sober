#include <Geode/Geode.hpp>
#include "FileWatcher.hpp"
#include "Scheduler.hpp"

using namespace geode::prelude;

std::unordered_map<std::filesystem::path, std::shared_ptr<FileWatcher>> FileWatcher::s_watchers;

FileWatcher* FileWatcher::getForDirectory(const std::filesystem::path& directory) {
    auto iter = s_watchers.find(directory);

    if (iter == s_watchers.end()) {
        auto watcher = std::make_shared<FileWatcher>(directory);
        s_watchers[directory] = watcher;
        return watcher.get();
    }
    else {
        return iter->second.get();
    }

    return nullptr;
}

void FileWatcher::removeDirectory(const std::filesystem::path& directory) {
    s_watchers.erase(directory);
}

void FileWatcher::watch(const std::string& name, std::function<void()>&& method) {
    m_filesToWatch[name] = std::move(method);
}

FileWatcher::FileWatcher(const std::filesystem::path& directory) {
    m_directory = directory;
    m_id = fmt::format("{}-schedule", utils::string::pathToString(directory));

    m_handle = CreateFileW(
        directory.c_str(),
        GENERIC_READ | FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (m_handle == INVALID_HANDLE_VALUE) {
        geode::log::error("Failed to open directory for watching: {}", GetLastError());
        return;
    }

    std::thread([this] {
        while (true) {
            if (!ReadDirectoryChangesW(
                m_handle,
                m_buffer,
                sizeof(m_buffer),
                TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_ATTRIBUTES |
                FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_CREATION,
                &m_bytesReturned,
                nullptr,
                nullptr
            )) {
                log::error("Failed to read directory changes: {}", GetLastError());
                return;
            }

            auto change = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(m_buffer);
            do {
                std::wstring wname(change->FileName, change->FileNameLength / sizeof(WCHAR));
                std::string name = utils::string::wideToUtf8(wname);

                queueInMainThread([name, this] {
                    for (const auto& [k, v] : m_filesToWatch) {
                        if (name == k) {
                            if (v) v();
                        }
                    }
                });
                
                change = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    reinterpret_cast<char*>(change) + change->NextEntryOffset
                );
            } while (change->NextEntryOffset != 0);
        }
    }).detach();
}

FileWatcher::~FileWatcher() {
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
    }
    Scheduler::get()->unschedule(m_id);
}