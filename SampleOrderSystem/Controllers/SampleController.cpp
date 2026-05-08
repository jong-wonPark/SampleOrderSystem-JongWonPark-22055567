#include "SampleController.h"
#include "../Views/SampleView.h"
#include "../Views/MainMenuView.h"

#include <stdexcept>

SampleController::SampleController(InventoryService& invSvc)
    : invSvc_(invSvc)
{}

void SampleController::run() {
    while (true) {
        int choice = SampleView::promptSubMenu();
        if (choice == 0) break;
        MainMenuView::clearScreen();
        switch (choice) {
            case 1: handleRegister(); break;
            case 2: handleList();     break;
            case 3: handleSearch();   break;
        }
        MainMenuView::pause();
    }
}

void SampleController::handleRegister() {
    auto input = SampleView::promptSampleInput();
    try {
        auto s = invSvc_.registerSample(
            input.sample_id, input.sample_name,
            input.avg_production_time_hours, input.yield_rate,
            input.initial_quantity);
        MainMenuView::showSuccess(
            "시료 등록 완료: " + s.sample_name + " (ID: " + s.sample_id + ")");
    } catch (const std::exception& e) {
        MainMenuView::showError(e.what());
    }
}

void SampleController::handleList() {
    MainMenuView::printHeader("시료 목록");
    SampleView::showSampleTable(invSvc_.getAllSamples(), invSvc_.getAllInventory());
}

void SampleController::handleSearch() {
    auto keyword = SampleView::promptSearchKeyword();
    MainMenuView::printHeader("검색 결과: \"" + keyword + "\"");
    SampleView::showSearchResults(
        invSvc_.findSamplesByName(keyword), invSvc_.getAllInventory());
}
