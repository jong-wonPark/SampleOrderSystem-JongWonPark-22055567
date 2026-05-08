#include "DataMonitor.h"

#include <algorithm>
#include <chrono>
#include <conio.h>   // _kbhit(), _getch() — Windows 전용
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

// ── ANSI 색상 상수 ────────────────────────────────────────────────

namespace Clr {
    constexpr const char* Reset     = "\033[0m";
    constexpr const char* Dim       = "\033[2m";
    constexpr const char* BrRed     = "\033[91m";
    constexpr const char* BrYellow  = "\033[93m";
    constexpr const char* BrGreen   = "\033[92m";
    constexpr const char* BrCyan    = "\033[96m";
    constexpr const char* BoldCyan  = "\033[1;36m";
    constexpr const char* BoldWhite = "\033[1;37m";
}

static constexpr int W = 80;

// ── 정적 유틸 구현 ────────────────────────────────────────────────

void DataMonitor::clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

void DataMonitor::hline(char ch, int w) {
    std::cout << std::string(w, ch) << '\n';
}

int DataMonitor::utf8_width(const std::string& s) {
    int width = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        if      (c < 0x80) { width += 1; i += 1; }
        else if (c < 0xE0) { width += 1; i += 2; }
        else if (c < 0xF0) { width += 2; i += 3; }  // 한글/CJK = 2칸
        else                { width += 2; i += 4; }
    }
    return width;
}

static std::string utf8_clip(const std::string& s, int w) {
    int cur = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        size_t cb; int cw;
        if      (c < 0x80) { cb=1; cw=1; }
        else if (c < 0xE0) { cb=2; cw=1; }
        else if (c < 0xF0) { cb=3; cw=2; }
        else                { cb=4; cw=2; }
        if (cur + cw > w) return s.substr(0, i) + std::string(w - cur, ' ');
        cur += cw; i += cb;
    }
    return s;
}

std::string DataMonitor::rpad(const std::string& s, int w) {
    int dw = utf8_width(s);
    if (dw >= w) return utf8_clip(s, w);
    return s + std::string(w - dw, ' ');
}

std::string DataMonitor::lpad(const std::string& s, int w) {
    int dw = utf8_width(s);
    if (dw >= w) return utf8_clip(s, w);
    return std::string(w - dw, ' ') + s;
}

std::string DataMonitor::trunc(const std::string& s, int max_w) {
    if (utf8_width(s) <= max_w) return s;
    return utf8_clip(s, max_w - 1) + "~";
}

std::string DataMonitor::colored_status(OrderStatus s) {
    std::string text = toString(s);
    switch (s) {
        case OrderStatus::RESERVED:  return text;
        case OrderStatus::REJECTED:  return std::string(Clr::BrRed)    + text + Clr::Reset;
        case OrderStatus::PRODUCING: return std::string(Clr::BrYellow) + text + Clr::Reset;
        case OrderStatus::CONFIRMED: return std::string(Clr::BrGreen)  + text + Clr::Reset;
        case OrderStatus::RELEASE:   return std::string(Clr::BrCyan)   + text + Clr::Reset;
        default:                     return text;
    }
}

