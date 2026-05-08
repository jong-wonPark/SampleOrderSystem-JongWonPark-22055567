#pragma once

#include <atomic>
#include <filesystem>
#include <string>

// 테스트 공통 헬퍼 — 각 테스트마다 격리된 임시 디렉토리 제공

class TempDir {
    std::filesystem::path path_;
    static inline std::atomic<int> counter_{ 0 };
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("sos_test_" + std::to_string(++counter_));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    std::filesystem::path file(const std::string& name) const { return path_ / name; }
};
