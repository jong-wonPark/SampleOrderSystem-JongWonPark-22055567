#pragma once
#include "../Persistence/DataPersistence.h"
#include "../Repositories/InventoryRepository.h"
#include "../Repositories/OrderRepository.h"
#include "../Repositories/ProductionQueueRepository.h"
#include "../Services/InventoryService.h"
#include "../Services/ProductionService.h"
#include "../Services/OrderService.h"
#include "SampleController.h"
#include "OrderController.h"
#include "MonitoringController.h"
#include "ProductionController.h"

class AppController {
public:
    AppController();
    void run();
private:
    // ── Managers (선언 순서 = 초기화 순서) ──────────────────────
    InventoryManager       invMgr_;
    OrderManager           ordMgr_;
    ProductionQueueManager prodMgr_;

    // ── Repositories ─────────────────────────────────────────────
    InventoryRepository       invRepo_;
    OrderRepository           ordRepo_;
    ProductionQueueRepository queueRepo_;

    // ── Services ─────────────────────────────────────────────────
    InventoryService  invSvc_;
    ProductionService prodSvc_;   // invSvc_ 이후
    OrderService      ordSvc_;    // invSvc_ + prodSvc_ 이후

    // ── Sub-controllers ──────────────────────────────────────────
    SampleController      sampleCtrl_;
    OrderController       orderCtrl_;
    MonitoringController  monitoringCtrl_;
    ProductionController  productionCtrl_;

    void handleMenuChoice(int choice);
    void checkProductionOnStartup(); // 시작 시 완료된 생산 자동 처리
};
