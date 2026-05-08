#include <gtest/gtest.h>

#include <filesystem>

#include "Persistence/DataPersistence.h"
#include "TestHelpers.h"

namespace fs = std::filesystem;


// ════════════════════════════════════════════════════════════════
// InventoryManager 테스트
// ════════════════════════════════════════════════════════════════

class InventoryManagerTest : public ::testing::Test {
protected:
    TempDir tmp_;
    std::unique_ptr<InventoryManager> mgr_;
    void SetUp() override {
        mgr_ = std::make_unique<InventoryManager>(tmp_.file("inventory.json"));
    }
};

TEST_F(InventoryManagerTest, AddSample_ReturnsCorrectFields) {
    auto s = mgr_->add_sample("S-001", "실리콘 웨이퍼 200mm", 24.5, 0.92, 100);
    EXPECT_EQ(s.sample_id,   "S-001");
    EXPECT_EQ(s.sample_name, "실리콘 웨이퍼 200mm");
    EXPECT_DOUBLE_EQ(s.avg_production_time_hours, 24.5);
    EXPECT_DOUBLE_EQ(s.yield_rate, 0.92);
}

TEST_F(InventoryManagerTest, AddSample_DuplicateId_Throws) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 0);
    EXPECT_THROW(mgr_->add_sample("S-001", "웨이퍼B", 5.0, 0.8, 0),
                 std::invalid_argument);
}

TEST_F(InventoryManagerTest, GetAllSamples_Empty) {
    EXPECT_TRUE(mgr_->get_all_samples().empty());
}

TEST_F(InventoryManagerTest, GetAllSamples_Multiple) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 100);
    mgr_->add_sample("S-002", "웨이퍼B", 20.0, 0.85, 50);
    EXPECT_EQ(mgr_->get_all_samples().size(), 2u);
}

TEST_F(InventoryManagerTest, GetSampleById_Found) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 100);
    auto s = mgr_->get_sample_by_id("S-001");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->sample_id, "S-001");
}

TEST_F(InventoryManagerTest, GetSampleById_NotFound) {
    EXPECT_FALSE(mgr_->get_sample_by_id("NONE").has_value());
}

TEST_F(InventoryManagerTest, FindSamplesByName_PartialMatch) {
    mgr_->add_sample("S-001", "실리콘 웨이퍼 200mm", 10.0, 0.9, 0);
    mgr_->add_sample("S-002", "GaN 기판", 20.0, 0.85, 0);
    auto result = mgr_->find_samples_by_name("웨이퍼");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].sample_id, "S-001");
}

TEST_F(InventoryManagerTest, FindSamplesByName_CaseInsensitive) {
    mgr_->add_sample("S-001", "Silicon Wafer", 10.0, 0.9, 0);
    EXPECT_EQ(mgr_->find_samples_by_name("silicon").size(), 1u);
    EXPECT_EQ(mgr_->find_samples_by_name("SILICON").size(), 1u);
}

TEST_F(InventoryManagerTest, FindSamplesByName_NoMatch) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 0);
    EXPECT_TRUE(mgr_->find_samples_by_name("없는거").empty());
}

TEST_F(InventoryManagerTest, GetInventoryById_CorrectQuantity) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 50);
    auto inv = mgr_->get_inventory_by_id("S-001");
    ASSERT_TRUE(inv.has_value());
    EXPECT_EQ(inv->quantity, 50);
    EXPECT_EQ(inv->unit, "EA");
}

TEST_F(InventoryManagerTest, UpdateStock_Add) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 50);
    auto inv = mgr_->update_stock("S-001", 30);
    EXPECT_EQ(inv.quantity, 80);
}

TEST_F(InventoryManagerTest, UpdateStock_Deduct) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 50);
    auto inv = mgr_->update_stock("S-001", -20);
    EXPECT_EQ(inv.quantity, 30);
}

TEST_F(InventoryManagerTest, UpdateStock_ExactDeduct) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 50);
    auto inv = mgr_->update_stock("S-001", -50);
    EXPECT_EQ(inv.quantity, 0);
}

TEST_F(InventoryManagerTest, UpdateStock_InsufficientStock_Throws) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 10);
    EXPECT_THROW(mgr_->update_stock("S-001", -20), std::runtime_error);
}

TEST_F(InventoryManagerTest, UpdateStock_UnknownId_Throws) {
    EXPECT_THROW(mgr_->update_stock("NONE", 10), std::out_of_range);
}

TEST_F(InventoryManagerTest, RemoveSample) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 0);
    mgr_->remove_sample("S-001");
    EXPECT_FALSE(mgr_->get_sample_by_id("S-001").has_value());
}

