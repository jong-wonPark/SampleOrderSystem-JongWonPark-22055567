# Phase 3 설계 문서 — Repository 계층

## 목표

Persistence 위에서 **인메모리 CRUD + 필터 쿼리** 레이어 구현.  
Service(Phase 4)가 Manager(Persistence)를 직접 호출하지 않고 Repository를 통해서만 데이터에 접근한다.

- `Repositories/InventoryRepository.h/.cpp`
- `Repositories/OrderRepository.h/.cpp`
- `Repositories/ProductionQueueRepository.h/.cpp`

---

## 1. 의존성

```
Repository (Phase 3)
  ├─→ Persistence/DataPersistence.h  (Manager 3종)
  └─→ Models/*.h                     (데이터 구조체)

Service (Phase 4) ──→ Repository    (Repository만 호출, Manager 직접 호출 금지)
```

---

## 2. 생성 파일 목록

```
SampleOrderSystem/
└── Repositories/
    ├── InventoryRepository.h / .cpp
    ├── OrderRepository.h / .cpp
    └── ProductionQueueRepository.h / .cpp
```

수정 파일:
```
SampleOrderSystem/
├── SampleOrderSystem.vcxproj         ← ClCompile/ClInclude 6개 추가
└── SampleOrderSystem.vcxproj.filters ← Repositories 필터 추가

Tests/
├── SampleOrderSystemTests.vcxproj    ← RepositoryTests.cpp 추가
└── RepositoryTests.cpp               ← 신규 테스트 파일
```

---

## 3. 핵심 패턴: Write-through Cache

```
[Service]
    │ Repository 메서드 호출
    ▼
[Repository]  ── 읽기: in-memory std::vector (빠름)
    │           ── 쓰기: Manager 먼저 호출 (auto-ID 등 생성) → 캐시 갱신
    ▼
[Manager]     ── JSON 파일 I/O
```

**규칙**:
1. 생성자에서 Manager로부터 전체 데이터를 `std::vector`에 로드
2. 읽기 연산은 캐시(vector)에서만 수행
3. 쓰기 연산은 **Manager 먼저 호출** → 반환값(자동 채번된 ID 포함)으로 캐시 갱신
4. Manager 예외는 그대로 전파 (Service가 catch)

---

## 4. 클래스 설계

### 4.1 `InventoryRepository`

SampleItem과 InventoryItem을 두 개의 독립 캐시로 관리.  
(두 타입이 `sample_id` 기준 1:1 관계이므로 동기 유지가 단순)

```cpp
class InventoryRepository {
public:
    explicit InventoryRepository(InventoryManager& mgr);

    // ── SampleItem 관련 (메뉴 1: 시료 관리) ──────────────────────
    SampleItem              addSample(const std::string& sample_id,
                                      const std::string& sample_name,
                                      double avg_production_time_hours,
                                      double yield_rate,
                                      int    initial_quantity = 0,
                                      const std::string& unit = "EA");
    std::vector<SampleItem>   findAllSamples() const;
    std::optional<SampleItem> findSampleById(const std::string& sample_id) const;
    std::vector<SampleItem>   findSamplesByName(const std::string& keyword) const;
    void removeSample(const std::string& sample_id);

    // ── InventoryItem 관련 (재고 확인) ───────────────────────────
    std::vector<InventoryItem>   findAllInventory() const;
    std::optional<InventoryItem> findInventoryById(const std::string& sample_id) const;
    // delta > 0: 입고 / delta < 0: 출고
    InventoryItem updateStock(const std::string& sample_id, int delta);
    // 재고 충분 여부 순수 조회 (쓰기 없음)
    bool hasSufficientStock(const std::string& sample_id, int quantity) const;

private:
    InventoryManager&          mgr_;
    std::vector<SampleItem>    samples_;    // 캐시
    std::vector<InventoryItem> inventory_;  // 캐시
};
```

**쓰기 시 캐시 갱신 방식**:
- `addSample`: Manager 호출 → SampleItem 캐시 push_back, InventoryItem 캐시 push_back
- `updateStock`: Manager 호출 → inventory_ 캐시의 해당 항목 수량 갱신
- `removeSample`: Manager 호출 → 두 캐시에서 sample_id 항목 erase

---

### 4.2 `OrderRepository`

```cpp
class OrderRepository {
public:
    explicit OrderRepository(OrderManager& mgr);

    // ── Create ────────────────────────────────────────────────────
    Order add(const std::string& sample_id,
              const std::string& sample_name,
              const std::string& customer_name,
              int                order_quantity);

    // ── Read ──────────────────────────────────────────────────────
    std::vector<Order>   findAll() const;
    std::optional<Order> findByOrderNumber(const std::string& order_number) const;
    std::vector<Order>   findByStatus(OrderStatus status) const;

    // ── Update ────────────────────────────────────────────────────
    Order updateStatus(const std::string& order_number,
                       OrderStatus        new_status,
                       const std::string& note = "");

    // ── Delete ────────────────────────────────────────────────────
    void remove(const std::string& order_number);

private:
    OrderManager&       mgr_;
    std::vector<Order>  orders_;  // 캐시
};
```

