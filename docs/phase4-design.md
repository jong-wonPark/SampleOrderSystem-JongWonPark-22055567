# Phase 4 설계 문서 — Service 계층

## 목표

**핵심 비즈니스 로직 및 주문 상태 전이** 구현.  
Controller(Phase 6)가 Service를 통해서만 비즈니스 동작을 실행한다.  
Repository를 직접 호출하거나 상태 전이 로직을 갖지 않는다.

---

## 1. 의존성 그래프

```
InventoryService  ──→ InventoryRepository

ProductionService ──→ ProductionQueueRepository
                  ──→ OrderRepository           (완료 시 CONFIRMED 갱신)
                  ──→ InventoryService           (생산 완료 후 재고 처리)

OrderService      ──→ OrderRepository
                  ──→ InventoryService
                  ──→ ProductionService          (재고 부족 시 큐 등록)
```

> **순환 의존 없음**: ProductionService는 OrderService를 호출하지 않고 OrderRepository를 직접 호출한다.

---

## 2. 생성 파일 목록

```
SampleOrderSystem/
└── Services/
    ├── InventoryService.h / .cpp
    ├── OrderService.h / .cpp
    └── ProductionService.h / .cpp
```

수정 파일:
```
SampleOrderSystem/
├── SampleOrderSystem.vcxproj
└── SampleOrderSystem.vcxproj.filters

Tests/
├── SampleOrderSystemTests.vcxproj
└── ServiceTests.cpp  ← 신규
```

---

## 3. 반환 타입 컨벤션

| 반환 타입 | 용도 |
|---|---|
| `Order` | 생성 성공이 보장되는 경우 (`placeOrder`) |
| `bool` | 성공/실패가 있는 변경 연산 (`approveOrder`, `deductStock` 등) |
| `std::optional<T>` | 존재 여부가 불확실한 조회 (`getStock`) |
| `std::vector<T>` | 복수 조회 (항상 반환, 없으면 빈 벡터) |

Service 메서드는 Repository 예외를 catch하여 `false`/`std::nullopt`로 변환한다.  
단, `placeOrder`는 실패 시 예외를 그대로 전파한다 (호출 측에서 입력 유효성을 보장해야 함).

---

## 4. InventoryService

### 4.1 역할
- 메뉴 1 (시료 관리): 시료 등록·조회·검색
- OrderService / ProductionService에서 재고 확인·차감·추가

### 4.2 API

```cpp
class InventoryService {
public:
    explicit InventoryService(InventoryRepository& repo);

    // ── 시료 마스터 관리 (메뉴 1) ────────────────────────────────
    SampleItem              registerSample(const std::string& sample_id,
                                           const std::string& sample_name,
                                           double avg_production_time_hours,
                                           double yield_rate,
                                           int    initial_quantity = 0);
    std::vector<SampleItem>   getAllSamples() const;
    std::optional<SampleItem> findSampleById(const std::string& sample_id) const;
    std::vector<SampleItem>   findSamplesByName(const std::string& keyword) const;

    // ── 재고 관리 ─────────────────────────────────────────────────
    std::optional<InventoryItem> getStock(const std::string& sample_id) const;
    std::vector<InventoryItem>   getAllInventory() const;
    bool hasEnoughStock(const std::string& sample_id, int quantity) const;
    // delta < 0 출고: 재고 부족이면 false 반환
    bool deductStock(const std::string& sample_id, int quantity);
    // 생산 완료 시 입고
    bool addStock(const std::string& sample_id, int quantity);

private:
    InventoryRepository& repo_;
};
```

### 4.3 구현 요점

```cpp
bool InventoryService::deductStock(const std::string& sample_id, int quantity) {
    try {
        repo_.updateStock(sample_id, -quantity);  // 부족 시 exception
        return true;
    } catch (...) { return false; }
}

bool InventoryService::addStock(const std::string& sample_id, int quantity) {
    try {
        repo_.updateStock(sample_id, +quantity);
        return true;
    } catch (...) { return false; }
}
```

---

## 5. OrderService

### 5.1 역할
- 메뉴 2: 고객 주문 접수 (→ RESERVED)
- 메뉴 3: 생산 담당자 승인/거절

### 5.2 API

```cpp
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

    // 메뉴 3: 승인 처리
    //   precondition: 해당 주문이 RESERVED 상태
    //   재고 충분 → deductStock → CONFIRMED
    //   재고 부족 → enqueueProduction → PRODUCING
    bool approveOrder(const std::string& order_number);

    // 메뉴 3: 거절 (RESERVED → REJECTED)
    bool rejectOrder(const std::string& order_number,
                     const std::string& note = "");

    // 조회 (모니터링·목록 표시용)
    std::vector<Order>   getAllOrders() const;
    std::vector<Order>   getOrdersByStatus(OrderStatus status) const;
    std::optional<Order> findByOrderNumber(const std::string& order_number) const;

private:
    OrderRepository&   orderRepo_;
    InventoryService&  inventoryService_;
    ProductionService& productionService_;
};
```

