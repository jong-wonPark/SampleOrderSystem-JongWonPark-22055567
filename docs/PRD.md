# s-semi 시료 관리 시스템 (SampleOrderSystem) — PRD

## 1. 프로젝트 개요

### 1.1 목적
반도체 회사 s-semi의 시료(Sample) 재고·주문·생산을 통합 관리하는 콘솔 기반 C++ 애플리케이션.  
주문 접수 → 생산 담당자 승인 → 재고 확인 → 출고 준비 or 생산 큐 → 출고의 전체 흐름을 처리한다.

### 1.2 참고 레포지토리
| 역할 | 레포지토리 | 적용 내용 |
|---|---|---|
| MVC 구조 골격 | ConsoleMVC-JongWonPark-22055567 | 5계층 MVC, AppController DI 패턴 |
| 데이터 영속성 | DataPersistence-JongWonPark-22055567 | nlohmann/json 기반 Manager 클래스, JSON 파일 CRUD |
| 실시간 모니터 | DataMonitor-JongWonPark-22055567 | 백그라운드 스레드 + 콘솔 대시보드 |
| 테스트 데이터 | DummyDataGenerator-JongWonPark-22055567 | 초기 더미 데이터 생성 도구 |

### 1.3 빌드 환경
- **IDE**: Visual Studio 2022
- **언어 표준**: C++20 (`stdcpp20`)
- **인코딩**: UTF-8 (`/utf-8` 컴파일러 옵션)
- **외부 라이브러리**: nlohmann/json v3.11.3 (헤더 온리)
- **솔루션**: `SampleOrderSystem.slnx` (메인 + DataMonitor + DummyDataGenerator 3개 프로젝트)

---

## 2. 도메인 개념

### 2.1 등장인물 (역할)
| 역할 | 설명 |
|---|---|
| 주문 담당자 | 고객 주문을 시스템에 등록하고, 생산 담당자에게 전달 |
| 생산 담당자 | 주문을 승인/거절하고, 출고 처리를 실행 |
| 시스템 | 재고 확인, 생산 큐 FIFO 관리, 상태 전이 자동 처리 |

### 2.2 핵심 엔티티
| 엔티티 | 설명 |
|---|---|
| `SampleItem` | 시료 마스터 정보 (시료ID, 명칭, 평균 생산시간, 수율) |
| `InventoryItem` | 시료별 현재 재고 수량 |
| `Order` | 고객 주문 (시료ID, 고객명, 주문수량, 상태) |
| `ProductionQueueItem` | 생산 라인에 등록된 생산 요청 (FIFO 순서 유지) |

---

## 3. 비즈니스 흐름 및 상태 전이

### 3.1 주문 전체 흐름
```
[고객]
  │ 시료ID / 고객명 / 주문수량
  ▼
[주문 접수 (RESERVED)]
  │ 주문 담당자 → 생산 담당자에게 전달
  │
  ├─ 거절 ──────────────────────────────→ [거절됨 (REJECTED)]  ★ 종료
  │
  └─ 승인
       │
       ├─ 재고 ≥ 주문수량 ──────────────→ [출고 대기 (CONFIRMED)]
       │   재고 즉시 차감                          │
       │                                          │
       └─ 재고 < 주문수량                          │
            │                                     │
            ▼                                     │
       [생산 중 (PRODUCING)]                       │
            │ 생산 큐(FIFO) 등록                   │
            │ 생산 완료 → 재고 차감                 │
            └─────────────────────────────────────┘
                                                   │
                                              생산 담당자가
                                              출고 처리
                                                   │
                                                   ▼
                                            [출고 완료 (RELEASE)]  ★ 종료
```

### 3.2 OrderStatus 열거형
```
RESERVED   // 주문 접수 (주문 담당자가 등록, 생산 담당자 검토 대기 포함)
REJECTED   // 주문 거절 (최종)
PRODUCING  // 주문 승인 완료 및 재고 부족으로 생산 중
CONFIRMED  // 주문 승인 완료 및 출고 대기 중 (재고 확보 완료)
RELEASE    // 출고 완료 (최종)
```

