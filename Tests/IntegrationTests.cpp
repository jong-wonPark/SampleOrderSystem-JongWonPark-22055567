#include <gtest/gtest.h>

#include <filesystem>

#include "Persistence/DataPersistence.h"
#include "Repositories/InventoryRepository.h"
#include "Repositories/OrderRepository.h"
#include "Repositories/ProductionQueueRepository.h"
#include "Services/InventoryService.h"
#include "Services/ProductionService.h"
#include "Services/OrderService.h"
#include "TestHelpers.h"

namespace fs = std::filesystem;

// ── 전체 스택 헬퍼 ─────────────────────────────────────────────────

struct Stack {
    InventoryManager          invMgr;
    OrderManager              ordMgr;
    ProductionQueueManager    prodMgr;
    InventoryRepository       invRepo;
    OrderRepository           ordRepo;
    ProductionQueueRepository queueRepo;
    InventoryService          invSvc;
    ProductionService         prodSvc;
    OrderService              ordSvc;

    explicit Stack(const TempDir& tmp)
        : invMgr   (tmp.file("inventory.json"))
        , ordMgr   (tmp.file("orders.json"))
        , prodMgr  (tmp.file("production_queue.json"))
        , invRepo  (invMgr)
        , ordRepo  (ordMgr)
        , queueRepo(prodMgr)
        , invSvc   (invRepo)
        , prodSvc  (queueRepo, ordRepo, invSvc)
        , ordSvc   (ordRepo, invSvc, prodSvc)
    {}
};

// ════════════════════════════════════════════════════════════════
// 시나리오 A: 재고 있음 → CONFIRMED → RELEASE
// ════════════════════════════════════════════════════════════════

TEST(IntegrationScenarioA, StockSufficient_FullFlow) {
    TempDir tmp;
    Stack s(tmp);

    // 1. 시료 등록 (재고 50)
    s.invSvc.registerSample("S-001", "실리콘 웨이퍼", 24.0, 0.92, 50);
    ASSERT_EQ(s.invSvc.getStock("S-001")->quantity, 50);

    // 2. 주문 접수 → RESERVED
    auto o = s.ordSvc.placeOrder("S-001", "실리콘 웨이퍼", "삼성전자", 10);
    EXPECT_EQ(o.status, OrderStatus::RESERVED);

    // 3. 승인 → 재고 충분 → CONFIRMED, 재고 차감
    EXPECT_TRUE(s.ordSvc.approveOrder(o.order_number));
    EXPECT_EQ(s.ordSvc.findByOrderNumber(o.order_number)->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(s.invSvc.getStock("S-001")->quantity, 40);

    // 4. 출고 → RELEASE
    EXPECT_TRUE(s.prodSvc.shipOrder(o.order_number));
    EXPECT_EQ(s.ordSvc.findByOrderNumber(o.order_number)->status, OrderStatus::RELEASE);

    // 5. 생산 큐는 비어 있어야 함
    EXPECT_TRUE(s.prodSvc.getQueue().empty());
}

TEST(IntegrationScenarioA, Persistence_SurvivesRestart) {
    TempDir tmp;
    std::string orderNum;
    {
        Stack s(tmp);
        s.invSvc.registerSample("S-001", "실리콘 웨이퍼", 24.0, 0.92, 50);
        auto o = s.ordSvc.placeOrder("S-001", "실리콘 웨이퍼", "삼성전자", 10);
        orderNum = o.order_number;
        s.ordSvc.approveOrder(o.order_number);
        s.prodSvc.shipOrder(o.order_number);
    }
    // 새 인스턴스로 재시작 시뮬레이션
    Stack s2(tmp);
    auto order = s2.ordSvc.findByOrderNumber(orderNum);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::RELEASE);
    EXPECT_EQ(s2.invSvc.getStock("S-001")->quantity, 40);
}