### 5.3 `approveOrder` 상태 전이 로직

```
approveOrder(order_number)
  │
  ├─ 주문 조회 실패 or 상태 != RESERVED  ──→ return false
  │
  ├─ inventoryService.hasEnoughStock(sample_id, order_quantity)
  │       true  ──→ inventoryService.deductStock(...)
  │               + orderRepo.updateStatus(..., CONFIRMED)
  │               + return true
  │
  └─   false  ──→ productionService.enqueueProduction(order_number, ...)
                + orderRepo.updateStatus(..., PRODUCING)
                + return true
```

---

## 6. ProductionService

### 6.1 역할
- 메뉴 5: 출고 처리 (CONFIRMED → RELEASE)
- 메뉴 6: 생산 라인 관리 (생산 시작·완료)
- OrderService 내부에서 enqueueProduction 호출

### 6.2 API

```cpp
class ProductionService {
public:
    ProductionService(ProductionQueueRepository& queueRepo,
                      OrderRepository&           orderRepo,
                      InventoryService&          inventoryService);

    // OrderService::approveOrder 내부에서 호출
    // Order → PRODUCING, 큐 말단 등록
    ProductionQueueItem enqueueProduction(const std::string& order_number,
                                          const std::string& sample_id,
                                          const std::string& sample_name,
                                          int                planned_quantity);

    // 메뉴 6: 큐 front(Waiting) → InProduction
    // 대기 항목 없으면 false
    bool startNextProduction();

    // 메뉴 6: InProduction → 완료
    //   1. queueRepo.dequeue()
    //   2. inventoryService.addStock(planned_qty)   ← 생산된 재고 입고
    //   3. inventoryService.deductStock(order_qty)  ← 주문량 차감
    //   4. orderRepo.updateStatus(..., CONFIRMED)
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
```

### 6.3 `completeProduction` 상태 전이 로직

```
completeProduction(production_id)
  │
  ├─ queueRepo.findById 실패 or 상태 != InProduction  ──→ return false
  │
  ├─ completed = queueRepo.dequeue(production_id)      ← 큐에서 제거
  ├─ inventoryService.addStock(sample_id, planned_qty) ← 생산된 재고 입고
  ├─ inventoryService.deductStock(sample_id, order_qty)← 주문량 차감
  ├─ orderRepo.updateStatus(order_number, CONFIRMED)
  └─ return true
```

> `addStock` 후 `deductStock`의 의미: 생산된 재고가 실제로 재고에 반영됐다가 주문에 배정됨을 명시적으로 표현. `planned_quantity == order_quantity`이면 재고 순변화 0.

### 6.4 `shipOrder` 상태 전이 로직

```
shipOrder(order_number)
  │
  ├─ 주문 조회 실패 or 상태 != CONFIRMED  ──→ return false
  └─ orderRepo.updateStatus(..., RELEASE)
     return true
```

---

## 7. AppController에서의 의존성 조립 (Phase 6 예고)

```cpp
// 생성자 초기화 리스트 (순서 중요)
AppController::AppController(...)
    // Repositories
    : inventoryRepo_(inventoryMgr_),
      orderRepo_(orderMgr_),
      productionQueueRepo_(productionQueueMgr_),
    // Services (Repository 먼저 초기화 후)
      inventoryService_(inventoryRepo_),
      productionService_(productionQueueRepo_, orderRepo_, inventoryService_),
      orderService_(orderRepo_, inventoryService_, productionService_),
    // Controllers
      ...
```

---

## 8. 주요 시나리오별 흐름 요약

### 시나리오 A: 재고 있음 → 즉시 출고 대기
```
placeOrder()        → RESERVED
approveOrder()      → hasEnoughStock=true → deductStock → CONFIRMED
shipOrder()         → RELEASE
```

### 시나리오 B: 재고 없음 → 생산 후 출고
```
placeOrder()            → RESERVED
approveOrder()          → hasEnoughStock=false → enqueueProduction → PRODUCING
startNextProduction()   → ProductionQueueItem: Waiting → InProduction
completeProduction()    → addStock + deductStock + CONFIRMED
shipOrder()             → RELEASE
```

### 시나리오 C: 거절
```
placeOrder()  → RESERVED
rejectOrder() → REJECTED
```

---

## 9. 테스트 설계 (`Tests/ServiceTests.cpp`)

각 Service 테스트는 전체 의존성 체인을 실제 Manager + Repository + Service로 구성한다.

```cpp
// 공통 SetUp 패턴
TempDir tmp_;
InventoryManager        invMgr_   { tmp_.file("inventory.json") };
OrderManager            ordMgr_   { tmp_.file("orders.json") };
ProductionQueueManager  prodMgr_  { tmp_.file("production_queue.json") };

InventoryRepository     invRepo_  { invMgr_ };
OrderRepository         ordRepo_  { ordMgr_ };
ProductionQueueRepository queueRepo_{ prodMgr_ };

InventoryService        invSvc_   { invRepo_ };
ProductionService       prodSvc_  { queueRepo_, ordRepo_, invSvc_ };
OrderService            ordSvc_   { ordRepo_, invSvc_, prodSvc_ };
```

