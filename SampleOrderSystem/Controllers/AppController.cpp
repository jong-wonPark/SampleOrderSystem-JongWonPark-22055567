#include "AppController.h"
#include "../Views/MainMenuView.h"

#include <iostream>

AppController::AppController()
    : invMgr_   ("data/inventory.json")
    , ordMgr_   ("data/orders.json")
    , prodMgr_  ("data/production_queue.json")
    , invRepo_  (invMgr_)
    , ordRepo_  (ordMgr_)
    , queueRepo_(prodMgr_)
    , invSvc_   (invRepo_)
    , prodSvc_  (queueRepo_, ordRepo_, invSvc_)
    , ordSvc_   (ordRepo_,   invSvc_, prodSvc_)
    , sampleCtrl_     (invSvc_)
    , orderCtrl_      (ordSvc_, invSvc_)
    , monitoringCtrl_ (ordSvc_, invSvc_, prodSvc_)
    , productionCtrl_ (prodSvc_, ordSvc_)
{}

void AppController::run() {
    // 프로그램 시작 시 생산 완료 여부 복구
    // JSON에 기록된 started_at / estimated_completion 기준으로
    // 프로그램 종료 중 완료됐어야 할 항목을 자동 처리
    checkProductionOnStartup();

    while (true) {
        MainMenuView::showMainMenu();
        int choice = MainMenuView::promptMenuChoice();
        if (choice == 0) break;
        MainMenuView::clearScreen();
        handleMenuChoice(choice);
    }
    std::cout << "\ns-semi 시료 관리 시스템을 종료합니다.\n";
}

void AppController::checkProductionOnStartup() {
    auto completedOrders = prodSvc_.autoCompleteExpired();
    if (completedOrders.empty()) return;

    std::cout << Clr::BrYellow
              << "\n[시작 알림] 프로그램 종료 중 생산이 완료된 주문이 있습니다.\n"
              << Clr::Reset;
    for (const auto& orderNum : completedOrders) {
        auto order = ordSvc_.findByOrderNumber(orderNum);
        if (order.has_value())
            std::cout << Clr::BrGreen
                      << "  ✓ " << orderNum << " → 출고 대기(CONFIRMED)\n"
                      << Clr::Reset;
    }
    MainMenuView::pause();
}

void AppController::handleMenuChoice(int choice) {
    switch (choice) {
        case 1: sampleCtrl_.run();               break;
        case 2: orderCtrl_.runPlaceOrder();      break;
        case 3: orderCtrl_.runApproval();        break;
        case 4: monitoringCtrl_.run();           break;
        case 5: productionCtrl_.runShipping();   break;
        case 6: productionCtrl_.runProduction(); break;
    }
}