---

### 4.3 `ProductionQueueRepository`

`queue_` 캐시는 항상 `queued_at` ASC 정렬 상태를 유지한다.  
새 항목은 항상 현재 시각이 가장 크므로 push_back 후 정렬 불필요.

```cpp
class ProductionQueueRepository {
public:
    explicit ProductionQueueRepository(ProductionQueueManager& mgr);

    // ── Create ────────────────────────────────────────────────────
    ProductionQueueItem enqueue(const std::string& order_number,
                                const std::string& sample_id,
                                const std::string& sample_name,
                                int                planned_quantity);

    // ── Read (모두 queued_at ASC) ─────────────────────────────────
    std::vector<ProductionQueueItem>   findAll() const;
    std::optional<ProductionQueueItem> findById(const std::string& production_id) const;
    // Waiting 항목만 (FIFO 순서)
    std::vector<ProductionQueueItem>   getWaitingQueue() const;
    // Waiting 항목 중 가장 앞 (queued_at 가장 이른 것)
    std::optional<ProductionQueueItem> getFront() const;

    // ── Update ────────────────────────────────────────────────────
    // Waiting → InProduction
    ProductionQueueItem start(const std::string& production_id);

    // ── Delete + Return ───────────────────────────────────────────
    // 생산 완료: 큐에서 제거 후 항목 반환 (Service가 Order 상태 전이에 사용)
    ProductionQueueItem dequeue(const std::string& production_id);

    // Waiting 항목만 취소
    void cancel(const std::string& production_id);

private:
    ProductionQueueManager&          mgr_;
    std::vector<ProductionQueueItem> queue_;  // 캐시, queued_at ASC 유지
};
```

**`dequeue`의 의미**: Persistence의 `complete()`을 호출하여 파일에서 삭제 + 캐시에서 제거 + 완료된 항목 반환.  
"완료 후 Order 상태를 CONFIRMED으로 변경"하는 비즈니스 로직은 Service(Phase 4)가 담당.

---

## 5. 구현 상세

### 5.1 생성자 — 캐시 로드 패턴

```cpp
// InventoryRepository
InventoryRepository::InventoryRepository(InventoryManager& mgr)
    : mgr_(mgr),
      samples_(mgr.get_all_samples()),
      inventory_(mgr.get_all_inventory())
{}

// OrderRepository
OrderRepository::OrderRepository(OrderManager& mgr)
    : mgr_(mgr),
      orders_(mgr.get_all())
{}

// ProductionQueueRepository
ProductionQueueRepository::ProductionQueueRepository(ProductionQueueManager& mgr)
    : mgr_(mgr),
      queue_(mgr.get_all())  // Manager가 이미 queued_at ASC 정렬하여 반환
{}
```

### 5.2 쓰기 메서드 — Write-through 패턴

```cpp
// ① Manager 먼저 호출 (auto-ID 생성 등 side-effect 발생)
// ② Manager 반환값으로 캐시 갱신
// ③ 최종 값 반환

// 예시: OrderRepository::add
Order OrderRepository::add(...) {
    Order o = mgr_.add(...);      // ① persist → order_number 채번
    orders_.push_back(o);         // ② 캐시 갱신
    return o;                     // ③
}

// 예시: OrderRepository::updateStatus
Order OrderRepository::updateStatus(const std::string& order_number, ...) {
    Order updated = mgr_.update_status(order_number, ...);  // ①
    auto it = std::find_if(orders_.begin(), orders_.end(),  // ② 캐시 갱신
        [&](const Order& o) { return o.order_number == order_number; });
    if (it != orders_.end()) *it = updated;
    return updated;                                          // ③
}
```

### 5.3 `hasSufficientStock` — 순수 조회

```cpp
bool InventoryRepository::hasSufficientStock(
    const std::string& sample_id, int quantity) const
{
    auto inv = findInventoryById(sample_id);
    return inv.has_value() && inv->quantity >= quantity;
}
```

---

## 6. 프로젝트 파일 변경 사항

### 6.1 `SampleOrderSystem.vcxproj` — ItemGroup 추가

```xml
<ItemGroup>
  <ClCompile Include="Repositories\InventoryRepository.cpp" />
  <ClCompile Include="Repositories\OrderRepository.cpp" />
  <ClCompile Include="Repositories\ProductionQueueRepository.cpp" />
</ItemGroup>
<ItemGroup>
  <ClInclude Include="Repositories\InventoryRepository.h" />
  <ClInclude Include="Repositories\OrderRepository.h" />
  <ClInclude Include="Repositories\ProductionQueueRepository.h" />
</ItemGroup>
```

### 6.2 `SampleOrderSystem.vcxproj.filters` — Repositories 필터

