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
    , monitoringCtrl_ (ordSvc_, invSvc_)
    , productionCtrl_ (prodSvc_, ordSvc_)
{}

void AppController::run() {
    while (true) {
        MainMenuView::showMainMenu();
        int choice = MainMenuView::promptMenuChoice();
        if (choice == 0) break;
        MainMenuView::clearScreen();
        handleMenuChoice(choice);
    }
    std::cout << "\ns-semi 시료 관리 시스템을 종료합니다.\n";
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
