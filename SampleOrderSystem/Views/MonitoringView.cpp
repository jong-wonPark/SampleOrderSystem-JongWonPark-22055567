#include "MonitoringView.h"

#include <iostream>
#include <iomanip>
#include <sstream>

// RESERVED 주문 기준 해당 시료의 재고 상태 반환
// CONFIRMED은 승인 시 이미 재고가 차감되었으므로 제외
static std::string stockStatus(const std::string& sample_id, int qty,
                                const std::vector<Order>& orders)
{
    if (qty == 0) return "고갈";

    int pending = 0;
    for (const auto& o : orders)
        if (o.sample_id == sample_id && o.status == OrderStatus::RESERVED)
            pending += o.order_quantity;

    return (pending > qty) ? "부족" : "여유";
}

static const char* stockStatusColor(const std::string& status) {
    if (status == "고갈") return Clr::BrRed;
    if (status == "부족") return Clr::BrYellow;
    return Clr::BrGreen;
}

void MonitoringView::showOrderSummary(const std::vector<Order>& orders) {
    int reserved = 0, producing = 0, confirmed = 0, release = 0, rejected = 0;
    for (const auto& o : orders) {
        switch (o.status) {
            case OrderStatus::RESERVED:  ++reserved;  break;
            case OrderStatus::PRODUCING: ++producing; break;
            case OrderStatus::CONFIRMED: ++confirmed; break;
            case OrderStatus::RELEASE:   ++release;   break;
            case OrderStatus::REJECTED:  ++rejected;  break;
        }
    }
    using V = MainMenuView;
    std::cout << "[주문 현황]\n";
    std::cout << "  " << V::padRight("주문 접수   (RESERVED)", 26)  << ":  " << reserved  << "건\n";
    std::cout << "  " << V::padRight("생산 중     (PRODUCING)", 26) << ":  "
              << Clr::BrYellow << producing << "건" << Clr::Reset << "\n";
    std::cout << "  " << V::padRight("출고 대기   (CONFIRMED)", 26) << ":  "
              << Clr::BrGreen  << confirmed << "건" << Clr::Reset << "\n";
    std::cout << "  " << V::padRight("출고 완료   (RELEASE)", 26)   << ":  "
              << Clr::BrCyan   << release   << "건" << Clr::Reset << "\n";
    std::cout << "  " << V::padRight("거절됨      (REJECTED)", 26)  << ":  "
              << Clr::BrRed    << rejected  << "건" << Clr::Reset << "\n";
    std::cout << "  " << std::string(32, '-') << "\n";
    std::cout << "  " << V::padRight("합계", 26) << ":  " << orders.size() << "건\n";
}

void MonitoringView::showInventoryTable(const std::vector<SampleItem>&    samples,
                                         const std::vector<InventoryItem>& inventory,
                                         const std::vector<Order>&         orders)
{
    using V = MainMenuView;
    std::cout << "\n[재고 현황]\n";
    if (samples.empty()) {
        std::cout << "  등록된 시료가 없습니다.\n";
        return;
    }
    V::printLine('-');
    std::cout << "  "
              << V::padRight("시료ID", 8)   << "  "
              << V::padRight("시료명", 20)   << "  "
              << V::padLeft("재고", 8)       << "  "
              << V::padLeft("상태", 4)       << "\n";
    V::printLine('-');
    for (const auto& s : samples) {
        int qty = 0;
        std::string unit = "EA";
        for (const auto& inv : inventory)
            if (inv.sample_id == s.sample_id) { qty = inv.quantity; unit = inv.unit; break; }

        std::string status = stockStatus(s.sample_id, qty, orders);
        const char* sc = stockStatusColor(status);

        const char* qc = (qty == 0) ? Clr::BrRed
                       : (qty < 10) ? Clr::BrYellow
                       : Clr::BrGreen;
        std::ostringstream qty_s;
        qty_s << qty << " " << unit;

        std::cout << "  "
                  << V::padRight(V::truncate(s.sample_id, 8), 8)    << "  "
                  << V::padRight(V::truncate(s.sample_name, 20), 20) << "  "
                  << qc << V::padLeft(qty_s.str(), 8) << Clr::Reset  << "  "
                  << sc << V::padLeft(status, 4)       << Clr::Reset  << "\n";
    }
    V::printLine('-');
    // 범례
    std::cout << "  "
              << Clr::BrGreen  << "여유" << Clr::Reset << ": 주문대비 재고 충분  "
              << Clr::BrYellow << "부족" << Clr::Reset << ": RESERVED 주문량 > 재고  "
              << Clr::BrRed    << "고갈" << Clr::Reset << ": 수량 0\n";
}

void MonitoringView::showDashboard(const std::vector<Order>&         orders,
                                    const std::vector<SampleItem>&    samples,
                                    const std::vector<InventoryItem>& inventory)
{
    MainMenuView::printHeader("모니터링");
    std::cout << '\n';
    showOrderSummary(orders);
    showInventoryTable(samples, inventory, orders);
    MainMenuView::printLine('-');
}
