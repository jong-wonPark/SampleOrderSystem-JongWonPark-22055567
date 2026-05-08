# Phase 6 설계 문서 — Controller 계층

## 목표

**입력 → 서비스 → 뷰 파이프라인** 구현.  
Repository 직접 호출 금지. 모든 비즈니스 처리는 Service를 통해서만 실행한다.

---

## 1. 의존성

```
Controller (Phase 6)
  ├─→ Services/*.h   (InventoryService, OrderService, ProductionService)
  └─→ Views/*.h      (MainMenuView, SampleView, OrderView, MonitoringView, ProductionView)
```

---

## 2. 생성 파일 목록

```
SampleOrderSystem/
└── Controllers/
    ├── SampleController.h / .cpp      ← 메뉴 1: 시료 관리
    ├── OrderController.h / .cpp       ← 메뉴 2·3: 시료 주문·승인/거절
    ├── MonitoringController.h / .cpp  ← 메뉴 4: 모니터링
    ├── ProductionController.h / .cpp  ← 메뉴 5·6: 출고 처리·생산 라인
    └── AppController.h / .cpp        ← 전체 조립 & 메인 루프
```

수정 파일:
```
SampleOrderSystem/
├── SampleOrderSystem.vcxproj
├── SampleOrderSystem.vcxproj.filters
└── main.cpp   ← AppController 연결
```

---

## 3. Controller 공통 패턴

### 3.1 메서드 구조
```
public run() / runXxx()   ← AppController가 호출하는 진입점
    └─ 서브메뉴 루프 or 단일 액션 처리
         └─ private handle*()  ← 개별 액션 (View 입력 → Service → View 출력)
```

### 3.2 handle 메서드 내부 흐름
```cpp
void XxxController::handleSomething() {
    // 1. View로 사전 정보 출력 (목록 등)
    XxxView::showXxx(service_.getData());

    // 2. View로 사용자 입력 수집
    auto input = XxxView::promptXxx();
    if (input == "0" || input.empty()) return;  // 취소

    // 3. Service 호출 (비즈니스 처리)
    if (!service_.doSomething(input)) {
        MainMenuView::showError("처리 실패 메시지");
        return;
    }

    // 4. View로 결과 출력
    auto updated = service_.findByXxx(input);
    if (updated.has_value())
        XxxView::showResult(*updated);
}
```

---

## 4. SampleController — 메뉴 1

```cpp
class SampleController {
public:
    explicit SampleController(InventoryService& invSvc);
    void run();           // 서브메뉴 루프 (1:등록 2:목록 3:검색 0:뒤로)
private:
    InventoryService& invSvc_;
    void handleRegister();
    void handleList();
    void handleSearch();
};
```

**run() 루프**:
```cpp
while (true) {
    int choice = SampleView::promptSubMenu();
    if (choice == 0) break;
    switch (choice) {
        case 1: handleRegister(); break;
        case 2: handleList();     break;
        case 3: handleSearch();   break;
    }
    MainMenuView::pause();
}
```

**handleRegister()**:
```
SampleView::promptSampleInput()
→ invSvc_.registerSample(input)
→ MainMenuView::showSuccess / showError
```

**handleList()**:
```
invSvc_.getAllSamples() + getAllInventory()
→ SampleView::showSampleTable(samples, inventory)
```

**handleSearch()**:
```
SampleView::promptSearchKeyword()
→ invSvc_.findSamplesByName(keyword)
→ SampleView::showSearchResults(results, inventory)
```

---

## 5. OrderController — 메뉴 2·3

```cpp
class OrderController {
public:
    OrderController(OrderService& ordSvc, InventoryService& invSvc);
    void runPlaceOrder();  // 메뉴 2: 단일 액션
    void runApproval();    // 메뉴 3: 서브메뉴 루프 (1:승인 2:거절 0:뒤로)
private:
    OrderService&     ordSvc_;
    InventoryService& invSvc_;
    void handlePlaceOrder();
    void handleApprove();
    void handleReject();
};
```

**handlePlaceOrder()**:
```
OrderView::promptOrderInput()           → (sample_id, customer_name, qty)
→ invSvc_.findSampleById(sample_id)     → 시료 존재 확인 (없으면 showError + return)
→ ordSvc_.placeOrder(id, name, cust, qty)
→ OrderView::showPlaceOrderResult(order)
```

> `invSvc_.findSampleById(sample_id)` 로 시료 유효성을 Controller에서 사전 검증한다.  
> `sample_name`은 Service 호출 시 `SampleItem.sample_name`으로 채운다.

**handleApprove()**:
```
ordSvc_.getOrdersByStatus(RESERVED)
→ OrderView::showReservedOrders(reserved)  (비어있으면 메시지 후 return)
→ OrderView::promptOrderNumber("승인")     (0이면 return)
→ ordSvc_.approveOrder(order_number)       (false면 showError + return)
→ ordSvc_.findByOrderNumber(order_number)  → updated Order
→ OrderView::showApproveResult(*updated)
```