### 3.3 상태 전이 규칙 요약
| 현재 상태 | 다음 상태 | 트리거 | 처리 내용 |
|---|---|---|---|
| `RESERVED` | `REJECTED` | 생산 담당자 거절 | 거절 사유 기록 |
| `RESERVED` | `CONFIRMED` | 생산 담당자 승인 + 재고 충분 | 재고 즉시 차감 |
| `RESERVED` | `PRODUCING` | 생산 담당자 승인 + 재고 부족 | 생산 큐(FIFO) 말단에 등록 |
| `PRODUCING` | `CONFIRMED` | 생산 완료 | 큐에서 제거, 재고 차감 |
| `CONFIRMED` | `RELEASE` | 생산 담당자 출고 처리 | 출고 완료 기록 |

### 3.4 생산 큐 (FIFO)
- `PRODUCING` 상태 주문은 반드시 생산 큐에 1:1 대응하는 `ProductionQueueItem`을 가진다.
- 처리는 항상 큐 앞(front)에서 시작한다 (`queued_at` 기준 오름차순).
- 큐 항목 상태: `Waiting` → `InProduction` → 완료 시 큐에서 제거 + 연결 Order를 `CONFIRMED`으로 갱신.
- `Waiting` 상태에 한해 취소 가능 (Order도 함께 `RESERVED`로 롤백하지 않고 별도 처리).

---

## 4. 메인 메뉴 구조

### 4.1 메뉴 화면 레이아웃
```
============================================
     s-semi 시료 관리 시스템
============================================
 1. 시료 관리
 2. 시료 주문
 3. 주문 승인/거절
 4. 모니터링
 5. 출고 처리
 6. 생산 라인
 0. 종료
--------------------------------------------
메뉴를 선택하세요:
```

### 4.2 메뉴별 기능 상세

| 번호 | 메뉴명 | 세부 기능 | 담당 Controller / View |
|---|---|---|---|
| 1 | **시료 관리** | 새로운 시료 등록 / 시료 목록 조회 / 이름으로 검색 | `SampleController` / `SampleView` |
| 2 | **시료 주문** | 고객 주문 접수 (시료ID·고객명·수량 입력 → RESERVED 생성) | `OrderController` / `OrderView` |
| 3 | **주문 승인/거절** | RESERVED 주문 목록 표시 → 승인(재고 확인 분기) or 거절 | `OrderController` / `OrderView` |
| 4 | **모니터링** | 상태별 주문 건수 요약 + 시료별 재고 현황 테이블 | `MonitoringController` / `MonitoringView` |
| 5 | **출고 처리** | CONFIRMED 상태 주문 목록 표시 → 출고 실행(→ RELEASE) | `ProductionController` / `ProductionView` |
| 6 | **생산 라인** | 생산 중(InProduction) 항목 + 대기 큐(Waiting) 목록 확인 및 생산 시작·완료 처리 | `ProductionController` / `ProductionView` |
| 0 | **종료** | 프로그램 정상 종료 | `AppController` |

### 4.3 메뉴별 구현 Phase 매핑

| 메뉴 | 첫 완성 Phase | 핵심 구현 내용 |
|---|---|---|
| 시료 관리 | **Phase 6** | Phase 1 `SampleItem` 모델 → Phase 2 `InventoryManager` → Phase 3 `InventoryRepository` → Phase 4 `InventoryService` → Phase 5 `SampleView` → Phase 6 `SampleController` |
| 시료 주문 | **Phase 6** | Phase 1 `Order` 모델 → Phase 2 `OrderManager` → Phase 3 `OrderRepository` → Phase 4 `OrderService::placeOrder` → Phase 5 `OrderView` → Phase 6 `OrderController` |
| 주문 승인/거절 | **Phase 6** | Phase 4 `OrderService::approveOrder / rejectOrder`(상태 전이 핵심) → Phase 5 `OrderView` → Phase 6 `OrderController` |
| 모니터링 | **Phase 6** | Phase 3 Repository 집계 쿼리(`findByStatus` 등) → Phase 4 조회 서비스 → Phase 5 `MonitoringView` → Phase 6 `MonitoringController` |
| 출고 처리 | **Phase 6** | Phase 4 `ProductionService::shipOrder`(CONFIRMED→RELEASE) → Phase 5 `ProductionView` → Phase 6 `ProductionController` |
| 생산 라인 | **Phase 6** | Phase 3 `ProductionQueueRepository::getFront / getWaitingQueue` → Phase 4 `ProductionService::startNextProduction / completeProduction` → Phase 5 `ProductionView` → Phase 6 `ProductionController` |
| 종료 | **Phase 6** | `AppController::run()` 루프 탈출 조건 처리 |