TEST_F(InventoryManagerTest, Persistence_ReloadFromFile) {
    mgr_->add_sample("S-001", "웨이퍼A", 10.0, 0.9, 100);
    mgr_->update_stock("S-001", -30);

    InventoryManager mgr2(tmp_.file("inventory.json"));
    auto inv = mgr2.get_inventory_by_id("S-001");
    ASSERT_TRUE(inv.has_value());
    EXPECT_EQ(inv->quantity, 70);
}


// ════════════════════════════════════════════════════════════════
// OrderManager 테스트
// ════════════════════════════════════════════════════════════════

class OrderManagerTest : public ::testing::Test {
protected:
    TempDir tmp_;
    std::unique_ptr<OrderManager> mgr_;
    void SetUp() override {
        mgr_ = std::make_unique<OrderManager>(tmp_.file("orders.json"));
    }
};

TEST_F(OrderManagerTest, Add_ReturnsReservedStatus) {
    auto o = mgr_->add("S-001", "웨이퍼A", "삼성전자", 10);
    EXPECT_EQ(o.sample_id,      "S-001");
    EXPECT_EQ(o.customer_name,  "삼성전자");
    EXPECT_EQ(o.order_quantity, 10);
    EXPECT_EQ(o.status,         OrderStatus::RESERVED);
}

TEST_F(OrderManagerTest, Add_OrderNumberFormat) {
    auto o = mgr_->add("S-001", "웨이퍼A", "고객A", 5);
    // ORD-YYYYMMDD-XXXX = 4+8+1+4 = 17자
    EXPECT_EQ(o.order_number.substr(0, 4), "ORD-");
    EXPECT_EQ(o.order_number.size(), 17u);
}

TEST_F(OrderManagerTest, Add_SequentialNumbering) {
    auto o1 = mgr_->add("S-001", "웨이퍼A", "고객A", 5);
    auto o2 = mgr_->add("S-001", "웨이퍼A", "고객B", 10);
    EXPECT_NE(o1.order_number, o2.order_number);
}

TEST_F(OrderManagerTest, GetAll_Empty) {
    EXPECT_TRUE(mgr_->get_all().empty());
}

TEST_F(OrderManagerTest, GetByOrderNumber_Found) {
    auto o     = mgr_->add("S-001", "웨이퍼A", "고객A", 5);
    auto found = mgr_->get_by_order_number(o.order_number);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->order_number, o.order_number);
}

TEST_F(OrderManagerTest, GetByOrderNumber_NotFound) {
    EXPECT_FALSE(mgr_->get_by_order_number("NONE").has_value());
}

TEST_F(OrderManagerTest, GetByStatus) {
    mgr_->add("S-001", "웨이퍼A", "고객A", 5);
    mgr_->add("S-001", "웨이퍼A", "고객B", 10);
    EXPECT_EQ(mgr_->get_by_status(OrderStatus::RESERVED).size(),  2u);
    EXPECT_TRUE(mgr_->get_by_status(OrderStatus::CONFIRMED).empty());
}

TEST_F(OrderManagerTest, UpdateStatus_ChangesStatus) {
    auto o       = mgr_->add("S-001", "웨이퍼A", "고객A", 5);
    auto updated = mgr_->update_status(o.order_number, OrderStatus::CONFIRMED);
    EXPECT_EQ(updated.status, OrderStatus::CONFIRMED);
}

TEST_F(OrderManagerTest, UpdateStatus_WithNote) {
    auto o       = mgr_->add("S-001", "웨이퍼A", "고객A", 5);
    auto updated = mgr_->update_status(o.order_number, OrderStatus::REJECTED, "재고 부족");
    EXPECT_EQ(updated.status, OrderStatus::REJECTED);
    EXPECT_EQ(updated.note,   "재고 부족");
}

TEST_F(OrderManagerTest, UpdateStatus_UnknownOrder_Throws) {
    EXPECT_THROW(mgr_->update_status("NONE", OrderStatus::CONFIRMED), std::out_of_range);
}

TEST_F(OrderManagerTest, Remove_DeletesOrder) {
    auto o = mgr_->add("S-001", "웨이퍼A", "고객A", 5);
    mgr_->remove(o.order_number);
    EXPECT_FALSE(mgr_->get_by_order_number(o.order_number).has_value());
}

TEST_F(OrderManagerTest, Persistence_StatusSurvivesReload) {
    auto o = mgr_->add("S-001", "웨이퍼A", "고객A", 5);
    mgr_->update_status(o.order_number, OrderStatus::CONFIRMED);

    OrderManager mgr2(tmp_.file("orders.json"));
    auto found = mgr2.get_by_order_number(o.order_number);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->status, OrderStatus::CONFIRMED);
}


// ════════════════════════════════════════════════════════════════
// ProductionQueueManager 테스트
// ════════════════════════════════════════════════════════════════