std::string DataMonitor::now_str() {
    auto t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ── 생성자 / 소멸자 ───────────────────────────────────────────────

DataMonitor::DataMonitor(fs::path data_dir, int refresh_sec)
    : data_dir_(std::move(data_dir)), refresh_sec_(refresh_sec) {}

DataMonitor::~DataMonitor() {
    running_ = false;
    refresh_cv_.notify_all();
    if (bg_thread_.joinable()) bg_thread_.join();
}

// ── 백그라운드 갱신 스레드 ────────────────────────────────────────

void DataMonitor::bg_refresh_loop() {
    while (running_) {
        {
            MonitorSnapshot snap = load_snapshot();
            std::lock_guard<std::mutex> lock(snapshot_mutex_);
            snapshot_ = std::move(snap);
        }
        data_ready_ = true;

        std::unique_lock<std::mutex> lock(refresh_cv_mutex_);
        refresh_cv_.wait_for(lock,
            std::chrono::seconds(refresh_sec_),
            [this] { return force_refresh_.load() || !running_.load(); });
        force_refresh_ = false;
    }
}

MonitorSnapshot DataMonitor::load_snapshot() const {
    MonitorSnapshot snap;
    snap.captured_at = now_str();
    try {
        InventoryManager       invMgr (data_dir_ / "inventory.json");
        OrderManager           ordMgr (data_dir_ / "orders.json");
        ProductionQueueManager prodMgr(data_dir_ / "production_queue.json");
        snap.samples   = invMgr.get_all_samples();
        snap.inventory = invMgr.get_all_inventory();
        snap.orders    = ordMgr.get_all();
        snap.queue     = prodMgr.get_all();
    } catch (const std::exception& e) {
        snap.has_error = true;
        snap.error_msg = e.what();
    }
    return snap;
}

// ── 메인 루프 ─────────────────────────────────────────────────────

void DataMonitor::run() {
    bg_thread_ = std::thread(&DataMonitor::bg_refresh_loop, this);

    std::cout << "데이터 로딩 중..." << std::flush;
    while (!data_ready_ && running_)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

    MonitorSnapshot local_snap;
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex_);
        local_snap = snapshot_;
    }
    clear_screen();
    display(local_snap);

    while (running_) {
        if (data_ready_.exchange(false)) {
            {
                std::lock_guard<std::mutex> lock(snapshot_mutex_);
                local_snap = snapshot_;
            }
            clear_screen();
            display(local_snap);
        }

        if (_kbhit()) {
            char c = _getch();
            handle_input(c);
            if (running_) {
                clear_screen();
                display(local_snap);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    running_ = false;
    refresh_cv_.notify_all();
    if (bg_thread_.joinable()) bg_thread_.join();

    clear_screen();
    std::cout << "DataMonitor를 종료합니다.\n";
}

void DataMonitor::handle_input(char c) {
    switch (c) {
        case '1': current_view_ = View::Dashboard;  order_filter_ = ""; break;
        case '2': current_view_ = View::Inventory;  order_filter_ = ""; break;
        case '3': current_view_ = View::Orders;     order_filter_ = ""; break;
        case '4': current_view_ = View::Production; order_filter_ = ""; break;
        // 주문 상태 필터
        case 'a': case 'A': order_filter_ = "RESERVED";  current_view_ = View::Orders; break;
        case 'b': case 'B': order_filter_ = "PRODUCING"; current_view_ = View::Orders; break;
        case 'c': case 'C': order_filter_ = "CONFIRMED"; current_view_ = View::Orders; break;
        case 'd': case 'D': order_filter_ = "RELEASE";   current_view_ = View::Orders; break;
        case 'e': case 'E': order_filter_ = "REJECTED";  current_view_ = View::Orders; break;
        case 'r': case 'R':
            force_refresh_ = true;
            refresh_cv_.notify_one();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            data_ready_ = true;
            break;
        case 'q': case 'Q': running_ = false; break;
    }
}

// ── 화면 표시 ─────────────────────────────────────────────────────

void DataMonitor::display(const MonitorSnapshot& snap) const {
    show_header(snap);
    show_nav();

    if (snap.has_error) {
        std::cout << Clr::BrRed << "  [오류] " << snap.error_msg << Clr::Reset << "\n";
        show_footer();
        return;
    }

    switch (current_view_) {
        case View::Dashboard:   show_dashboard(snap);   break;
        case View::Inventory:   show_inventory(snap);   break;
        case View::Orders:      show_orders(snap);      break;
        case View::Production:  show_production(snap);  break;
    }

    show_footer();
}

void DataMonitor::show_header(const MonitorSnapshot& snap) const {
    std::cout << Clr::BoldCyan;
    hline('=');
    std::string title = "  s-semi DataMonitor";
    std::string ts    = "[" + snap.captured_at + "]";
    int gap = W - utf8_width(title) - (int)ts.size();
    std::cout << title << std::string(std::max(1, gap), ' ') << ts << "\n";

    std::string path_s = "  데이터 경로: " + data_dir_.string();
    std::string intv_s = "갱신: " + std::to_string(refresh_sec_) + "초";
    int tw = utf8_width(path_s);
    int gap2 = W - tw - (int)intv_s.size();
    if (tw > W - (int)intv_s.size() - 2)
        path_s = "  " + trunc(data_dir_.string(), W - (int)intv_s.size() - 4) + "  ";
    std::cout << path_s << std::string(std::max(1, gap2), ' ') << intv_s << "\n";
    hline('=');
    std::cout << Clr::Reset;
}

void DataMonitor::show_nav() const {
    std::cout << "\n" << Clr::BoldWhite
              << " [1] 대시보드  [2] 재고 현황  [3] 주문 목록  [4] 생산 큐"
              << "  [R] 새로고침  [Q] 종료"
              << Clr::Reset << "\n";
    if (current_view_ == View::Orders) {
        std::cout << Clr::Dim
                  << "   주문 필터: [A] RESERVED  [B] PRODUCING  [C] CONFIRMED"
                     "  [D] RELEASE  [E] REJECTED  [3] 전체"
                  << Clr::Reset << "\n";
    }
    std::cout << "\n";
}

void DataMonitor::show_dashboard(const MonitorSnapshot& snap) const {
    // ── 카운트 집계 ───────────────────────────────────────────────
    int reserved=0, producing=0, confirmed=0, release=0, rejected=0;
    for (const auto& o : snap.orders) {
        switch (o.status) {
            case OrderStatus::RESERVED:  ++reserved;  break;
            case OrderStatus::PRODUCING: ++producing; break;
            case OrderStatus::CONFIRMED: ++confirmed; break;
            case OrderStatus::RELEASE:   ++release;   break;
            case OrderStatus::REJECTED:  ++rejected;  break;
        }
    }
    int waiting=0, in_prod=0;
    for (const auto& p : snap.queue) {
        if (p.status == ProductionQueueStatus::Waiting)          ++waiting;
        else if (p.status == ProductionQueueStatus::InProduction) ++in_prod;
    }
    int total_stock = 0;
    for (const auto& inv : snap.inventory) total_stock += inv.quantity;

    // ── 요약 ─────────────────────────────────────────────────────
    std::cout << Clr::BoldWhite << "[ 통합 현황 요약 ]" << Clr::Reset << "\n";
    hline('-');
    std::cout << "  재고    " << Clr::BrCyan << snap.samples.size() << "종" << Clr::Reset
              << "   총 재고량 " << total_stock << " EA\n";
    std::cout << "  주문    "
              << "대기 "   << reserved  << "건  "
              << Clr::BrYellow << "PRODUCING " << producing << "건" << Clr::Reset << "  "
              << Clr::BrGreen  << "CONFIRMED " << confirmed << "건" << Clr::Reset << "  "
              << Clr::BrCyan   << "RELEASE "   << release   << "건" << Clr::Reset << "  "
              << Clr::BrRed    << "REJECTED "  << rejected  << "건" << Clr::Reset
              << "  (합계 " << snap.orders.size() << "건)\n";
    std::cout << "  생산 큐 "
              << "대기 " << waiting << "건  "
              << Clr::BrYellow << "생산 중 " << in_prod << "건" << Clr::Reset
              << "  (합계 " << snap.queue.size() << "건)\n";
    hline('-');

    // ── 재고 미니 테이블 ──────────────────────────────────────────
    if (!snap.samples.empty()) {
        std::cout << "\n" << Clr::BoldCyan
                  << "[ 재고 현황  " << snap.samples.size() << "종 ]" << Clr::Reset << "\n";
        hline('-');
        std::cout << Clr::BoldWhite
                  << "  " << rpad("시료ID", 8)   << "  "
                  << rpad("시료명", 20)            << "  "
                  << lpad("재고", 6)               << "  "
                  << lpad("수율", 6)               << "\n" << Clr::Reset;
        hline('-');
        for (const auto& s : snap.samples) {
            int qty = 0;
            for (const auto& inv : snap.inventory)
                if (inv.sample_id == s.sample_id) { qty = inv.quantity; break; }
            std::ostringstream yield_s;
            yield_s << std::fixed << std::setprecision(1) << (s.yield_rate * 100.0) << "%";
            const char* qc = (qty == 0) ? Clr::BrRed
                           : (qty < 10) ? Clr::BrYellow
                           : Clr::BrGreen;
            std::cout << "  "
                      << rpad(trunc(s.sample_id, 8), 8)    << "  "
                      << rpad(trunc(s.sample_name, 20), 20) << "  "
                      << qc << lpad(std::to_string(qty), 6) << Clr::Reset << "  "
                      << lpad(yield_s.str(), 6) << "\n";
        }
        hline('-');
    }

    // ── 주문 미니 테이블 ──────────────────────────────────────────
    if (!snap.orders.empty()) {
        std::cout << "\n" << Clr::BoldCyan
                  << "[ 주문 목록  " << snap.orders.size() << "건 ]" << Clr::Reset << "\n";
        hline('-');
        std::cout << Clr::BoldWhite
                  << "  " << rpad("주문번호", 18)  << "  "
                  << rpad("시료명", 16)             << "  "
                  << lpad("수량", 4)                << "  "
                  << rpad("고객명", 12)             << "  "
                  << "상태\n" << Clr::Reset;
        hline('-');
        for (const auto& o : snap.orders) {
            std::cout << "  "
                      << rpad(trunc(o.order_number, 18), 18)  << "  "
                      << rpad(trunc(o.sample_name, 16), 16)   << "  "
                      << lpad(std::to_string(o.order_quantity), 4) << "  "
                      << rpad(trunc(o.customer_name, 12), 12) << "  "
                      << colored_status(o.status) << "\n";
        }
        hline('-');
    }
}

void DataMonitor::show_inventory(const MonitorSnapshot& snap) const {
    std::cout << Clr::BoldCyan
              << "[ 재고 현황 상세  " << snap.samples.size() << "종 ]" << Clr::Reset << "\n";
    hline('=');
    std::cout << Clr::BoldWhite
              << "  " << rpad("시료ID", 8)      << "  "
              << rpad("시료명", 20)               << "  "
              << lpad("재고", 6)                  << "  "
              << rpad("단위", 4)                  << "  "
              << lpad("수율", 6)                  << "  "
              << lpad("생산(분)", 8)               << "  "
              << lpad("상태", 4)                  << "  "
              << "최종 갱신\n" << Clr::Reset;
    hline('-');
    if (snap.samples.empty()) {
        std::cout << Clr::Dim << "  (데이터 없음)\n" << Clr::Reset;
    } else {
        for (const auto& s : snap.samples) {
            int qty = 0; std::string unit = "EA"; std::string updated;
            for (const auto& inv : snap.inventory) {
                if (inv.sample_id == s.sample_id) {
                    qty = inv.quantity; unit = inv.unit; updated = inv.last_updated; break;
                }
            }
            // 재고 상태: 큐 존재 여부 + RESERVED 주문 기준
            bool hasQueue = false;
            for (const auto& p : snap.queue)
                if (p.sample_id == s.sample_id) { hasQueue = true; break; }
            int pending = 0;
            for (const auto& o : snap.orders)
                if (o.sample_id == s.sample_id && o.status == OrderStatus::RESERVED)
                    pending += o.order_quantity;
            std::string status = (qty == 0)        ? "고갈"
                               : hasQueue          ? "부족"
                               : (pending > qty)   ? "부족" : "여유";
            const char* sc = (status == "고갈") ? Clr::BrRed
                           : (status == "부족") ? Clr::BrYellow
                           : Clr::BrGreen;

            std::ostringstream yield_s;
            int prod_mins = static_cast<int>(std::round(s.avg_production_time_hours * 60.0));
            yield_s << std::fixed << std::setprecision(1) << (s.yield_rate * 100.0) << "%";
            const char* qc = (qty == 0) ? Clr::BrRed
                           : (qty < 10) ? Clr::BrYellow
                           : Clr::BrGreen;
            std::cout << "  "
                      << rpad(trunc(s.sample_id, 8), 8)     << "  "
                      << rpad(trunc(s.sample_name, 20), 20)  << "  "
                      << qc << lpad(std::to_string(qty), 6) << Clr::Reset << "  "
                      << rpad(unit, 4)             << "  "
                      << lpad(yield_s.str(), 6)              << "  "
                      << lpad(std::to_string(prod_mins), 8)  << "  "
                      << sc << lpad(status, 4) << Clr::Reset << "  "
                      << trunc(updated, 19) << "\n";
        }
    }
    hline('=');
}

void DataMonitor::show_orders(const MonitorSnapshot& snap) const {
    // 상태 필터 적용
    std::vector<Order> filtered;
    for (const auto& o : snap.orders) {
        if (order_filter_.empty() || toJsonString(o.status) == order_filter_)
            filtered.push_back(o);
    }

    std::string title = "[ 주문 목록";
    if (!order_filter_.empty()) title += "  필터: " + order_filter_;
    title += "  " + std::to_string(filtered.size()) + "건 ]";
    std::cout << Clr::BoldCyan << title << Clr::Reset << "\n";
    hline('=');
    std::cout << Clr::BoldWhite
              << "  " << rpad("주문번호", 18)   << "  "
              << rpad("시료명", 16)              << "  "
              << lpad("수량", 4)                 << "  "
              << rpad("고객명", 14)              << "  "
              << rpad("접수 일시", 19)           << "  "
              << "상태\n" << Clr::Reset;
    hline('-');
    if (filtered.empty()) {
        std::cout << Clr::Dim << "  (해당 조건의 주문 없음)\n" << Clr::Reset;
    } else {
        for (const auto& o : filtered) {
            std::cout << "  "
                      << rpad(trunc(o.order_number, 18), 18)  << "  "
                      << rpad(trunc(o.sample_name, 16), 16)   << "  "
                      << lpad(std::to_string(o.order_quantity), 4) << "  "
                      << rpad(trunc(o.customer_name, 14), 14) << "  "
                      << rpad(trunc(o.order_date, 19), 19)    << "  "
                      << colored_status(o.status) << "\n";
        }
    }
    hline('=');

    // REJECTED 주문 메모 섹션
    bool has_notes = false;
    for (const auto& o : filtered) if (!o.note.empty()) { has_notes = true; break; }
    if (has_notes) {
        std::cout << "\n" << Clr::BoldWhite << "[ 메모 (거절 사유 등) ]" << Clr::Reset << "\n";
        hline('-');
        for (const auto& o : filtered) {
            if (o.note.empty()) continue;
            std::cout << "  " << rpad(o.order_number, 18) << "  "
                      << Clr::BrRed << o.note << Clr::Reset << "\n";
        }
        hline('-');
    }
}

void DataMonitor::show_production(const MonitorSnapshot& snap) const {
    std::cout << Clr::BoldCyan
              << "[ 생산 큐  " << snap.queue.size() << "건 ]" << Clr::Reset << "\n";
    hline('=');

    std::vector<ProductionQueueItem> in_prod, waiting;
    for (const auto& p : snap.queue) {
        if (p.status == ProductionQueueStatus::InProduction) in_prod.push_back(p);
        else                                                  waiting.push_back(p);
    }

    // ── 생산 중 ────────────────────────────────────────────────────
    std::cout << Clr::BrYellow << "[생산 중 (InProduction) — " << in_prod.size() << "건]" << Clr::Reset << "\n";
    if (in_prod.empty()) {
        std::cout << Clr::Dim << "  (없음)\n" << Clr::Reset;
    } else {
        hline('-');
        std::cout << Clr::BoldWhite
                  << "  " << rpad("생산ID", 19)    << "  "
                  << rpad("주문번호", 18)            << "  "
                  << rpad("시료명", 16)              << "  "
                  << lpad("수량", 4)                 << "  "
                  << "시작 시각\n" << Clr::Reset;
        hline('-');
        for (const auto& p : in_prod) {
            std::cout << "  "
                      << Clr::BrYellow
                      << rpad(trunc(p.production_id, 19), 19) << Clr::Reset << "  "
                      << rpad(trunc(p.order_number, 18), 18)  << "  "
                      << rpad(trunc(p.sample_name, 16), 16)   << "  "
                      << lpad(std::to_string(p.planned_quantity), 4) << "  "
                      << trunc(p.started_at.empty() ? "-" : p.started_at, 19) << "\n";
        }
        hline('-');
    }

    // ── 대기 중 ────────────────────────────────────────────────────
    std::cout << "\n" << Clr::BrCyan
              << "[대기 중 (Waiting) — " << waiting.size() << "건, FIFO 순서]"
              << Clr::Reset << "\n";
    if (waiting.empty()) {
        std::cout << Clr::Dim << "  (없음)\n" << Clr::Reset;
    } else {
        hline('-');
        std::cout << Clr::BoldWhite
                  << "  " << lpad("순번", 4)         << "  "
                  << rpad("생산ID", 19)               << "  "
                  << rpad("주문번호", 18)              << "  "
                  << rpad("시료명", 16)                << "  "
                  << lpad("수량", 4)                   << "  "
                  << "등록 시각\n" << Clr::Reset;
        hline('-');
        for (int i = 0; i < (int)waiting.size(); ++i) {
            const auto& p = waiting[i];
            std::cout << "  "
                      << lpad(std::to_string(i + 1), 4)        << "  "
                      << rpad(trunc(p.production_id, 19), 19)   << "  "
                      << rpad(trunc(p.order_number, 18), 18)    << "  "
                      << rpad(trunc(p.sample_name, 16), 16)     << "  "
                      << lpad(std::to_string(p.planned_quantity), 4) << "  "
                      << trunc(p.queued_at, 19) << "\n";
        }
        hline('-');
    }
    hline('=');
}

void DataMonitor::show_footer() const {
    std::cout << "\n" << Clr::Dim
              << "  자동 갱신: " << refresh_sec_ << "초"
              << "  |  [1-4] 뷰 전환  [A-E] 주문 필터  [R] 즉시 갱신  [Q] 종료"
              << Clr::Reset << "\n";
}
