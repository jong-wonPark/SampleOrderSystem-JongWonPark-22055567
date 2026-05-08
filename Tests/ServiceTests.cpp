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

// ── 전체 서비스 체인 공통 Fixture ────────────────────────────────

struct ServiceFixture : public ::testing::Test {
    TempDir                   tmp_;
    InventoryManager          invMgr_   { tmp_.file("inventory.json") };
    OrderManager              ordMgr_   { tmp_.file("orders.json") };
    ProductionQueueManager    prodMgr_  { tmp_.file("production_queue.json") };
    InventoryRepository       invRepo_  { invMgr_ };
    OrderRepository           ordRepo_  { ordMgr_ };
    ProductionQueueRepository queueRepo_{ prodMgr_ };
    InventoryService          invSvc_   { invRepo_ };
    ProductionService         prodSvc_  { queueRepo_, ordRepo_, invSvc_ };
    OrderService              ordSvc_   { ordRepo_, invSvc_, prodSvc_ };

    // 시료 등록 헬퍼
    void addSample(const std::string& id, int qty) {
        invSvc_.registerSample(id, id + "_name", 10.0, 0.9, qty);
    }
};


// ════════════════════════════════════════════════════════════════
// InventoryService 테스트
// ════════════════════════════════════════════════════════════════

class InventoryServiceTest : public ServiceFixture {};

TEST_F(InventoryServiceTest, RegisterSample_CanBeFound) {
    invSvc_.registerSample("S-001", "웨이퍼A", 10.0, 0.9, 50);
    auto s = invSvc_.findSampleById("S-001");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->sample_name, "웨이퍼A");
}

TEST_F(InventoryServiceTest, GetAllSamples_ReturnsAll) {
    invSvc_.registerSample("S-001", "웨이퍼A", 10.0, 0.9, 0);
    invSvc_.registerSample("S-002", "웨이퍼B", 20.0, 0.85, 0);
    EXPECT_EQ(invSvc_.getAllSamples().size(), 2u);
}

TEST_F(InventoryServiceTest, FindSamplesByName_PartialMatch) {
    invSvc_.registerSample("S-001", "실리콘 웨이퍼", 10.0, 0.9, 0);
    invSvc_.registerSample("S-002", "GaN 기판", 20.0, 0.85, 0);
    auto result = invSvc_.findSamplesByName("웨이퍼");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].sample_id, "S-001");
}

TEST_F(InventoryServiceTest, GetStock_Found) {
    addSample("S-001", 30);
    auto inv = invSvc_.getStock("S-001");
    ASSERT_TRUE(inv.has_value());
    EXPECT_EQ(inv->quantity, 30);
}

TEST_F(InventoryServiceTest, GetStock_NotFound) {
    EXPECT_FALSE(invSvc_.getStock("NONE").has_value());
}

TEST_F(InventoryServiceTest, HasEnoughStock_True) {
    addSample("S-001", 50);
    EXPECT_TRUE(invSvc_.hasEnoughStock("S-001", 50));
    EXPECT_TRUE(invSvc_.hasEnoughStock("S-001", 30));
}

TEST_F(InventoryServiceTest, HasEnoughStock_False) {
    addSample("S-001", 10);
    EXPECT_FALSE(invSvc_.hasEnoughStock("S-001", 11));
    EXPECT_FALSE(invSvc_.hasEnoughStock("NONE", 1));
}

TEST_F(InventoryServiceTest, DeductStock_Success_ReturnsTrue) {
    addSample("S-001", 50);
    EXPECT_TRUE(invSvc_.deductStock("S-001", 20));
    EXPECT_EQ(invSvc_.getStock("S-001")->quantity, 30);
}

TEST_F(InventoryServiceTest, DeductStock_Insufficient_ReturnsFalse_NoThrow) {
    addSample("S-001", 10);
    EXPECT_FALSE(invSvc_.deductStock("S-001", 20));
    EXPECT_EQ(invSvc_.getStock("S-001")->quantity, 10);  // 변화 없음
}

