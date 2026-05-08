#pragma once
#include <string>
#include <vector>

#include "../Models/Enums.h"

// ── ANSI 색상 상수 ────────────────────────────────────────────────
namespace Clr {
    constexpr const char* Reset     = "\033[0m";
    constexpr const char* Bold      = "\033[1m";
    constexpr const char* Dim       = "\033[2m";
    constexpr const char* BrRed     = "\033[91m";
    constexpr const char* BrYellow  = "\033[93m";
    constexpr const char* BrGreen   = "\033[92m";
    constexpr const char* BrCyan    = "\033[96m";
    constexpr const char* BoldCyan  = "\033[1;36m";
    constexpr const char* BoldWhite = "\033[1;37m";
}

class MainMenuView {
public:
    // ── 메인 메뉴 ────────────────────────────────────────────────
    static void showMainMenu();
    static int  promptMenuChoice();         // 0~6, 범위 외 재입력

    // ── 공통 피드백 ───────────────────────────────────────────────
    static void showSuccess(const std::string& msg);
    static void showError(const std::string& msg);
    static void pause();                    // Enter 대기

    // ── 레이아웃 유틸 ─────────────────────────────────────────────
    static void printHeader(const std::string& title);
    static void printLine(char ch = '-', int width = 80);
    static void clearScreen();

    // ── 입력 유틸 ─────────────────────────────────────────────────
    static std::string prompt(const std::string& label);
    static int         promptInt(const std::string& label);    // 숫자 아닌 경우 재입력
    static double      promptDouble(const std::string& label);
    // min~max 범위 정수 선택 루프 (errMsg 생략 시 자동 생성)
    static int         promptChoice(int min, int max, const std::string& errMsg = "");

    // ── 문자열 유틸 (UTF-8 표시 폭 기반) ────────────────────────
    static int         utf8DisplayWidth(const std::string& s);
    static std::string padRight(const std::string& s, int w);
    static std::string padLeft(const std::string& s, int w);
    static std::string truncate(const std::string& s, int max_w);
    static std::string colorStatus(OrderStatus status);
};