**handleReject()**:
```
ordSvc_.getOrdersByStatus(RESERVED)
→ OrderView::showReservedOrders(reserved)
→ OrderView::promptOrderNumber("거절")
→ OrderView::promptRejectNote()
→ ordSvc_.rejectOrder(order_number, note)
→ ordSvc_.findByOrderNumber(order_number)
→ OrderView::showRejectResult(*updated)
```

---

## 6. MonitoringController — 메뉴 4

```cpp
class MonitoringController {
public:
    MonitoringController(OrderService& ordSvc, InventoryService& invSvc);
    void run();           // 단일 화면 출력 후 pause
private:
    OrderService&     ordSvc_;
    InventoryService& invSvc_;
};
```

**run()**:
```
ordSvc_.getAllOrders()
invSvc_.getAllSamples()
invSvc_.getAllInventory()
→ MonitoringView::showDashboard(orders, samples, inventory)
→ MainMenuView::pause()
```

---

## 7. ProductionController — 메뉴 5·6

```cpp
class ProductionController {
public:
    ProductionController(ProductionService& prodSvc, OrderService& ordSvc);
    void runShipping();    // 메뉴 5: 단일 액션
    void runProduction();  // 메뉴 6: 서브메뉴 루프 (1:생산시작 2:생산완료 0:뒤로)
private:
    ProductionService& prodSvc_;
    OrderService&      ordSvc_;
    void handleShip();
    void handleStartProduction();
    void handleCompleteProduction();
};
```

**handleShip()**:
```
ordSvc_.getOrdersByStatus(CONFIRMED)
→ ProductionView::showConfirmedOrders(confirmed)  (비어있으면 return)
→ ProductionView::promptShipOrderNumber()          (0이면 return)
→ prodSvc_.shipOrder(order_number)                 (false면 showError + return)
→ ordSvc_.findByOrderNumber(order_number)
→ ProductionView::showShipResult(*updated)
```

**handleStartProduction()**:
```
prodSvc_.getQueue()
→ ProductionView::showProductionQueue(queue)
→ prodSvc_.startNextProduction()                   (false면 "대기 항목 없음" + return)
→ prodSvc_.getCurrentProduction()
→ ProductionView::showStartResult(*inProd)
```

**handleCompleteProduction()** — 핵심 처리:
```
prodSvc_.getQueue()
→ ProductionView::showProductionQueue(queue)
→ ProductionView::promptCompleteProductionId()      (0이면 return)

// 완료 전 order_number 확보 (complete 후 큐에서 사라지므로)
→ prodSvc_.getQueue()에서 production_id 매칭 → orderNumber 추출
  (없으면 showError + return)

→ prodSvc_.completeProduction(production_id)        (false면 showError + return)

→ ordSvc_.findByOrderNumber(orderNumber)
→ ProductionView::showCompleteResult(*confirmedOrder)
```

> **설계 포인트**: `completeProduction` 이후 생산 항목이 큐에서 사라지므로  
> `order_number`를 **complete 호출 전**에 `getQueue()`에서 미리 추출해야 한다.

---

## 8. AppController — 전체 조립

### 8.1 멤버 선언 (초기화 순서 = 선언 순서)

```cpp
class AppController {
public:
    AppController();     // 데이터 경로: "data/" (실행 파일 기준)
    void run();
private:
    // ── Managers (파일 경로 주입) ────────────────────────────────
    InventoryManager       invMgr_;
    OrderManager           ordMgr_;
    ProductionQueueManager prodMgr_;

    // ── Repositories ─────────────────────────────────────────────
    InventoryRepository       invRepo_;
    OrderRepository           ordRepo_;
    ProductionQueueRepository queueRepo_;

    // ── Services (의존성 순서 엄수) ──────────────────────────────
    InventoryService  invSvc_;
    ProductionService prodSvc_;  // invSvc_ 이후
    OrderService      ordSvc_;   // invSvc_ + prodSvc_ 이후

    // ── Sub-controllers ──────────────────────────────────────────
    SampleController      sampleCtrl_;
    OrderController       orderCtrl_;
    MonitoringController  monitoringCtrl_;
    ProductionController  productionCtrl_;

    void handleMenuChoice(int choice);
};
```

### 8.2 생성자 초기화 리스트 (의존성 연결)

```cpp
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
```

> **C++ 멤버 초기화 규칙**: 초기화 리스트의 순서가 아닌 **클래스 멤버 선언 순서**로 초기화된다.  
> 위 선언 순서가 의존성을 올바르게 반영하는지 반드시 확인해야 한다.

### 8.3 run() 메인 루프