> **요점**: 모든 메뉴는 Phase 6(Controller 계층)에서 처음 완성되며, Phase 4(Service)가 각 메뉴의 핵심 비즈니스 로직을 담당한다.

---

## 6. 아키텍처 설계

### 6.1 계층 구조 (ConsoleMVC 패턴 준수)
```
View ─→ Controller ─→ Service ─→ Repository ─→ Model
                                      │
                                 Persistence
                               (JSON 파일 I/O)
```

| 계층 | 역할 |
|---|---|
| **Model** | 순수 데이터 구조 + 순수 계산 메서드 |
| **Repository** | CRUD + 필터 쿼리. 비즈니스 로직 없음. 인메모리 `std::vector` |
| **Persistence** | nlohmann/json 기반 JSON 파일 읽기/쓰기. Repository가 시작·종료 시 호출 |
| **Service** | 핵심 비즈니스 로직. 여러 Repository 조율 |
| **Controller** | 사용자 입력 수집 → Service 호출 → View 결과 전달. Repository 직접 호출 금지 |
| **View** | 콘솔 출력 및 입력 수집만. 비즈니스 로직 없음 |
| **AppController** | 모든 의존성 소유·조립 (생성자 수동 DI) |

### 6.2 의존성 주입 구조 (AppController)
```cpp
// 생성자 초기화 리스트에서 아래 순서로 조립
Repositories: InventoryRepo, OrderRepo, ProductionQueueRepo
    ↓ (reference로 주입)
Services: InventoryService, OrderService, ProductionService
    ↓ (reference로 주입)
Controllers: SampleController, OrderController, ProductionController, MonitoringController
```

### 6.3 영속성 전략 (DataPersistence 패턴 준수)
- JSON 파일 3개를 `data/` 디렉토리에 저장
- 앱 시작 시 JSON → 인메모리 로드, 변경 시마다 JSON에 즉시 저장
- 타임스탬프: ISO 8601 형식 (`YYYY-MM-DDTHH:MM:SS`)
- ID 자동 채번: `ORD-YYYYMMDD-XXXX`, `PROD-YYYYMMDD-XXXX`

---

## 7. 디렉토리 구조 및 파일 목록

```
SampleOrderSystem/              ← 솔루션 루트
├── SampleOrderSystem.slnx
├── docs/
│   └── PRD.md
├── data/                       ← 런타임 JSON 데이터 파일
│   ├── inventory.json          ← 시료 재고 현황
│   ├── orders.json             ← 주문 목록
│   └── production_queue.json   ← 생산 큐
├── include/
│   └── nlohmann/               ← JSON 라이브러리 (헤더 온리)
│       └── json.hpp
│
├── SampleOrderSystem/          ← 메인 프로젝트 (5계층 MVC)
│   ├── SampleOrderSystem.vcxproj
│   ├── main.cpp
│   │
│   ├── Models/
│   │   ├── Enums.h             ← OrderStatus, ProductionQueueStatus enum
│   │   ├── SampleItem.h        ← 시료 마스터 (시료ID, 명칭, 생산시간, 수율)
│   │   ├── InventoryItem.h     ← 재고 항목 (시료ID, 수량)
│   │   ├── Order.h             ← 주문 (주문번호, 시료ID, 고객명, 수량, 상태)
│   │   └── ProductionQueueItem.h ← 생산 큐 항목 (생산ID, 주문번호, 대기순서)
│   │
│   ├── Persistence/            ← DataPersistence 레이어 (JSON CRUD)
│   │   ├── DataPersistence.h
│   │   └── DataPersistence.cpp
│   │
│   ├── Repositories/           ← 인메모리 CRUD + 필터
│   │   ├── InventoryRepository.h / .cpp
│   │   ├── OrderRepository.h / .cpp
│   │   └── ProductionQueueRepository.h / .cpp
│   │
│   ├── Services/               ← 비즈니스 로직
│   │   ├── InventoryService.h / .cpp
│   │   ├── OrderService.h / .cpp
│   │   └── ProductionService.h / .cpp
│   │
│   ├── Controllers/            ← 입력→서비스→뷰 연결
│   │   ├── AppController.h / .cpp        ← 의존성 조립 & 메인루프 & 종료
│   │   ├── SampleController.h / .cpp     ← 메뉴 1: 시료 관리
│   │   ├── OrderController.h / .cpp      ← 메뉴 2·3: 시료 주문·승인/거절
│   │   ├── MonitoringController.h / .cpp ← 메뉴 4: 모니터링
│   │   └── ProductionController.h / .cpp ← 메뉴 5·6: 출고 처리·생산 라인
│   │
│   └── Views/                  ← 콘솔 입출력
│       ├── MainMenuView.h / .cpp         ← 메인 메뉴 렌더링
│       ├── SampleView.h / .cpp           ← 메뉴 1: 시료 관리 UI
│       ├── OrderView.h / .cpp            ← 메뉴 2·3: 주문·승인 UI
│       ├── MonitoringView.h / .cpp       ← 메뉴 4: 모니터링 UI
│       └── ProductionView.h / .cpp       ← 메뉴 5·6: 출고·생산 라인 UI
│
├── DataMonitor/                ← 실시간 모니터링 도구 (별도 프로젝트)
│   ├── DataMonitor.vcxproj
│   ├── DataMonitor.h
│   ├── DataMonitor.cpp
│   └── main.cpp
│
└── DummyDataGenerator/         ← 테스트 더미 데이터 생성 도구 (별도 프로젝트)
    ├── DummyDataGenerator.vcxproj
    └── src/
        ├── DataPersistence.h   ← 공유 (심볼릭 or 복사)
        ├── DataPersistence.cpp
        └── main.cpp
```

