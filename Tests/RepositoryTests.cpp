#include <gtest/gtest.h>

#include <filesystem>

#include "Persistence/DataPersistence.h"
#include "Repositories/InventoryRepository.h"
#include "Repositories/OrderRepository.h"
#include "Repositories/ProductionQueueRepository.h"
#include "TestHelpers.h"

namespace fs = std::filesystem;


// ════════════════════════════════════════════════════════════════
// InventoryRepository 테스트
// ════════════════════════════════════════════════════════════════

class InventoryRepositoryTest : public ::testing::Test {
protected:
    TempDir tmp_;
    InventoryManager        mgr_{ tmp_.file("inventory.json") };
    InventoryRepository     repo_{ mgr_ };
};

TEST_F(InventoryRepositoryTest, AddSample_CacheUpdated) {
    repo_.addSample("S-001", "웨이퍼A", 10.0, 0.9, 50);
    EXPECT_EQ(repo_.findAllSamples().size(), 1u);
    EXPECT_EQ(repo_.findAllInventory().size(), 1u);
}

TEST_F(InventoryRepositoryTest, AddSample_FilePersisted) {
    repo_.addSample("S-001", "웨이퍼A", 10.0, 0.9, 50);
    // 새 Manager+Repository로 파일 재로드
    InventoryManager  mgr2{ tmp_.file("inventory.json") };
    InventoryRepository repo2{ mgr2 };
    ASSERT_EQ(repo2.findAllSamples().size(), 1u);
    EXPECT_EQ(repo2.findAllSamples()[0].sample_name, "웨이퍼A");
    EXPECT_EQ(repo2.findAllInventory()[0].quantity, 50);
}

TEST_F(InventoryRepositoryTest, FindSampleById_FromCache) {
    repo_.addSample("S-001", "웨이퍼A", 10.0, 0.9, 0);
    auto s = repo_.findSampleById("S-001");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->sample_id, "S-001");
}

TEST_F(InventoryRepositoryTest, FindSampleById_NotFound) {
    EXPECT_FALSE(repo_.findSampleById("NONE").has_value());
}

TEST_F(InventoryRepositoryTest, FindSamplesByName_PartialMatch) {
    repo_.addSample("S-001", "실리콘 웨이퍼", 10.0, 0.9, 0);
    repo_.addSample("S-002", "GaN 기판", 20.0, 0.85, 0);
    auto result = repo_.findSamplesByName("웨이퍼");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].sample_id, "S-001");
}

TEST_F(InventoryRepositoryTest, FindSamplesByName_CaseInsensitive) {
    repo_.addSample("S-001", "Silicon Wafer", 10.0, 0.9, 0);
    EXPECT_EQ(repo_.findSamplesByName("SILICON").size(), 1u);
    EXPECT_EQ(repo_.findSamplesByName("wafer").size(), 1u);
}

TEST_F(InventoryRepositoryTest, FindInventoryById_CorrectQuantity) {
    repo_.addSample("S-001", "웨이퍼A", 10.0, 0.9, 30);
    auto inv = repo_.findInventoryById("S-001");
    ASSERT_TRUE(inv.has_value());
    EXPECT_EQ(inv->quantity, 30);
    EXPECT_EQ(inv->unit, "EA");
}

TEST_F(InventoryRepositoryTest, UpdateStock_CacheAndFile) {
    repo_.addSample("S-001", "웨이퍼A", 10.0, 0.9, 50);
    repo_.updateStock("S-001", -20);

    // 캐시 확인
    EXPECT_EQ(repo_.findInventoryById("S-001")->quantity, 30);

    // 파일 확인
    InventoryManager  mgr2{ tmp_.file("inventory.json") };
    InventoryRepository repo2{ mgr2 };
    EXPECT_EQ(repo2.findInventoryById("S-001")->quantity, 30);
}

TEST_F(InventoryRepositoryTest, HasSufficientStock_True) {
    repo_.addSample("S-001", "웨이퍼A", 10.0, 0.9, 50);
    EXPECT_TRUE(repo_.hasSufficientStock("S-001", 50));
    EXPECT_TRUE(repo_.hasSufficientStock("S-001", 30));
}

TEST_F(InventoryRepositoryTest, HasSufficientStock_False) {
    repo_.addSample("S-001", "웨이퍼A", 10.0, 0.9, 10);
    EXPECT_FALSE(repo_.hasSufficientStock("S-001", 11));
    EXPECT_FALSE(repo_.hasSufficientStock("NONE",  1));
}