TEST_F(InventoryServiceTest, AddStock_IncreasesQuantity) {
    addSample("S-001", 10);
    EXPECT_TRUE(invSvc_.addStock("S-001", 30));
    EXPECT_EQ(invSvc_.getStock("S-001")->quantity, 40);
}


// ════════════════════════════════════════════════════════════════
// OrderService 테스트
// ════════════════════════════════════════════════════════════════

class OrderServiceTest : public ServiceFixture {};

TEST_F(OrderServiceTest, PlaceOrder_CreatesReservedOrder) {
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "삼성전자", 10);
    EXPECT_EQ(o.status,         OrderStatus::RESERVED);
    EXPECT_EQ(o.customer_name,  "삼성전자");
    EXPECT_EQ(o.order_quantity, 10);
}

TEST_F(OrderServiceTest, ApproveOrder_StockSufficient_Confirmed) {
    addSample("S-001", 50);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);

    EXPECT_TRUE(ordSvc_.approveOrder(o.order_number));
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::CONFIRMED);
    // 재고 차감 확인
    EXPECT_EQ(invSvc_.getStock("S-001")->quantity, 40);
}

TEST_F(OrderServiceTest, ApproveOrder_StockInsufficient_Producing) {
    addSample("S-001", 5);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);

    EXPECT_TRUE(ordSvc_.approveOrder(o.order_number));
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::PRODUCING);
    // 생산 큐 등록 확인
    EXPECT_EQ(queueRepo_.getWaitingQueue().size(), 1u);
    // 재고 미차감 확인
    EXPECT_EQ(invSvc_.getStock("S-001")->quantity, 5);
}

TEST_F(OrderServiceTest, ApproveOrder_ZeroStock_Producing) {
    addSample("S-001", 0);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    EXPECT_TRUE(ordSvc_.approveOrder(o.order_number));
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::PRODUCING);
}

TEST_F(OrderServiceTest, ApproveOrder_NotReserved_ReturnsFalse) {
    addSample("S-001", 50);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    ordSvc_.approveOrder(o.order_number);          // → CONFIRMED
    EXPECT_FALSE(ordSvc_.approveOrder(o.order_number)); // 재승인 시도
}

TEST_F(OrderServiceTest, ApproveOrder_UnknownOrder_ReturnsFalse) {
    EXPECT_FALSE(ordSvc_.approveOrder("NONE"));
}

TEST_F(OrderServiceTest, RejectOrder_REJECTED) {
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    EXPECT_TRUE(ordSvc_.rejectOrder(o.order_number, "재고 부족"));
    auto updated = ordSvc_.findByOrderNumber(o.order_number);
    EXPECT_EQ(updated->status, OrderStatus::REJECTED);
    EXPECT_EQ(updated->note,   "재고 부족");
}

TEST_F(OrderServiceTest, RejectOrder_NotReserved_ReturnsFalse) {
    addSample("S-001", 50);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    ordSvc_.approveOrder(o.order_number);          // → CONFIRMED
    EXPECT_FALSE(ordSvc_.rejectOrder(o.order_number));
}

TEST_F(OrderServiceTest, GetOrdersByStatus_FilterWorks) {
    addSample("S-001", 100);
    ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);        // RESERVED
    auto o2 = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객B", 10);
    ordSvc_.approveOrder(o2.order_number);                       // CONFIRMED
    EXPECT_EQ(ordSvc_.getOrdersByStatus(OrderStatus::RESERVED).size(),  1u);
    EXPECT_EQ(ordSvc_.getOrdersByStatus(OrderStatus::CONFIRMED).size(), 1u);
}


// ════════════════════════════════════════════════════════════════
// ProductionService 테스트
// ════════════════════════════════════════════════════════════════

class ProductionServiceTest : public ServiceFixture {};

TEST_F(ProductionServiceTest, StartNextProduction_WaitingToInProduction) {
    addSample("S-001", 0);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    ordSvc_.approveOrder(o.order_number);   // → PRODUCING + 큐 등록

    EXPECT_TRUE(prodSvc_.startNextProduction());
    auto cur = prodSvc_.getCurrentProduction();
    ASSERT_TRUE(cur.has_value());
    EXPECT_EQ(cur->status, ProductionQueueStatus::InProduction);
}