---

## 8. 데이터 모델 상세

### 8.1 SampleItem (시료 마스터)
```cpp
struct SampleItem {
    std::string sample_id;                   // 시료ID (예: "S-001")
    std::string sample_name;                 // 시료명 (예: "실리콘 웨이퍼 200mm")
    double      avg_production_time_hours;   // 평균 생산 시간(시)
    double      yield_rate;                  // 수율 (0.0 ~ 1.0)
};
```

### 8.2 InventoryItem (재고)
```cpp
struct InventoryItem {
    std::string sample_id;
    int         quantity;        // 실재고 수량
    std::string unit;            // 단위 (기본 "EA")
    std::string last_updated;    // ISO 8601
};
```

### 8.3 Order (주문)
```cpp
struct Order {
    std::string  order_number;   // ORD-YYYYMMDD-XXXX (자동 채번)
    std::string  sample_id;      // 시료ID
    std::string  sample_name;    // 시료명 (비정규화, 표시용)
    std::string  customer_name;  // 고객명
    int          order_quantity; // 주문수량
    OrderStatus  status;         // RESERVED / REJECTED / PRODUCING / CONFIRMED / RELEASE
    std::string  order_date;     // 주문 접수 일시 (ISO 8601)
    std::string  note;           // 거절 사유 등 메모 (선택)
};
```

### 8.4 ProductionQueueItem (생산 큐)
```cpp
struct ProductionQueueItem {
    std::string           production_id;  // PROD-YYYYMMDD-XXXX (자동 채번)
    std::string           order_number;   // 연결된 주문번호
    std::string           sample_id;
    std::string           sample_name;
    int                   planned_quantity;
    ProductionQueueStatus status;         // Waiting / InProduction
    std::string           queued_at;      // 큐 등록 시각 (ISO 8601) — FIFO 정렬 기준
    std::string           started_at;     // 생산 시작 시각 (미시작 시 빈 문자열)
};
```

---

## 9. 서비스 계층 핵심 메서드

### 9.1 OrderService
```cpp
// 주문 담당자: 주문 등록 (→ RESERVED)
Order  placeOrder(sample_id, customer_name, quantity);

// 생산 담당자: 승인
//   재고 충분 → InventoryService::deductStock() → CONFIRMED
//   재고 부족 → ProductionService::enqueueProduction() → PRODUCING
bool   approveOrder(order_number);

// 생산 담당자: 거절 (RESERVED → REJECTED)
bool   rejectOrder(order_number, note);
```

### 9.2 ProductionService
```cpp
// approveOrder 내부에서 자동 호출: 재고 부족 시 큐 등록 (Order → PRODUCING)
ProductionQueueItem enqueueProduction(order_number);

// 생산 담당자: 큐 앞(front) 항목 생산 시작 (Waiting → InProduction)
bool   startNextProduction();

// 생산 담당자: 생산 완료 처리
//   → ProductionQueueItem 큐에서 제거
//   → InventoryService::deductStock() (주문량 차감)
//   → 연결 Order를 CONFIRMED으로 갱신
bool   completeProduction(production_id);

// 생산 담당자: 출고 처리 (CONFIRMED → RELEASE)
bool   shipOrder(order_number);
```