```cpp
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
```

### 8.4 main.cpp 최종 형태

```cpp
#include <iostream>
#include <windows.h>
#include "Controllers/AppController.h"

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    AppController app;
    app.run();
    return 0;
}
```

---

## 9. 프로젝트 파일 변경 사항

### 9.1 `SampleOrderSystem.vcxproj`

```xml
<ItemGroup>
  <ClCompile Include="Controllers\SampleController.cpp" />
  <ClCompile Include="Controllers\OrderController.cpp" />
  <ClCompile Include="Controllers\MonitoringController.cpp" />
  <ClCompile Include="Controllers\ProductionController.cpp" />
  <ClCompile Include="Controllers\AppController.cpp" />
</ItemGroup>
<ItemGroup>
  <ClInclude Include="Controllers\SampleController.h" />
  <ClInclude Include="Controllers\OrderController.h" />
  <ClInclude Include="Controllers\MonitoringController.h" />
  <ClInclude Include="Controllers\ProductionController.h" />
  <ClInclude Include="Controllers\AppController.h" />
</ItemGroup>
```

### 9.2 `SampleOrderSystem.vcxproj.filters`

```xml
<Filter Include="Controllers">
  <UniqueIdentifier>{A1B2C3D4-0006-0006-0006-000000000006}</UniqueIdentifier>
</Filter>
```

---

## 10. 핵심 설계 결정

| 항목 | 결정 | 이유 |
|---|---|---|
| Repository 직접 호출 | 금지 — Service만 호출 | Controller는 비즈니스 흐름 오케스트레이션 전담 |
| 시료 유효성 검증 위치 | OrderController (Controller) | Service는 sample_name을 받는 API라 존재 여부는 Controller 책임 |
| `order_number` 선행 조회 | handleCompleteProduction에서 complete 전 추출 | complete 후 큐에서 사라져 order_number 조회 불가 |
| 멤버 선언 순서 | 의존성 위상 정렬 순서 | C++는 선언 순서로 초기화 → 잘못된 순서 시 참조 대상 미초기화 UB |
| 데이터 파일 경로 | `"data/"` 하드코딩 (AppController) | Phase 0 post-build가 exe 옆에 data/ 복사하므로 단순 상대경로 충분 |
| 메뉴 2 vs 서브메뉴 | runPlaceOrder / runApproval 분리 | 메뉴 2는 단일 액션, 메뉴 3은 1:승인/2:거절 서브 선택 필요 |
| 자동 테스트 | 없음 | Controller는 UI 인터랙션 필요 — Phase 7 수동 통합 검증 |

---

## 11. 완료 기준 (Definition of Done)

| 번호 | 검증 항목 |
|---|---|
| 1 | 빌드 오류 없음 |
| 2 | AppController 생성자 컴파일 정상 (초기화 순서 오류 없음) |
| 3 | 메인 메뉴 0번 → "종료합니다" 메시지 후 정상 종료 |
| 4 | 메뉴 1 → 시료 등록 → 목록에 반영 확인 |
| 5 | 메뉴 2 → 주문 접수 → 메뉴 4 모니터링에서 RESERVED 카운트 증가 |
| 6 | 메뉴 3 승인 (재고있음) → 메뉴 4에서 CONFIRMED 증가 |
| 7 | 메뉴 3 승인 (재고없음) → 메뉴 6 생산 큐에 항목 추가 |
| 8 | 메뉴 6 생산시작 → 생산완료 → 메뉴 5에서 출고 대기 목록 확인 |
| 9 | 메뉴 5 출고 → 메뉴 4 RELEASE 카운트 증가 |
| 10 | 앱 재시작 후 data/ JSON 파일에서 이전 데이터 복구 |

---

## 12. 작업 순서 체크리스트

```
[ ] 1. SampleOrderSystem/Controllers/ 디렉토리 생성
[ ] 2. Controllers/SampleController.h / .cpp 작성
[ ] 3. Controllers/OrderController.h / .cpp 작성
[ ] 4. Controllers/MonitoringController.h / .cpp 작성
[ ] 5. Controllers/ProductionController.h / .cpp 작성
[ ] 6. Controllers/AppController.h / .cpp 작성
      [ ] 6-1. 멤버 선언 순서 (의존성 위상 정렬)
      [ ] 6-2. 생성자 초기화 리스트
      [ ] 6-3. run() + handleMenuChoice()
[ ] 7. SampleOrderSystem.vcxproj — ClCompile/ClInclude 10개 추가
[ ] 8. SampleOrderSystem.vcxproj.filters — Controllers 필터 추가
[ ] 9. main.cpp — AppController 연결 (주석 해제)
[ ] 10. 빌드 확인
[ ] 11. 완료 기준 10가지 수동 검증 (Phase 7 통합 검증)
```
