#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../Models/Order.h"
#include "../Models/Enums.h"
#include "../Repositories/OrderRepository.h"
#include "InventoryService.h"
#include "ProductionService.h"

class OrderService {
public:
    OrderService(OrderRepository&   orderRepo,
                 InventoryService&  inventoryService,
                 ProductionService& productionService);

    // 메뉴 2: 주문 접수 (항상 RESERVED로 생성)
    Order placeOrder(const std::string& sample_id,
                     const std::string& sample_name,
                     const std::string& customer_name,
                     int                order_quantity);

    // 메뉴 3: 승인
    //   재고 충분 → deductStock → CONFIRMED
    //   재고 부족 → enqueueProduction → PRODUCING
    //   precondition 실패(미존재/비RESERVED) → false
    bool approveOrder(const std::string& order_number);

    // 메뉴 3: 거절 (RESERVED → REJECTED)
    bool rejectOrder(const std::string& order_number,
                     const std::string& note = "");

    // 조회 (모니터링·목록 표시)
    std::vector<Order>   getAllOrders() const;
    std::vector<Order>   getOrdersByStatus(OrderStatus status) const;
    std::optional<Order> findByOrderNumber(const std::string& order_number) const;

private:
    OrderRepository&   orderRepo_;
    InventoryService&  inventoryService_;
    ProductionService& productionService_;
};