### 9.3 InventoryService
```cpp
// 재고 조회
std::optional<InventoryItem> getStock(sample_id);

// 재고 차감 (출고 시)
bool   deductStock(sample_id, quantity);

// 재고 추가 (생산 완료 시 자동 호출 후 즉시 차감)
bool   addStock(sample_id, quantity);
```

---

## 10. 구현 단계 (Phase별 순서)

### Phase 0 — 프로젝트 기반 설정
**목표**: 빌드 환경 구성, 외부 라이브러리 통합

1. `include/nlohmann/json.hpp` 배치 (v3.11.3)
2. `SampleOrderSystem.vcxproj` 설정: C++20, `/utf-8`, 추가 포함 경로 `$(SolutionDir)include`
3. `data/` 초기 JSON 파일 생성 (빈 배열 `[]` or 시드 데이터)
4. `main.cpp` 최소 진입점 작성 (`SetConsoleCP(CP_UTF8)` + `AppController::run()` 호출)

---

### Phase 1 — Model 계층
**목표**: 모든 데이터 구조 정의. 이 단계 이후 전체 계층이 타입을 공유한다.

1. `Models/Enums.h`
   - `OrderStatus` enum class (5개 값: RESERVED, REJECTED, PRODUCING, CONFIRMED, RELEASE) + `toString()` inline 함수
   - `ProductionQueueStatus` enum class (Waiting, InProduction) + `toString()`
2. `Models/SampleItem.h` — 시료 마스터 구조체
3. `Models/InventoryItem.h` — 재고 구조체
4. `Models/Order.h` — 주문 구조체
5. `Models/ProductionQueueItem.h` — 생산 큐 항목 구조체

---

### Phase 2 — Persistence 계층 (DataPersistence 패턴)
**목표**: JSON 파일 ↔ 구조체 직렬화/역직렬화 구현

1. `Persistence/DataPersistence.h` — Manager 클래스 3개 선언
   - `InventoryManager` (inventory.json)
   - `OrderManager` (orders.json)
   - `ProductionQueueManager` (production_queue.json)
2. `Persistence/DataPersistence.cpp` — 각 Manager 구현
   - `to_json()` / `from_json()` 함수 쌍 (모든 구조체)
   - ID 자동 채번 (`ORD-YYYYMMDD-XXXX`, `PROD-YYYYMMDD-XXXX`)
   - `load()` / `save()` private 메서드
3. 각 Manager의 CRUD 메서드 완성:
   - Create: `add()` / `enqueue()`
   - Read: `get_all()`, `get_by_id()`, `get_by_status()`
   - Update: `update_status()`, `update_stock()`
   - Delete: `remove()` / `cancel()`

---

### Phase 3 — Repository 계층
**목표**: Persistence 위에서 인메모리 CRUD + 필터 쿼리 제공

1. `Repositories/InventoryRepository.h/.cpp`
   - 시작 시 `InventoryManager::get_all()`로 로드
   - 변경 시 `InventoryManager`로 즉시 저장
   - `findById()`, `getAll()`, `updateQuantity()`
2. `Repositories/OrderRepository.h/.cpp`
   - `findByStatus()`, `findByOrderNumber()`, `updateStatus()`
3. `Repositories/ProductionQueueRepository.h/.cpp`
   - `getFront()` (큐 앞 항목, `queued_at` 기준 FIFO)
   - `getWaitingQueue()` (대기 목록 순서 보장)
   - `enqueue()`, `dequeue(production_id)`

---

### Phase 4 — Service 계층
**목표**: 핵심 비즈니스 로직 및 상태 전이 구현

1. `Services/InventoryService.h/.cpp`
   - `getStock(sample_id)`, `hasEnoughStock(sample_id, quantity)`
   - `deductStock()`, `addStock()`
2. `Services/OrderService.h/.cpp`
   - `placeOrder()`: Order 생성, status=RESERVED
   - `approveOrder()`: RESERVED → 재고 확인 후 분기
     - 재고 충분 → `InventoryService::deductStock()` → CONFIRMED
     - 재고 부족 → `ProductionService::enqueueProduction()` → PRODUCING
   - `rejectOrder()`: RESERVED → REJECTED
