#include "MainMenuView.h"

#include <iostream>
#include <limits>
#include <sstream>

static constexpr int W = 80;

// ── UTF-8 표시 폭 ────────────────────────────────────────────────

int MainMenuView::utf8DisplayWidth(const std::string& s) {
    int w = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        if      (c < 0x80) { w += 1; i += 1; }
        else if (c < 0xE0) { w += 1; i += 2; }
        else if (c < 0xF0) { w += 2; i += 3; }  // 한글/CJK = 2칸
        else                { w += 2; i += 4; }
    }
    return w;
}

// 표시 폭 w에 맞게 자르기 (남는 공간 공백 채움)
static std::string utf8Clip(const std::string& s, int w) {
    int cur = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        size_t cb; int cw;
        if      (c < 0x80) { cb = 1; cw = 1; }
        else if (c < 0xE0) { cb = 2; cw = 1; }
        else if (c < 0xF0) { cb = 3; cw = 2; }
        else                { cb = 4; cw = 2; }
        if (cur + cw > w) return s.substr(0, i) + std::string(w - cur, ' ');
        cur += cw; i += cb;
    }
    return s;
}

std::string MainMenuView::padRight(const std::string& s, int w) {
    int dw = utf8DisplayWidth(s);
    if (dw >= w) return utf8Clip(s, w);
    return s + std::string(w - dw, ' ');
}

std::string MainMenuView::padLeft(const std::string& s, int w) {
    int dw = utf8DisplayWidth(s);
    if (dw >= w) return utf8Clip(s, w);
    return std::string(w - dw, ' ') + s;
}

std::string MainMenuView::truncate(const std::string& s, int max_w) {
    if (utf8DisplayWidth(s) <= max_w) return s;
    return utf8Clip(s, max_w - 1) + "~";
}

std::string MainMenuView::colorStatus(OrderStatus status) {
    std::string text = toString(status);
    switch (status) {
        case OrderStatus::RESERVED:  return text;
        case OrderStatus::REJECTED:  return std::string(Clr::BrRed)    + text + Clr::Reset;
        case OrderStatus::PRODUCING: return std::string(Clr::BrYellow) + text + Clr::Reset;
        case OrderStatus::CONFIRMED: return std::string(Clr::BrGreen)  + text + Clr::Reset;
        case OrderStatus::RELEASE:   return std::string(Clr::BrCyan)   + text + Clr::Reset;
        default:                     return text;
    }
}

// ── 레이아웃 ─────────────────────────────────────────────────────

void MainMenuView::clearScreen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

void MainMenuView::printLine(char ch, int width) {
    std::cout << std::string(width, ch) << '\n';
}

void MainMenuView::printHeader(const std::string& title) {
    printLine('=');
    int tw  = utf8DisplayWidth(title);
    int pad = (W - tw) / 2;
    if (pad < 0) pad = 0;
    std::cout << std::string(pad, ' ') << Clr::BoldCyan << title << Clr::Reset << '\n';
    printLine('=');
}

// ── 피드백 ───────────────────────────────────────────────────────

void MainMenuView::showSuccess(const std::string& msg) {
    std::cout << Clr::BrGreen << "  ✓ " << msg << Clr::Reset << '\n';
}

void MainMenuView::showError(const std::string& msg) {
    std::cout << Clr::BrRed << "  ✗ " << msg << Clr::Reset << '\n';
}

void MainMenuView::pause() {
    std::cout << "\n계속하려면 Enter를 누르세요...";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

// ── 입력 유틸 ────────────────────────────────────────────────────

std::string MainMenuView::prompt(const std::string& label) {
    std::cout << "  " << label << ": ";
    std::string s;
    std::getline(std::cin, s);
    return s;
}

int MainMenuView::promptInt(const std::string& label) {
    while (true) {
        std::cout << "  " << label << ": ";
        std::string line;
        std::getline(std::cin, line);
        try {
            size_t pos;
            int val = std::stoi(line, &pos);
            if (pos == line.size()) return val;
        } catch (...) {}
        showError("숫자를 입력하세요.");
    }
}

double MainMenuView::promptDouble(const std::string& label) {
    while (true) {
        std::cout << "  " << label << ": ";
        std::string line;
        std::getline(std::cin, line);
        try {
            size_t pos;
            double val = std::stod(line, &pos);
            if (pos == line.size()) return val;
        } catch (...) {}
        showError("숫자를 입력하세요.");
    }
}

// ── 메인 메뉴 ────────────────────────────────────────────────────

void MainMenuView::showMainMenu() {
    printHeader("s-semi 시료 관리 시스템");
    std::cout << "\n"
              << "  1. 시료 관리\n"
              << "  2. 시료 주문\n"
              << "  3. 주문 승인/거절\n"
              << "  4. 모니터링\n"
              << "  5. 출고 처리\n"
              << "  6. 생산 라인\n"
              << "  0. 종료\n\n";
    printLine();
}

int MainMenuView::promptMenuChoice() {
    while (true) {
        std::cout << "메뉴를 선택하세요: ";
        std::string line;
        std::getline(std::cin, line);
        try {
            size_t pos;
            int val = std::stoi(line, &pos);
            if (pos == line.size() && val >= 0 && val <= 6) return val;
        } catch (...) {}
        showError("0~6 중 하나를 입력하세요.");
    }
}