class ProductionQueueManagerTest : public ::testing::Test {
protected:
    TempDir tmp_;
    std::unique_ptr<ProductionQueueManager> mgr_;
    void SetUp() override {
        mgr_ = std::make_unique<ProductionQueueManager>(
                    tmp_.file("production_queue.json"));
    }
};

TEST_F(ProductionQueueManagerTest, Enqueue_ReturnsWaitingItem) {
    auto p = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    EXPECT_FALSE(p.production_id.empty());
    EXPECT_EQ(p.order_number,  "ORD-001");
    EXPECT_EQ(p.status,        ProductionQueueStatus::Waiting);
    EXPECT_TRUE(p.started_at.empty());
}

TEST_F(ProductionQueueManagerTest, Enqueue_ProductionIdFormat) {
    auto p = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    EXPECT_EQ(p.production_id.substr(0, 5), "PROD-");
    EXPECT_EQ(p.production_id.size(), 18u); // PROD-YYYYMMDD-XXXX = 5+8+1+4
}

TEST_F(ProductionQueueManagerTest, Enqueue_SequentialIds) {
    auto p1 = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    auto p2 = mgr_->enqueue("ORD-002", "S-002", "웨이퍼B", 5);
    EXPECT_NE(p1.production_id, p2.production_id);
}

TEST_F(ProductionQueueManagerTest, GetAll_Empty) {
    EXPECT_TRUE(mgr_->get_all().empty());
}

TEST_F(ProductionQueueManagerTest, GetFront_ReturnsFirst_ThenShiftsAfterComplete) {
    auto p1 = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    // p1이 front여야 함
    auto front1 = mgr_->get_front();
    ASSERT_TRUE(front1.has_value());
    EXPECT_EQ(front1->production_id, p1.production_id);

    auto p2 = mgr_->enqueue("ORD-002", "S-002", "웨이퍼B", 5);
    // p1 처리 후 p2가 front가 되어야 함
    mgr_->start(p1.production_id);
    mgr_->complete(p1.production_id);

    auto front2 = mgr_->get_front();
    ASSERT_TRUE(front2.has_value());
    EXPECT_EQ(front2->production_id, p2.production_id);
}

TEST_F(ProductionQueueManagerTest, GetFront_NoneWhenEmpty) {
    EXPECT_FALSE(mgr_->get_front().has_value());
}

TEST_F(ProductionQueueManagerTest, Start_WaitingToInProduction) {
    auto p       = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    auto started = mgr_->start(p.production_id);
    EXPECT_EQ(started.status, ProductionQueueStatus::InProduction);
    EXPECT_FALSE(started.started_at.empty());
}

TEST_F(ProductionQueueManagerTest, Start_AlreadyInProduction_Throws) {
    auto p = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    mgr_->start(p.production_id);
    EXPECT_THROW(mgr_->start(p.production_id), std::runtime_error);
}

TEST_F(ProductionQueueManagerTest, Start_UnknownId_Throws) {
    EXPECT_THROW(mgr_->start("NONE"), std::out_of_range);
}

TEST_F(ProductionQueueManagerTest, Complete_RemovesFromQueue_ReturnsItem) {
    auto p = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    mgr_->start(p.production_id);
    auto completed = mgr_->complete(p.production_id);
    EXPECT_EQ(completed.order_number, "ORD-001");
    EXPECT_TRUE(mgr_->get_all().empty());
}

TEST_F(ProductionQueueManagerTest, Complete_NotInProduction_Throws) {
    auto p = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    EXPECT_THROW(mgr_->complete(p.production_id), std::runtime_error);
}

TEST_F(ProductionQueueManagerTest, Cancel_WaitingItem) {
    auto p = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    mgr_->cancel(p.production_id);
    EXPECT_TRUE(mgr_->get_all().empty());
}

TEST_F(ProductionQueueManagerTest, Cancel_InProduction_Throws) {
    auto p = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    mgr_->start(p.production_id);
    EXPECT_THROW(mgr_->cancel(p.production_id), std::runtime_error);
}

TEST_F(ProductionQueueManagerTest, GetByStatus_FilterWaiting) {
    auto p1 = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    mgr_->enqueue("ORD-002", "S-002", "웨이퍼B", 5);
    mgr_->start(p1.production_id);

    EXPECT_EQ(mgr_->get_by_status(ProductionQueueStatus::Waiting).size(),      1u);
    EXPECT_EQ(mgr_->get_by_status(ProductionQueueStatus::InProduction).size(), 1u);
}

TEST_F(ProductionQueueManagerTest, Persistence_StatusSurvivesReload) {
    auto p = mgr_->enqueue("ORD-001", "S-001", "웨이퍼A", 10);
    mgr_->start(p.production_id);

    ProductionQueueManager mgr2(tmp_.file("production_queue.json"));
    auto found = mgr2.get_by_id(p.production_id);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->status, ProductionQueueStatus::InProduction);
    EXPECT_FALSE(found->started_at.empty());
}