TEST_F(InventoryRepositoryTest, RemoveSample_RemovedFromBothCaches) {
    repo_.addSample("S-001", "웨이퍼A", 10.0, 0.9, 0);
    repo_.addSample("S-002", "웨이퍼B", 20.0, 0.85, 0);
    repo_.removeSample("S-001");
    EXPECT_EQ(repo_.findAllSamples().size(), 1u);
    EXPECT_EQ(repo_.findAllInventory().size(), 1u);
    EXPECT_FALSE(repo_.findSampleById("S-001").has_value());
}

TEST_F(InventoryRepositoryTest, ConstructorLoadsExistingData) {
    // 파일에 데이터 추가 후 새 Repository 생성 → 캐시 로드 확인
    mgr_.add_sample("S-001", "웨이퍼A", 10.0, 0.9, 70);
    InventoryRepository repo2{ mgr_ };
    EXPECT_EQ(repo2.findAllSamples().size(), 1u);
    EXPECT_EQ(repo2.findInventoryById("S-001")->quantity, 70);
}


// ════════════════════════════════════════════════════════════════
// OrderRepository 테스트
// ════════════════════════════════════════════════════════════════

class OrderRepositoryTest : public ::testing::Test {
protected:
    TempDir          tmp_;
    OrderManager     mgr_{ tmp_.file("orders.json") };
    OrderRepository  repo_{ mgr_ };
};

TEST_F(OrderRepositoryTest, Add_CacheUpdated) {
    repo_.add("S-001", "웨이퍼A", "삼성전자", 10);
    EXPECT_EQ(repo_.findAll().size(), 1u);
}

TEST_F(OrderRepositoryTest, Add_FilePersisted) {
    auto o = repo_.add("S-001", "웨이퍼A", "삼성전자", 10);
    OrderManager     mgr2{ tmp_.file("orders.json") };
    OrderRepository  repo2{ mgr2 };
    ASSERT_EQ(repo2.findAll().size(), 1u);
    EXPECT_EQ(repo2.findByOrderNumber(o.order_number)->status, OrderStatus::RESERVED);
}

TEST_F(OrderRepositoryTest, FindByStatus_FilterWorks) {
    repo_.add("S-001", "웨이퍼A", "고객A", 5);
    repo_.add("S-001", "웨이퍼A", "고객B", 10);
    EXPECT_EQ(repo_.findByStatus(OrderStatus::RESERVED).size(),  2u);
    EXPECT_TRUE(repo_.findByStatus(OrderStatus::CONFIRMED).empty());
}

TEST_F(OrderRepositoryTest, UpdateStatus_CacheUpdated) {
    auto o = repo_.add("S-001", "웨이퍼A", "고객A", 5);
    repo_.updateStatus(o.order_number, OrderStatus::CONFIRMED);
    EXPECT_EQ(repo_.findByOrderNumber(o.order_number)->status, OrderStatus::CONFIRMED);
    EXPECT_EQ(repo_.findByStatus(OrderStatus::CONFIRMED).size(), 1u);
    EXPECT_TRUE(repo_.findByStatus(OrderStatus::RESERVED).empty());
}

TEST_F(OrderRepositoryTest, UpdateStatus_WithNote_FilePersisted) {
    auto o = repo_.add("S-001", "웨이퍼A", "고객A", 5);
    repo_.updateStatus(o.order_number, OrderStatus::REJECTED, "재고 부족");
    OrderManager     mgr2{ tmp_.file("orders.json") };
    OrderRepository  repo2{ mgr2 };
    auto found = repo2.findByOrderNumber(o.order_number);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->status, OrderStatus::REJECTED);
    EXPECT_EQ(found->note,   "재고 부족");
}

TEST_F(OrderRepositoryTest, Remove_CacheUpdated) {
    auto o = repo_.add("S-001", "웨이퍼A", "고객A", 5);
    repo_.remove(o.order_number);
    EXPECT_TRUE(repo_.findAll().empty());
    EXPECT_FALSE(repo_.findByOrderNumber(o.order_number).has_value());
}

TEST_F(OrderRepositoryTest, ConstructorLoadsExistingData) {
    mgr_.add("S-001", "웨이퍼A", "고객A", 5);
    mgr_.add("S-001", "웨이퍼A", "고객B", 10);
    OrderRepository repo2{ mgr_ };
    EXPECT_EQ(repo2.findAll().size(), 2u);
}

