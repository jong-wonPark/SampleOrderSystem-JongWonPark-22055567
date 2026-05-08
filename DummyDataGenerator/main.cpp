#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>

#include "Persistence/DataPersistence.h"

namespace fs = std::filesystem;

// ── 유틸 ─────────────────────────────────────────────────────────

static fs::path getExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(NULL, buf, MAX_PATH);
    return fs::path(buf).parent_path();
}

static void clearJson(const fs::path& p) {
    std::ofstream f(p, std::ios::trunc);
    f << "[]";
}

static void sep(const std::string& title) {
    std::cout << "\n[" << title << "]\n";
}

// ── 데이터 정의 ───────────────────────────────────────────────────

struct SampleDef {
    std::string id, name;
    double avgTime, yieldRate;
    int qty;
};

struct OrderDef {
    std::string sampleId, sampleName, customer;
    int qty;
    OrderStatus targetStatus;
    std::string note;
};

// ── main ──────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // 기본 출력 경로: 솔루션 루트 data/ (exe 기준 3단계 위)
    // 실행 파일 위치: DummyDataGenerator\x64\Debug\DummyDataGenerator.exe
    // 솔루션 루트:   DummyDataGenerator\..\..\..\  (= SampleOrderSystem\)
    fs::path dataDir = (argc > 1)
        ? fs::path(argv[1])
        : getExeDir() / ".." / ".." / ".." / "data";
    dataDir = fs::weakly_canonical(dataDir);
    fs::create_directories(dataDir);

    std::cout << "================================================\n";
    std::cout << "  s-semi DummyDataGenerator\n";
    std::cout << "================================================\n";
    std::cout << "출력 경로: " << dataDir.string() << "\n";

    // 기존 파일 초기화
    clearJson(dataDir / "inventory.json");
    clearJson(dataDir / "orders.json");
    clearJson(dataDir / "production_queue.json");

    InventoryManager       invMgr (dataDir / "inventory.json");
    OrderManager           ordMgr (dataDir / "orders.json");
    ProductionQueueManager prodMgr(dataDir / "production_queue.json");

    // ────────────────────────────────────────────────────────────
    // 1. 시료 마스터 10종
    // ────────────────────────────────────────────────────────────
    sep("1. 시료 마스터 10종 등록");

    const std::vector<SampleDef> samples = {
        {"S-001", "실리콘 웨이퍼 200mm",   24.5, 0.92, 150},
        {"S-002", "실리콘 웨이퍼 300mm",   36.0, 0.88,  80},
        {"S-003", "GaN 기판 2인치",        48.0, 0.75,   0},  // 재고 없음
        {"S-004", "SiC 기판 4인치",        72.0, 0.70,  25},
        {"S-005", "GaAs 기판 3인치",       36.0, 0.80,   5},  // 소량
        {"S-006", "InP 기판 2인치",        60.0, 0.65,   0},  // 재고 없음
        {"S-007", "Ge 웨이퍼 100mm",      20.0, 0.90,  60},
        {"S-008", "Al2O3 기판 2인치",      30.0, 0.85,  30},
        {"S-009", "사파이어 기판 4인치",   48.0, 0.78,   0},  // 재고 없음
        {"S-010", "SiGe 웨이퍼 150mm",    28.0, 0.87,  12},
    };

    for (const auto& s : samples) {
        invMgr.add_sample(s.id, s.name, s.avgTime, s.yieldRate, s.qty);
        std::cout << "  + " << s.id << "  " << s.name
                  << "  (재고 " << s.qty << " EA)\n";
    }

    // ────────────────────────────────────────────────────────────
    // 2. 주문 20건 (RESERVED×5, CONFIRMED×4, PRODUCING×5, RELEASE×3, REJECTED×3)
    // ────────────────────────────────────────────────────────────
    sep("2. 주문 20건 생성");

    const std::vector<OrderDef> orderDefs = {
        // ── RESERVED (검토 대기) ──────────────────────────────────
        {"S-001","실리콘 웨이퍼 200mm","삼성전자",    20, OrderStatus::RESERVED,  ""},
        {"S-004","SiC 기판 4인치",     "SK하이닉스",   8, OrderStatus::RESERVED,  ""},
        {"S-007","Ge 웨이퍼 100mm",   "LG이노텍",    15, OrderStatus::RESERVED,  ""},
        {"S-010","SiGe 웨이퍼 150mm", "DB하이텍",     5, OrderStatus::RESERVED,  ""},
        {"S-005","GaAs 기판 3인치",   "서울반도체",    3, OrderStatus::RESERVED,  ""},
        // ── CONFIRMED (출고 대기) ─────────────────────────────────
        {"S-001","실리콘 웨이퍼 200mm","삼성전자",    10, OrderStatus::CONFIRMED, ""},
        {"S-002","실리콘 웨이퍼 300mm","SK하이닉스",   5, OrderStatus::CONFIRMED, ""},
        {"S-004","SiC 기판 4인치",     "LG이노텍",     3, OrderStatus::CONFIRMED, ""},
        {"S-008","Al2O3 기판 2인치",   "DB하이텍",     8, OrderStatus::CONFIRMED, ""},
        // ── PRODUCING (생산 중 — 생산 큐 항목 별도 생성) ──────────
        {"S-003","GaN 기판 2인치",     "삼성전자",    10, OrderStatus::PRODUCING, ""},
        {"S-006","InP 기판 2인치",     "SK하이닉스",   5, OrderStatus::PRODUCING, ""},
        {"S-009","사파이어 기판 4인치","LG이노텍",     8, OrderStatus::PRODUCING, ""},
        {"S-003","GaN 기판 2인치",     "서울반도체",  12, OrderStatus::PRODUCING, ""},
        {"S-009","사파이어 기판 4인치","DB하이텍",     6, OrderStatus::PRODUCING, ""},
        // ── RELEASE (출고 완료) ───────────────────────────────────
        {"S-001","실리콘 웨이퍼 200mm","삼성전자",    50, OrderStatus::RELEASE,   ""},
        {"S-002","실리콘 웨이퍼 300mm","SK하이닉스",  30, OrderStatus::RELEASE,   ""},
        {"S-007","Ge 웨이퍼 100mm",   "LG이노텍",    20, OrderStatus::RELEASE,   ""},
        // ── REJECTED (거절됨) ─────────────────────────────────────
        {"S-004","SiC 기판 4인치",     "삼성전자",   100, OrderStatus::REJECTED,  "수량 초과 (최대 50개)"},
        {"S-010","SiGe 웨이퍼 150mm", "SK하이닉스",  50, OrderStatus::REJECTED,  "생산 일정 미확정"},
        {"S-006","InP 기판 2인치",     "LG이노텍",    20, OrderStatus::REJECTED,  "재고 예측 불가"},
    };

    std::vector<std::string> orderNums;
    for (const auto& d : orderDefs) {
        auto o = ordMgr.add(d.sampleId, d.sampleName, d.customer, d.qty);
        orderNums.push_back(o.order_number);

        if (d.targetStatus == OrderStatus::CONFIRMED) {
            ordMgr.update_status(o.order_number, OrderStatus::CONFIRMED);
            invMgr.update_stock(d.sampleId, -d.qty);  // 승인 시 재고 차감
        } else if (d.targetStatus == OrderStatus::PRODUCING) {
            ordMgr.update_status(o.order_number, OrderStatus::PRODUCING);
        } else if (d.targetStatus == OrderStatus::RELEASE) {
            ordMgr.update_status(o.order_number, OrderStatus::RELEASE);
        } else if (d.targetStatus == OrderStatus::REJECTED) {
            ordMgr.update_status(o.order_number, OrderStatus::REJECTED, d.note);
        }

        std::string statusStr;
        switch (d.targetStatus) {
            case OrderStatus::RESERVED:  statusStr = "RESERVED";  break;
            case OrderStatus::CONFIRMED: statusStr = "CONFIRMED"; break;
            case OrderStatus::PRODUCING: statusStr = "PRODUCING"; break;
            case OrderStatus::RELEASE:   statusStr = "RELEASE";   break;
            case OrderStatus::REJECTED:  statusStr = "REJECTED";  break;
        }
        std::cout << "  + " << o.order_number
                  << "  " << d.customer
                  << "  (" << d.sampleId << " x" << d.qty << ")"
                  << "  -> " << statusStr << "\n";
    }

    // ────────────────────────────────────────────────────────────
    // 3. 생산 큐 5건 (PRODUCING 주문 index 9~13과 연결)
    //    Waiting: 3건 (index 9,10,11), InProduction: 2건 (index 12,13)
    // ────────────────────────────────────────────────────────────
    sep("3. 생산 큐 5건 생성");

    struct ProdDef {
        int    orderIdx;
        std::string sampleId, sampleName;
        int    qty;
        bool   inProduction;
    };

    const std::vector<ProdDef> prodDefs = {
        { 9, "S-003", "GaN 기판 2인치",      10, false},  // Waiting
        {10, "S-006", "InP 기판 2인치",        5, false},  // Waiting
        {11, "S-009", "사파이어 기판 4인치",    8, false},  // Waiting
        {12, "S-003", "GaN 기판 2인치",       12, true },  // InProduction
        {13, "S-009", "사파이어 기판 4인치",    6, true },  // InProduction
    };

    for (const auto& pd : prodDefs) {
        auto p = prodMgr.enqueue(
            orderNums[pd.orderIdx], pd.sampleId, pd.sampleName, pd.qty);
        std::string statusStr = "Waiting";
        if (pd.inProduction) {
            prodMgr.start(p.production_id);
            statusStr = "InProduction";
        }
        std::cout << "  + " << p.production_id
                  << "  [" << orderNums[pd.orderIdx] << "]"
                  << "  -> " << statusStr << "\n";
    }

    // ────────────────────────────────────────────────────────────
    // 결과 요약
    // ────────────────────────────────────────────────────────────
    auto allSamples = invMgr.get_all_samples();
    auto allOrders  = ordMgr.get_all();
    auto allQueue   = prodMgr.get_all();

    int reserved=0, confirmed=0, producing=0, release=0, rejected=0;
    for (const auto& o : allOrders) {
        switch (o.status) {
            case OrderStatus::RESERVED:  ++reserved;  break;
            case OrderStatus::CONFIRMED: ++confirmed; break;
            case OrderStatus::PRODUCING: ++producing; break;
            case OrderStatus::RELEASE:   ++release;   break;
            case OrderStatus::REJECTED:  ++rejected;  break;
        }
    }
    int waiting=0, inProd=0;
    for (const auto& p : allQueue) {
        if (p.status == ProductionQueueStatus::Waiting)      ++waiting;
        else if (p.status == ProductionQueueStatus::InProduction) ++inProd;
    }

    std::cout << "\n================================================\n";
    std::cout << "  생성 완료\n";
    std::cout << "------------------------------------------------\n";
    std::cout << "  시료       : " << allSamples.size() << "종\n";
    std::cout << "  주문       : " << allOrders.size()  << "건\n";
    std::cout << "    RESERVED  : " << reserved  << "건\n";
    std::cout << "    CONFIRMED : " << confirmed << "건\n";
    std::cout << "    PRODUCING : " << producing << "건\n";
    std::cout << "    RELEASE   : " << release   << "건\n";
    std::cout << "    REJECTED  : " << rejected  << "건\n";
    std::cout << "  생산 큐    : " << allQueue.size() << "건\n";
    std::cout << "    Waiting      : " << waiting << "건\n";
    std::cout << "    InProduction : " << inProd  << "건\n";
    std::cout << "================================================\n";
    std::cout << "출력 위치: " << dataDir.string() << "\n";
    return 0;
}
