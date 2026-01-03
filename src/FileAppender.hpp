#pragma once

#include <fstream>
#include <string>
#include <mutex>

class FileAppender {
public:
    FileAppender(const std::string& path) {
        std::lock_guard lock(m_mtx);
        // Open file in append mode, create if missing
        m_ofs.open(path, std::ios::out | std::ios::app);
    }

    ~FileAppender() {
        std::lock_guard lock(m_mtx);
        if (m_ofs.is_open()) {
            m_ofs.flush();
            m_ofs.close();
        }
    }

    void append(const std::string& data) {
        std::lock_guard lock(m_mtx);
        if (m_ofs.is_open()) {
            m_ofs << data;
            m_ofs.flush();
        }
    }

private:
    std::ofstream m_ofs;
    std::mutex m_mtx;
};