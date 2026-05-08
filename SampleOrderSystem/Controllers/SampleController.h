#pragma once
#include "../Services/InventoryService.h"

class SampleController {
public:
    explicit SampleController(InventoryService& invSvc);
    void run();   // 서브메뉴 루프 (1:등록 2:목록 3:검색 0:뒤로)
private:
    InventoryService& invSvc_;
    void handleRegister();
    void handleList();
    void handleSearch();
};