```xml
<Filter Include="Repositories">
  <UniqueIdentifier>{A1B2C3D4-0003-0003-0003-000000000003}</UniqueIdentifier>
</Filter>
```

### 6.3 `Tests/SampleOrderSystemTests.vcxproj` — RepositoryTests.cpp 추가

```xml
<ClCompile Include="RepositoryTests.cpp" />
<ClCompile Include="..\SampleOrderSystem\Repositories\InventoryRepository.cpp" />
<ClCompile Include="..\SampleOrderSystem\Repositories\OrderRepository.cpp" />
<ClCompile Include="..\SampleOrderSystem\Repositories\ProductionQueueRepository.cpp" />
```

---

## 7. 테스트 설계 (`Tests/RepositoryTests.cpp`)

Repository 테스트는 Manager를 통해 실제 파일을 사용한다 (TempDir 헬퍼 재사용).  
핵심 검증 포인트: **캐시와 파일 모두 올바르게 갱신되는지** 확인.

| 테스트 클래스 | 주요 케이스 |
|---|---|
| `InventoryRepositoryTest` | addSample, findAll/ById/ByName, hasSufficientStock, updateStock, removeSample, 파일 재시작 후 캐시 재로드 |
| `OrderRepositoryTest` | add, findAll/ByNumber/ByStatus, updateStatus, remove, 캐시-파일 동기화 |
| `ProductionQueueRepositoryTest` | enqueue, findAll(FIFO), getFront, getWaitingQueue, start, dequeue, cancel |

**캐시-파일 동기화 검증 방법**:
```cpp
// 1. Repository A에서 데이터 추가
repoA.add(...);

// 2. 같은 파일을 가리키는 새 Manager + Repository 인스턴스 생성
ManagerType mgr2(same_file_path);
RepositoryType repoB(mgr2);

// 3. repoB에서 데이터 조회 → 파일에 반영됐는지 확인
EXPECT_EQ(repoB.findAll().size(), 1u);
```

---

## 8. 핵심 설계 결정

| 항목 | 결정 | 이유 |
|---|---|---|
| 캐시 전략 | Write-through (`std::vector`) | ConsoleMVC 패턴 준수, 읽기 성능 보장 |
| 쓰기 순서 | Manager 먼저 → 캐시 갱신 | Manager가 auto-ID 생성 등 side-effect 담당 |
| 예외 처리 | Manager 예외 그대로 전파 | Repository는 data-access 계층, catch/변환은 Service 책임 |
| `dequeue` 이름 | `complete` 아닌 `dequeue` | Repository 관점에서 "큐에서 꺼냄", 비즈니스 "완료" 의미는 Service |
| 정렬 유지 | 생성자 로드 시 정렬 완료, push_back으로 유지 | 새 항목은 항상 최신 timestamp → 말단 삽입만으로 FIFO 보장 |
| `hasSufficientStock` | Repository에 위치 | 재고 조회는 data-access, "부족 시 어떻게 할지"는 Service |
| 메서드 명명 | `findAll`, `findById`, `findByStatus` | ConsoleMVC 참고 레포 패턴 준수 |

---

## 9. 완료 기준 (Definition of Done)

| 번호 | 검증 항목 |
|---|---|
| 1 | 빌드 오류 없음 (메인 + 테스트 프로젝트 모두) |
| 2 | Repository 생성 시 Manager 파일에서 정상 로드 |
| 3 | `add` 후 `findAll` 크기 1 증가 확인 |
| 4 | `updateStatus` 후 캐시와 파일 모두 갱신 확인 (재시작 검증) |
| 5 | `hasSufficientStock` 경계값 (정확히 같은 수량) 확인 |
| 6 | `getFront` FIFO 순서 보장 확인 |
| 7 | `dequeue` 후 큐에서 제거 + 반환값 정확성 확인 |
| 8 | `cancel` Waiting만 가능, InProduction 시 예외 전파 확인 |
| 9 | gtest 전체 테스트 통과 (기존 44개 + 신규 Repository 테스트) |

---

## 10. 작업 순서 체크리스트

```
[ ] 1. SampleOrderSystem/Repositories/ 디렉토리 생성
[ ] 2. Repositories/InventoryRepository.h 작성
[ ] 3. Repositories/InventoryRepository.cpp 작성
[ ] 4. Repositories/OrderRepository.h 작성
[ ] 5. Repositories/OrderRepository.cpp 작성
[ ] 6. Repositories/ProductionQueueRepository.h 작성
[ ] 7. Repositories/ProductionQueueRepository.cpp 작성
[ ] 8. SampleOrderSystem.vcxproj — ClCompile/ClInclude 6개 추가
[ ] 9. SampleOrderSystem.vcxproj.filters — Repositories 필터 추가
[ ] 10. Tests/RepositoryTests.cpp 작성
[ ] 11. Tests/SampleOrderSystemTests.vcxproj — Repository 소스 파일 추가
[ ] 12. 빌드 및 gtest 전체 실행 (기존 44개 + 신규 테스트 통과 확인)
```