// ════════════════════════════════════════════════════════════════
// 시나리오 B: 재고 없음 → PRODUCING → 생산완료 → CONFIRMED → RELEASE
// ════════════════════════════════════════════════════════════════

TEST(IntegrationScenarioB, NoStock_ProductionFlow) {
    TempDir tmp;
    Stack s(tmp);

    // 1. 시료 등록 (재고 0)
    s.invSvc.registerSample("S-002", "GaN 기판", 10.0, 0.85, 0);

    // 2. 주문 접수 → RESERVED
    auto o = s.ordSvc.placeOrder("S-002", "GaN 기판", "SK하이닉스", 10);
    EXPECT_EQ(o.status, OrderStatus::RESERVED);

    // 3. 승인 → 재고 부족 → PRODUCING + 생산 큐 등록
    EXPECT_TRUE(s.ordSvc.approveOrder(o.order_number));
    EXPECT_EQ(s.ordSvc.findByOrderNumber(o.order_number)->status, OrderStatus::PRODUCING);
    EXPECT_EQ(s.prodSvc.getQueue().size(), 1u);
    EXPECT_EQ(s.prodSvc.getQueue()[0].status, ProductionQueueStatus::Waiting);

    // 4. 생산 시작 → InProduction
    EXPECT_TRUE(s.prodSvc.startNextProduction());
    auto cur = s.prodSvc.getCurrentProduction();
    ASSERT_TRUE(cur.has_value());
    EXPECT_EQ(cur->status, ProductionQueueStatus::InProduction);

    // 5. 생산 완료 → CONFIRMED
    // planned=ceil(10/(0.85*0.9))=14, addStock(14)+deductStock(10) → 잔여 4
    EXPECT_TRUE(s.prodSvc.completeProduction(cur->production_id));
    EXPECT_EQ(s.ordSvc.findByOrderNumber(o.order_number)->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(s.invSvc.getStock("S-002")->quantity, 4);
    EXPECT_TRUE(s.prodSvc.getQueue().empty());

    // 6. 출고 → RELEASE
    EXPECT_TRUE(s.prodSvc.shipOrder(o.order_number));
    EXPECT_EQ(s.ordSvc.findByOrderNumber(o.order_number)->status, OrderStatus::RELEASE);
}

TEST(IntegrationScenarioB, Persistence_SurvivesRestartAtEachStep) {
    TempDir tmp;

    // STEP 1: 주문 접수 후 재시작
    std::string orderNum, prodId;
    {
        Stack s(tmp);
        s.invSvc.registerSample("S-002", "GaN 기판", 10.0, 0.85, 0);
        auto o = s.ordSvc.placeOrder("S-002", "GaN 기판", "SK하이닉스", 10);
        orderNum = o.order_number;
        s.ordSvc.approveOrder(o.order_number);
    }
    {
        Stack s2(tmp);
        EXPECT_EQ(s2.ordSvc.findByOrderNumber(orderNum)->status, OrderStatus::PRODUCING);
        EXPECT_EQ(s2.prodSvc.getQueue().size(), 1u);

        // STEP 2: 생산 시작 후 재시작
        s2.prodSvc.startNextProduction();
        prodId = s2.prodSvc.getCurrentProduction()->production_id;
    }
    {
        Stack s3(tmp);
        auto prod = s3.prodSvc.getCurrentProduction();
        ASSERT_TRUE(prod.has_value());
        EXPECT_EQ(prod->status, ProductionQueueStatus::InProduction);

        // STEP 3: 생산 완료 → CONFIRMED
        s3.prodSvc.completeProduction(prod->production_id);
    }
    {
        Stack s4(tmp);
        EXPECT_EQ(s4.ordSvc.findByOrderNumber(orderNum)->status, OrderStatus::CONFIRMED);
        EXPECT_TRUE(s4.prodSvc.getQueue().empty());

        // STEP 4: 출고 → RELEASE
        s4.prodSvc.shipOrder(orderNum);
    }
    {
        Stack s5(tmp);
        EXPECT_EQ(s5.ordSvc.findByOrderNumber(orderNum)->status, OrderStatus::RELEASE);
    }
}

// ════════════════════════════════════════════════════════════════
// 시나리오 C: 주문 거절 → REJECTED
// ════════════════════════════════════════════════════════════════

TEST(IntegrationScenarioC, RejectOrder_REJECTED) {
    TempDir tmp;
    Stack s(tmp);

    auto o = s.ordSvc.placeOrder("S-001", "웨이퍼A", "삼성전자", 10);
    EXPECT_TRUE(s.ordSvc.rejectOrder(o.order_number, "수량 초과"));

    auto updated = s.ordSvc.findByOrderNumber(o.order_number);
    EXPECT_EQ(updated->status, OrderStatus::REJECTED);
    EXPECT_EQ(updated->note,   "수량 초과");

    // 생산 큐와 재고에 영향 없음
    EXPECT_TRUE(s.prodSvc.getQueue().empty());
}

TEST(IntegrationScenarioC, Persistence_RejectedNotePreserved) {
    TempDir tmp;
    std::string orderNum;
    {
        Stack s(tmp);
        auto o = s.ordSvc.placeOrder("S-001", "웨이퍼A", "삼성전자", 10);
        orderNum = o.order_number;
        s.ordSvc.rejectOrder(o.order_number, "재고 부족");
    }
    Stack s2(tmp);
    auto order = s2.ordSvc.findByOrderNumber(orderNum);
    ASSERT_TRUE(order.has_value());
    EXPECT_EQ(order->status, OrderStatus::REJECTED);
    EXPECT_EQ(order->note, "재고 부족");
}

// ════════════════════════════════════════════════════════════════
// 생산 큐 FIFO 순서 검증
// ════════════════════════════════════════════════════════════════

TEST(IntegrationFIFO, MultipleOrders_ProcessedInOrder) {
    TempDir tmp;
    Stack s(tmp);

    s.invSvc.registerSample("S-001", "웨이퍼A", 10.0, 0.9, 0);

    // 3개 주문 → 모두 재고 없음 → PRODUCING (FIFO 큐)
    auto o1 = s.ordSvc.placeOrder("S-001", "웨이퍼A", "고객A", 5);
    auto o2 = s.ordSvc.placeOrder("S-001", "웨이퍼A", "고객B", 3);
    auto o3 = s.ordSvc.placeOrder("S-001", "웨이퍼A", "고객C", 7);
    s.ordSvc.approveOrder(o1.order_number);
    s.ordSvc.approveOrder(o2.order_number);
    s.ordSvc.approveOrder(o3.order_number);

    EXPECT_EQ(s.prodSvc.getQueue().size(), 3u);

    // FIFO: o1 → o2 → o3 순서로 처리
    for (const auto& [orderNum, qty] : std::vector<std::pair<std::string,int>>{
            {o1.order_number, 5}, {o2.order_number, 3}, {o3.order_number, 7}})
    {
        auto front = s.prodSvc.getQueue().front();
        EXPECT_EQ(front.order_number, orderNum);
        s.prodSvc.startNextProduction();
        auto cur = s.prodSvc.getCurrentProduction();
        ASSERT_TRUE(cur.has_value());
        s.prodSvc.completeProduction(cur->production_id);
        EXPECT_EQ(s.ordSvc.findByOrderNumber(orderNum)->status, OrderStatus::CONFIRMED);
    }
    EXPECT_TRUE(s.prodSvc.getQueue().empty());
}

// ════════════════════════════════════════════════════════════════
// 혼합 시나리오: 재고있음 + 재고없음 동시 처리
// ════════════════════════════════════════════════════════════════

TEST(IntegrationMixed, StockAndNoStock_BothHandled) {
    TempDir tmp;
    Stack s(tmp);

    s.invSvc.registerSample("S-001", "웨이퍼A", 10.0, 0.9, 10); // 재고 10
    s.invSvc.registerSample("S-002", "GaN 기판", 20.0, 0.85, 0); // 재고 0

    // 주문 2건 접수
    auto oA = s.ordSvc.placeOrder("S-001", "웨이퍼A", "고객A",  5); // 재고 충분
    auto oB = s.ordSvc.placeOrder("S-002", "GaN 기판", "고객B", 3); // 재고 부족

    // 승인
    s.ordSvc.approveOrder(oA.order_number); // → CONFIRMED
    s.ordSvc.approveOrder(oB.order_number); // → PRODUCING

    EXPECT_EQ(s.ordSvc.findByOrderNumber(oA.order_number)->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(s.ordSvc.findByOrderNumber(oB.order_number)->status, OrderStatus::PRODUCING);
    EXPECT_EQ(s.invSvc.getStock("S-001")->quantity, 5); // 차감됨
    EXPECT_EQ(s.invSvc.getStock("S-002")->quantity, 0); // 미차감

    // oA 출고
    EXPECT_TRUE(s.prodSvc.shipOrder(oA.order_number));
    EXPECT_EQ(s.ordSvc.findByOrderNumber(oA.order_number)->status, OrderStatus::RELEASE);

    // oB 생산 처리
    s.prodSvc.startNextProduction();
    auto cur = s.prodSvc.getCurrentProduction();
    ASSERT_TRUE(cur.has_value());
    s.prodSvc.completeProduction(cur->production_id);
    EXPECT_EQ(s.ordSvc.findByOrderNumber(oB.order_number)->status, OrderStatus::CONFIRMED);

    // oB 출고
    EXPECT_TRUE(s.prodSvc.shipOrder(oB.order_number));
    EXPECT_EQ(s.ordSvc.findByOrderNumber(oB.order_number)->status, OrderStatus::RELEASE);
}

// ════════════════════════════════════════════════════════════════
// 모니터링 집계 검증
// ════════════════════════════════════════════════════════════════

TEST(IntegrationMonitoring, StatusCountsCorrect) {
    TempDir tmp;
    Stack s(tmp);

    s.invSvc.registerSample("S-001", "웨이퍼A", 10.0, 0.9, 100);
    s.invSvc.registerSample("S-002", "GaN 기판", 20.0, 0.85, 0);

    // 주문 5건: 2 RESERVED, 1 CONFIRMED, 1 PRODUCING, 1 REJECTED
    auto o1 = s.ordSvc.placeOrder("S-001", "웨이퍼A", "고객A", 5);  // → RESERVED
    auto o2 = s.ordSvc.placeOrder("S-001", "웨이퍼A", "고객B", 5);  // → RESERVED
    auto o3 = s.ordSvc.placeOrder("S-001", "웨이퍼A", "고객C", 5);  // → CONFIRMED
    auto o4 = s.ordSvc.placeOrder("S-002", "GaN 기판", "고객D", 5); // → PRODUCING
    auto o5 = s.ordSvc.placeOrder("S-001", "웨이퍼A", "고객E", 5);  // → REJECTED

    s.ordSvc.approveOrder(o3.order_number);
    s.ordSvc.approveOrder(o4.order_number);
    s.ordSvc.rejectOrder(o5.order_number, "테스트");

    auto all = s.ordSvc.getAllOrders();
    EXPECT_EQ(all.size(), 5u);

    int reserved = 0, confirmed = 0, producing = 0, rejected = 0;
    for (const auto& o : all) {
        switch (o.status) {
            case OrderStatus::RESERVED:  ++reserved;  break;
            case OrderStatus::CONFIRMED: ++confirmed; break;
            case OrderStatus::PRODUCING: ++producing; break;
            case OrderStatus::REJECTED:  ++rejected;  break;
            default: break;
        }
    }
    EXPECT_EQ(reserved,  2);
    EXPECT_EQ(confirmed, 1);
    EXPECT_EQ(producing, 1);
    EXPECT_EQ(rejected,  1);
}