TEST_F(ProductionServiceTest, StartNextProduction_EmptyQueue_ReturnsFalse) {
    EXPECT_FALSE(prodSvc_.startNextProduction());
}

TEST_F(ProductionServiceTest, CompleteProduction_OrderBecomesConfirmed) {
    addSample("S-001", 0);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    ordSvc_.approveOrder(o.order_number);
    prodSvc_.startNextProduction();

    auto prod = prodSvc_.getCurrentProduction();
    ASSERT_TRUE(prod.has_value());
    EXPECT_TRUE(prodSvc_.completeProduction(prod->production_id));

    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::CONFIRMED);
    EXPECT_TRUE(prodSvc_.getQueue().empty());
}

TEST_F(ProductionServiceTest, CompleteProduction_StockNetZero) {
    addSample("S-001", 0);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    ordSvc_.approveOrder(o.order_number);
    prodSvc_.startNextProduction();
    auto prod = prodSvc_.getCurrentProduction();
    prodSvc_.completeProduction(prod->production_id);
    // planned=ceil(10/(0.9*0.9))=13, addStock(13)+deductStock(10) → 잔여 3
    EXPECT_EQ(invSvc_.getStock("S-001")->quantity, 3);
}

TEST_F(ProductionServiceTest, CompleteProduction_NotInProduction_ReturnsFalse) {
    addSample("S-001", 0);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    ordSvc_.approveOrder(o.order_number);  // Waiting 상태
    auto front = queueRepo_.getFront();
    EXPECT_FALSE(prodSvc_.completeProduction(front->production_id));
}

TEST_F(ProductionServiceTest, ShipOrder_CONFIRMED_ToRELEASE) {
    addSample("S-001", 50);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    ordSvc_.approveOrder(o.order_number);   // → CONFIRMED (재고 충분)

    EXPECT_TRUE(prodSvc_.shipOrder(o.order_number));
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::RELEASE);
}

TEST_F(ProductionServiceTest, ShipOrder_NotConfirmed_ReturnsFalse) {
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    EXPECT_FALSE(prodSvc_.shipOrder(o.order_number));  // RESERVED 상태
}

TEST_F(ProductionServiceTest, ShipOrder_UnknownOrder_ReturnsFalse) {
    EXPECT_FALSE(prodSvc_.shipOrder("NONE"));
}

TEST_F(ProductionServiceTest, FullScenario_StockSufficient) {
    // 시나리오 A: 재고 있음 → 즉시 출고 대기
    addSample("S-001", 50);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    EXPECT_EQ(o.status, OrderStatus::RESERVED);

    ordSvc_.approveOrder(o.order_number);
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(invSvc_.getStock("S-001")->quantity, 40);

    prodSvc_.shipOrder(o.order_number);
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::RELEASE);
}

TEST_F(ProductionServiceTest, FullScenario_NoStock) {
    // 시나리오 B: 재고 없음 → 생산 → 출고
    addSample("S-001", 0);
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    EXPECT_EQ(o.status, OrderStatus::RESERVED);

    ordSvc_.approveOrder(o.order_number);
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::PRODUCING);

    prodSvc_.startNextProduction();
    auto prod = prodSvc_.getCurrentProduction();
    ASSERT_TRUE(prod.has_value());

    prodSvc_.completeProduction(prod->production_id);
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::CONFIRMED);

    prodSvc_.shipOrder(o.order_number);
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::RELEASE);
}

TEST_F(ProductionServiceTest, FullScenario_Reject) {
    // 시나리오 C: 거절
    auto o = ordSvc_.placeOrder("S-001", "웨이퍼A", "고객A", 10);
    ordSvc_.rejectOrder(o.order_number, "수량 초과");
    EXPECT_EQ(ordSvc_.findByOrderNumber(o.order_number)->status, OrderStatus::REJECTED);
    EXPECT_TRUE(prodSvc_.getQueue().empty());
}