3. `Services/ProductionService.h/.cpp`
   - `enqueueProduction(order_number)`: 큐 말단에 추가, Order → PRODUCING
   - `startNextProduction()`: 큐 front Waiting → InProduction
   - `completeProduction(production_id)`:
     - 큐에서 제거
     - `InventoryService::deductStock()` (주문량 차감)
     - Order status → CONFIRMED
   - `shipOrder(order_number)`: CONFIRMED → RELEASE

---

### Phase 5 — View 계층
**목표**: 콘솔 입출력 UI. 비즈니스 로직 없이 표시·입력만.  
**구현 메뉴**: 모든 메뉴의 화면 출력 담당

1. `Views/MainMenuView.h/.cpp` — 메인 메뉴(0~6번) 렌더링
   - 공통 유틸: `printHeader()`, `printTable()`, `printLine()`, `promptInput()`
2. `Views/SampleView.h/.cpp` — **메뉴 1: 시료 관리**
   - 시료 등록 입력 폼 (시료ID, 명칭, 생산시간, 수율)
   - 시료 목록 테이블 출력
   - 이름 검색 결과 출력
3. `Views/OrderView.h/.cpp` — **메뉴 2·3: 시료 주문·승인/거절**
   - 주문 접수 입력 폼 (시료ID, 고객명, 수량)
   - RESERVED 주문 목록 (승인/거절 대상) 출력
   - 승인·거절 결과 메시지 (CONFIRMED/PRODUCING/REJECTED)
4. `Views/MonitoringView.h/.cpp` — **메뉴 4: 모니터링**
   - 상태별 주문 건수 요약표 (RESERVED/PRODUCING/CONFIRMED/RELEASE/REJECTED)
   - 시료별 재고 현황 테이블 (시료ID, 명칭, 수량)
5. `Views/ProductionView.h/.cpp` — **메뉴 5·6: 출고 처리·생산 라인**
   - CONFIRMED 주문 목록 (출고 대상) 출력
   - 생산 큐 목록 (Waiting/InProduction, FIFO 순서) 출력
   - 생산 시작·완료·출고 결과 메시지

---

### Phase 6 — Controller 계층
**목표**: 입력→서비스→뷰 파이프라인 연결. Repository 직접 호출 금지.

1. `Controllers/SampleController.h/.cpp` — **메뉴 1: 시료 관리**
   - `registerSample()`: SampleView 입력 수집 → InventoryService::addSample()
   - `showSampleList()`: InventoryService::getAll() → SampleView 출력
   - `searchByName()`: InventoryService::findByName() → SampleView 출력
2. `Controllers/OrderController.h/.cpp` — **메뉴 2·3: 시료 주문·승인/거절**
   - `placeOrder()`: OrderView 입력 수집 → OrderService::placeOrder() (→ RESERVED)
   - `showPendingOrders()`: OrderService::getByStatus(RESERVED) → OrderView 출력
   - `approveOrder()`: OrderService::approveOrder() (→ CONFIRMED or PRODUCING)
   - `rejectOrder()`: OrderService::rejectOrder() (→ REJECTED)
3. `Controllers/MonitoringController.h/.cpp` — **메뉴 4: 모니터링**
   - `showDashboard()`: OrderService 상태별 집계 + InventoryService 재고 조회 → MonitoringView 출력
4. `Controllers/ProductionController.h/.cpp` — **메뉴 5·6: 출고 처리·생산 라인**
   - `showConfirmedOrders()`: OrderService::getByStatus(CONFIRMED) → ProductionView 출력
   - `shipOrder()`: ProductionService::shipOrder() (→ RELEASE)
   - `showProductionQueue()`: ProductionService::getQueue() → ProductionView 출력
   - `startProduction()`: ProductionService::startNextProduction()
   - `completeProduction()`: ProductionService::completeProduction() (→ CONFIRMED)
5. `Controllers/AppController.h/.cpp` — **메인 루프 & 종료(0번)**
   - 전체 의존성 소유 (Repository, Service, Controller 인스턴스)
   - 생성자에서 참조로 DI 조립
   - `run()`: 메인 루프, 메뉴 번호(0~6) 입력 → 해당 Controller 호출

