#include "SampleView.h"

#include <iostream>
#include <iomanip>
#include <sstream>

int SampleView::promptSubMenu() {
    MainMenuView::printHeader("시료 관리");
    std::cout << "\n"
              << "  1. 시료 등록\n"
              << "  2. 목록 조회\n"
              << "  3. 이름 검색\n"
              << "  0. 뒤로\n\n";
    MainMenuView::printLine();
    while (true) {
        std::cout << "선택: ";
        std::string line;
        std::getline(std::cin, line);
        try {
            int val = std::stoi(line);
            if (val >= 0 && val <= 3) return val;
        } catch (...) {}
        MainMenuView::showError("0~3 중 하나를 입력하세요.");
    }
}

SampleInput SampleView::promptSampleInput() {
    MainMenuView::printHeader("시료 등록");
    std::cout << '\n';
    SampleInput in;
    in.sample_id   = MainMenuView::prompt("시료ID (예: S-001)");
    in.sample_name = MainMenuView::prompt("시료명");
    in.avg_production_time_hours = MainMenuView::promptDouble("평균 생산시간 (시간)");
    while (true) {
        in.yield_rate = MainMenuView::promptDouble("수율 (0.0 ~ 1.0)");
        if (in.yield_rate >= 0.0 && in.yield_rate <= 1.0) break;
        MainMenuView::showError("수율은 0.0 ~ 1.0 범위여야 합니다.");
    }
    in.initial_quantity = MainMenuView::promptInt("초기 재고 수량");
    MainMenuView::printLine();
    return in;
}

// ── 내부: 테이블 헤더 / 행 ────────────────────────────────────────

static void printSampleHeader() {
    using V = MainMenuView;
    V::printLine('-');
    std::cout << "  "
              << V::padLeft("번호", 4)            << "  "
              << V::padRight("시료ID", 8)          << "  "
              << V::padRight("시료명", 20)          << "  "
              << V::padLeft("생산시간(h)", 11)      << "  "
              << V::padLeft("수율", 6)             << "  "
              << V::padLeft("재고", 8)             << "\n";
    V::printLine('-');
}

static void printSampleRow(int idx,
                            const SampleItem& s,
                            const std::vector<InventoryItem>& inventory)
{
    using V = MainMenuView;
    int qty = 0;
    std::string unit = "EA";
    for (const auto& inv : inventory)
        if (inv.sample_id == s.sample_id) { qty = inv.quantity; unit = inv.unit; break; }

    std::ostringstream time_s, yield_s, qty_s;
    time_s  << std::fixed << std::setprecision(1) << s.avg_production_time_hours;
    yield_s << std::fixed << std::setprecision(1) << (s.yield_rate * 100.0) << "%";
    qty_s   << qty << " " << unit;

    const char* qc = (qty == 0) ? Clr::BrRed
                   : (qty < 10) ? Clr::BrYellow
                   : Clr::BrGreen;

    std::cout << "  "
              << V::padLeft(std::to_string(idx), 4)            << "  "
              << V::padRight(V::truncate(s.sample_id, 8), 8)    << "  "
              << V::padRight(V::truncate(s.sample_name, 20), 20) << "  "
              << V::padLeft(time_s.str(), 11)                    << "  "
              << V::padLeft(yield_s.str(), 6)                    << "  "
              << qc << V::padLeft(qty_s.str(), 8) << Clr::Reset  << "\n";
}

void SampleView::showSampleTable(const std::vector<SampleItem>&    samples,
                                  const std::vector<InventoryItem>& inventory)
{
    if (samples.empty()) {
        std::cout << "  등록된 시료가 없습니다.\n";
        return;
    }
    printSampleHeader();
    for (int i = 0; i < (int)samples.size(); ++i)
        printSampleRow(i + 1, samples[i], inventory);
    MainMenuView::printLine('-');
    std::cout << "  총 " << samples.size() << "종\n";
}

std::string SampleView::promptSearchKeyword() {
    std::cout << '\n';
    return MainMenuView::prompt("검색 키워드 (이름 부분 일치)");
}

void SampleView::showSearchResults(const std::vector<SampleItem>&    results,
                                    const std::vector<InventoryItem>& inventory)
{
    if (results.empty()) {
        std::cout << "  검색 결과가 없습니다.\n";
        return;
    }
    std::cout << "  검색 결과: " << results.size() << "건\n";
    printSampleHeader();
    for (int i = 0; i < (int)results.size(); ++i)
        printSampleRow(i + 1, results[i], inventory);
    MainMenuView::printLine('-');
}
