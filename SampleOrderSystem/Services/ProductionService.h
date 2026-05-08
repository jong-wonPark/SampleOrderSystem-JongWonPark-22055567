#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../Models/ProductionQueueItem.h"
#include "../Repositories/ProductionQueueRepository.h"
#include "../Repositories/OrderRepository.h"
#include "InventoryService.h"

class ProductionService {
public:
    ProductionService(ProductionQueueRepository& queueRepo,
                      OrderRepository&           orderRepo,
                      InventoryService&          inventoryService);

    // OrderService::approveOrder 내부에서 호출 (재고 부족 경로)
    ProductionQueueItem enqueueProduction(const std::string& order_number,
                                          const std::string& sample_id,
                                          const std::string& sample_name,
                                          int                planned_quantity,
                                          double             total_production_time_hours);

    // 메뉴 6: 큐 front(Waiting) → InProduction (대기 항목 없으면 false)
    bool startNextProduction();

    // 메뉴 6: InProduction 완료
    //   1. queueRepo.dequeue()
    //   2. addStock(planned_qty)   생산된 재고 입고
    //   3. deductStock(order_qty)  주문량 차감
    //   4. orderRepo → CONFIRMED
    bool completeProduction(const std::string& production_id);

    // 메뉴 5: 출고 처리 (CONFIRMED → RELEASE)
    bool shipOrder(const std::string& order_number);

    // 조회 (메뉴 6 화면용)
    std::vector<ProductionQueueItem>   getQueue() const;
    std::optional<ProductionQueueItem> getCurrentProduction() const;

private:
    ProductionQueueRepository& queueRepo_;
    OrderRepository&           orderRepo_;
    InventoryService&          inventoryService_;
};