---

### Phase 7 — 통합 및 main.cpp 완성
**목표**: 애플리케이션 진입점 완성 및 전체 흐름 검증

1. `main.cpp` 완성
   ```cpp
   SetConsoleCP(CP_UTF8);
   SetConsoleOutputCP(CP_UTF8);
   AppController app;
   app.run();
   ```
2. 전체 상태 전이 흐름 수동 검증:
   - RESERVED → 승인 + 재고 있음 → CONFIRMED → RELEASE
   - RESERVED → 승인 + 재고 없음 → PRODUCING → (생산 완료) → CONFIRMED → RELEASE
   - RESERVED → 거절 → REJECTED
3. JSON 파일 영속성 확인: 앱 재시작 후 데이터 복구

---

### Phase 8 — DummyDataGenerator 도구
**목표**: 개발·테스트용 초기 데이터를 JSON 파일로 자동 생성

1. `DummyDataGenerator/SampleOrderSystem.vcxproj` 설정
2. `DummyDataGenerator/src/main.cpp`
   - 시료 마스터 10종 생성 (다양한 반도체 품목)
   - 초기 재고 랜덤 설정 (일부는 0으로 재고 부족 시나리오 포함)
   - 주문 20건 생성 (RESERVED/PRODUCING/CONFIRMED/REJECTED/RELEASE 혼합)
   - 생산 큐 5건 생성 (Waiting/InProduction 혼합)
3. `DataPersistence.h/.cpp` 공유 처리 (솔루션 내 동일 파일 참조)

---

### Phase 9 — DataMonitor 도구
**목표**: JSON 데이터를 실시간으로 콘솔에 시각화하는 읽기 전용 모니터

1. `DataMonitor/DataMonitor.vcxproj` 설정
2. `DataMonitor/DataMonitor.h` — `DataMonitor` 클래스, `MonitorSnapshot` 구조체
3. `DataMonitor/DataMonitor.cpp`
   - 백그라운드 스레드: 3초 주기로 JSON 파일 재읽기
   - 뷰 4종: Dashboard / 재고 현황 / 주문 목록(상태 필터) / 생산 큐
   - 콘솔 색상 상태 표시, 프로그레스 바
   - 키 입력: `1`~`4` 뷰 전환, `r` 강제 새로고침, `q` 종료
4. `DataMonitor/main.cpp` — 진입점, `DataMonitor::run()` 호출

---

## 11. 코드 컨벤션

| 항목 | 규칙 |
|---|---|
| 멤버 변수 | `trailing underscore` (`orderService_`, `orderRepo_`) |
| Service 반환 | 성공/실패는 `bool` 또는 `std::optional<T>` |
| 한글 출력 | `SetConsoleCP(CP_UTF8)` + `SetConsoleOutputCP(CP_UTF8)` 전제 |
| 새 도메인 추가 | Model → Persistence → Repository → Service → Controller → View 순 |
| 계층 간 직접 접근 | Controller는 Repository 직접 호출 금지, Service를 통해서만 |
| 주석 | WHY가 비자명한 경우에만. WHAT 설명 주석 금지 |
| 상태 문자열 | 직렬화(JSON)는 `constexpr const char*`, 화면 출력은 `toString(enum)` |

---

## 12. 구현 순서 요약 (체크리스트)

```
Phase 0  [ ] 프로젝트 기반 설정 (빌드 환경, nlohmann/json, data/ 초기 파일)
Phase 1  [ ] Model 계층 (Enums, SampleItem, InventoryItem, Order, ProductionQueueItem)
Phase 2  [ ] Persistence 계층 (DataPersistence.h/.cpp, Manager 3종, JSON 직렬화)
Phase 3  [ ] Repository 계층 (Inventory/Order/ProductionQueueRepository)
Phase 4  [ ] Service 계층 (Inventory/Order/ProductionService, 상태 전이 로직)
Phase 5  [ ] View 계층 (MainMenuView, SampleView, OrderView, MonitoringView, ProductionView)
Phase 6  [ ] Controller 계층 (SampleController, OrderController, MonitoringController, ProductionController, AppController DI)
Phase 7  [ ] 통합 및 main.cpp 완성, 전체 흐름 검증
Phase 8  [ ] DummyDataGenerator 도구
Phase 9  [ ] DataMonitor 도구
```