| 테스트 클래스 | 핵심 케이스 |
|---|---|
| `InventoryServiceTest` | registerSample, getAllSamples, findByName, getStock, hasEnoughStock, deductStock(성공/부족), addStock |
| `OrderServiceTest` | placeOrder→RESERVED, approveOrder(재고있음→CONFIRMED+재고차감), approveOrder(재고없음→PRODUCING+큐등록), rejectOrder→REJECTED, 잘못된상태→false |
| `ProductionServiceTest` | enqueueProduction, startNextProduction(성공/빈큐→false), completeProduction(→CONFIRMED+재고정산), shipOrder(→RELEASE), 잘못된상태→false |

---

## 10. 프로젝트 파일 변경 사항

### 10.1 `SampleOrderSystem.vcxproj`

```xml
<ItemGroup>
  <ClCompile Include="Services\InventoryService.cpp" />
  <ClCompile Include="Services\OrderService.cpp" />
  <ClCompile Include="Services\ProductionService.cpp" />
</ItemGroup>
<ItemGroup>
  <ClInclude Include="Services\InventoryService.h" />
  <ClInclude Include="Services\OrderService.h" />
  <ClInclude Include="Services\ProductionService.h" />
</ItemGroup>
```

### 10.2 `SampleOrderSystem.vcxproj.filters`

```xml
<Filter Include="Services">
  <UniqueIdentifier>{A1B2C3D4-0004-0004-0004-000000000004}</UniqueIdentifier>
</Filter>
```

### 10.3 `Tests/SampleOrderSystemTests.vcxproj`

```xml
<ClCompile Include="ServiceTests.cpp" />
<ClCompile Include="..\SampleOrderSystem\Services\InventoryService.cpp" />
<ClCompile Include="..\SampleOrderSystem\Services\OrderService.cpp" />
<ClCompile Include="..\SampleOrderSystem\Services\ProductionService.cpp" />
```

---

## 11. 핵심 설계 결정

| 항목 | 결정 | 이유 |
|---|---|---|
| 순환 의존 해결 | ProductionService가 OrderService 아닌 OrderRepository 직접 호출 | 의존성 역전 없이 계층 분리 유지 |
| bool 반환 | 변경 연산은 bool로 통일 | Controller가 결과에 따라 View에 성공/실패 메시지 전달 가능 |
| 예외 catch 위치 | Service에서 catch → bool 변환 | Repository 예외 세부 사항을 Controller까지 노출하지 않음 |
| addStock + deductStock | completeProduction에서 두 단계 실행 | 생산물이 재고를 경유했음을 명시적으로 표현, 감사 추적 용이 |
| enqueueProduction 반환 | `ProductionQueueItem` (bool 아님) | OrderService가 production_id를 별도로 알 필요 없음 (내부용) |
| registerSample 위치 | InventoryService에 포함 | 시료 마스터와 재고는 동일 파일(inventory.json) 기반이므로 분리 불필요 |

---

## 12. 완료 기준 (Definition of Done)

| 번호 | 검증 항목 |
|---|---|
| 1 | 빌드 오류 없음 (메인 + 테스트) |
| 2 | 재고 충분 시 approveOrder → CONFIRMED + 재고 차감 확인 |
| 3 | 재고 부족 시 approveOrder → PRODUCING + 생산 큐 등록 확인 |
| 4 | completeProduction → CONFIRMED + 재고 addStock/deductStock 확인 |
| 5 | shipOrder → RELEASE 확인 |
| 6 | 잘못된 상태(non-RESERVED)에 approveOrder 호출 → false 반환 |
| 7 | deductStock 재고 부족 → false 반환 (예외 전파 없음) |
| 8 | startNextProduction 대기 항목 없음 → false 반환 |
| 9 | gtest 전체 테스트 통과 (기존 75개 + 신규 Service 테스트) |

---

## 13. 작업 순서 체크리스트

```
[ ] 1. SampleOrderSystem/Services/ 디렉토리 생성
[ ] 2. Services/InventoryService.h 작성
[ ] 3. Services/InventoryService.cpp 작성
[ ] 4. Services/ProductionService.h 작성   ← OrderService보다 먼저 (의존성 순서)
[ ] 5. Services/ProductionService.cpp 작성
[ ] 6. Services/OrderService.h 작성
[ ] 7. Services/OrderService.cpp 작성
[ ] 8. SampleOrderSystem.vcxproj 업데이트
[ ] 9. SampleOrderSystem.vcxproj.filters 업데이트
[ ] 10. Tests/ServiceTests.cpp 작성
[ ] 11. Tests/SampleOrderSystemTests.vcxproj 업데이트
[ ] 12. 빌드 및 gtest 전체 실행 확인
```