TEST_F(OrderRepositoryTest, UpdateStatus_UnknownOrder_ExceptionPropagates) {
    EXPECT_THROW(repo_.updateStatus("NONE", OrderStatus::CONFIRMED), std::out_of_range);
}


// ════════════════════════════════════════════════════════════════
// ProductionQueueRepository 테스트
// ════════════════════════════════════════════════════════════════

class ProductionQueueRepositoryTest : public ::testing::Test {
protected:
    TempDir                    tmp_;
    ProductionQueueManager     mgr_{ tmp_.file("production_queue.json") };
    ProductionQueueRepository  repo_{ mgr_ };
};

TEST_F(ProductionQueueRepositoryTest, Enqueue_CacheUpdated) {
    repo_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    EXPECT_EQ(repo_.findAll().size(), 1u);
}

TEST_F(ProductionQueueRepositoryTest, Enqueue_FilePersisted) {
    auto p = repo_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    ProductionQueueManager     mgr2{ tmp_.file("production_queue.json") };
    ProductionQueueRepository  repo2{ mgr2 };
    ASSERT_TRUE(repo2.findById(p.production_id).has_value());
}

TEST_F(ProductionQueueRepositoryTest, GetWaitingQueue_OnlyWaiting) {
    auto p1 = repo_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    repo_.enqueue("ORD-002", "S-002", "웨이퍼B", 5, 10.0);
    repo_.start(p1.production_id);

    auto waiting = repo_.getWaitingQueue();
    EXPECT_EQ(waiting.size(), 1u);
    EXPECT_EQ(waiting[0].status, ProductionQueueStatus::Waiting);
}

TEST_F(ProductionQueueRepositoryTest, GetFront_FifoOrder) {
    auto p1 = repo_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    repo_.enqueue("ORD-002", "S-002", "웨이퍼B", 5, 10.0);

    // p1이 먼저 등록됐으므로 front여야 함
    auto front = repo_.getFront();
    ASSERT_TRUE(front.has_value());
    EXPECT_EQ(front->production_id, p1.production_id);
}

TEST_F(ProductionQueueRepositoryTest, GetFront_ShiftsAfterDequeue) {
    auto p1 = repo_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    auto p2 = repo_.enqueue("ORD-002", "S-002", "웨이퍼B", 5, 10.0);

    repo_.start(p1.production_id);
    repo_.dequeue(p1.production_id);

    auto front = repo_.getFront();
    ASSERT_TRUE(front.has_value());
    EXPECT_EQ(front->production_id, p2.production_id);
}

TEST_F(ProductionQueueRepositoryTest, GetFront_NullWhenEmpty) {
    EXPECT_FALSE(repo_.getFront().has_value());
}

TEST_F(ProductionQueueRepositoryTest, Start_CacheUpdated) {
    auto p = repo_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    repo_.start(p.production_id);
    auto found = repo_.findById(p.production_id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->status, ProductionQueueStatus::InProduction);
    EXPECT_FALSE(found->started_at.empty());
}

TEST_F(ProductionQueueRepositoryTest, Dequeue_RemovesFromCache_ReturnsItem) {
    auto p = repo_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    repo_.start(p.production_id);
    auto completed = repo_.dequeue(p.production_id);
    EXPECT_EQ(completed.order_number, "ORD-001");
    EXPECT_TRUE(repo_.findAll().empty());
}

TEST_F(ProductionQueueRepositoryTest, Cancel_RemovesFromCache) {
    auto p = repo_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    repo_.cancel(p.production_id);
    EXPECT_TRUE(repo_.findAll().empty());
    EXPECT_FALSE(repo_.findById(p.production_id).has_value());
}

TEST_F(ProductionQueueRepositoryTest, Cancel_InProduction_ExceptionPropagates) {
    auto p = repo_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    repo_.start(p.production_id);
    EXPECT_THROW(repo_.cancel(p.production_id), std::runtime_error);
}

TEST_F(ProductionQueueRepositoryTest, ConstructorLoadsExistingData) {
    mgr_.enqueue("ORD-001", "S-001", "웨이퍼A", 10, 10.0);
    mgr_.enqueue("ORD-002", "S-002", "웨이퍼B", 5, 10.0);
    ProductionQueueRepository repo2{ mgr_ };
    EXPECT_EQ(repo2.findAll().size(), 2u);
    EXPECT_EQ(repo2.getWaitingQueue().size(), 2u);
}
