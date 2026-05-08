#include <filesystem>
#include <iostream>
#include <windows.h>

#include "DataMonitor.h"

namespace fs = std::filesystem;

static fs::path get_exe_dir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    return fs::path(buf).parent_path();
}

int main(int argc, char* argv[]) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // ANSI VT 색상 코드 활성화 (Windows 10+)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // 기본 데이터 경로: 솔루션 루트 data/ (exe 3단계 위)
    fs::path dataDir = (argc > 1)
        ? fs::path(argv[1])
        : get_exe_dir() / ".." / ".." / ".." / "data";
    dataDir = fs::weakly_canonical(dataDir);

    if (!fs::exists(dataDir)) {
        std::cerr << "[오류] 데이터 경로가 존재하지 않습니다: " << dataDir << "\n"
                  << "사용법: DataMonitor.exe [data_directory]\n";
        return 1;
    }

    DataMonitor monitor(dataDir, 3);
    monitor.run();
    return 0;
}
