# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 빌드 명령어

**MSBuild 경로**: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`

```powershell
$msbuild = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"

# 메인 애플리케이션
& $msbuild SampleOrderSystem/SampleOrderSystem.vcxproj /p:Configuration=Debug /p:Platform=x64

# 테스트 프로젝트
& $msbuild Tests/SampleOrderSystemTests.vcxproj /p:Configuration=Debug /p:Platform=x64

# DummyDataGenerator
& $msbuild DummyDataGenerator/DummyDataGenerator.vcxproj /p:Configuration=Debug /p:Platform=x64

# DataMonitor
& $msbuild DataMonitor/DataMonitor.vcxproj /p:Configuration=Debug /p:Platform=x64
```

컴파일러 요구사항: **Visual Studio 2022**, C++20 (`stdcpp20`), UTF-8 (`/utf-8`)

## 테스트 실행

```powershell
# 전체 테스트 실행 (현재 114개)
& "Tests/x64/Debug/SampleOrderSystemTests.exe"

# 특정 테스트 필터
& "Tests/x64/Debug/SampleOrderSystemTests.exe" --gtest_filter="OrderServiceTest.*"
& "Tests/x64/Debug/SampleOrderSystemTests.exe" --gtest_filter="IntegrationScenarioB.*"
```

테스트 파일별 커버리지:
- `DataPersistenceTests.cpp` — Persistence 계층 (Manager 3종)
- `RepositoryTests.cpp` — Repository 계층 (캐시-파일 동기화 포함)
- `ServiceTests.cpp` — Service 계층 (비즈니스 로직, 상태 전이)
- `IntegrationTests.cpp` — 엔드투엔드 시나리오 A/B/C

## 도구 실행

```powershell
# 더미 데이터 생성 (솔루션 루트 data/ 에 직접 기록)
& "DummyDataGenerator/x64/Debug/DummyDataGenerator.exe"

# 실시간 모니터링 (대화형, q로 종료)
& "DataMonitor/x64/Debug/DataMonitor.exe"

# 다른 data 경로 지정
& "DataMonitor/x64/Debug/DataMonitor.exe" "C:\path\to\data"
```

메인 앱 데이터 반영: `data/*.json`을 `SampleOrderSystem/x64/Debug/data/`에 복사.

## 아키텍처 개요

**5계층 MVC** — `AppController`가 모든 의존성을 소유하고 생성자 초기화 리스트에서 조립.

```
View → Controller → Service → Repository → Persistence(Manager) → JSON 파일
```

### 계층별 역할

| 계층 | 위치 | 역할 |
|---|---|---|
| **Model** | `SampleOrderSystem/Models/` | 순수 데이터 구조. `Enums.h`에 `OrderStatus`(5값), `ProductionQueueStatus`(2값) + `toString()` / `toJsonString()` / `*FromJson()` |
| **Persistence** | `SampleOrderSystem/Persistence/` | nlohmann/json 기반 파일 CRUD. Manager 3종(`InventoryManager`, `OrderManager`, `ProductionQueueManager`) |
| **Repository** | `SampleOrderSystem/Repositories/` | Write-through 캐시(`std::vector`). 생성 시 Manager에서 로드, 쓰기 시 Manager 먼저 호출 → 캐시 갱신 |
| **Service** | `SampleOrderSystem/Services/` | 핵심 비즈니스 로직. 변경 연산은 `bool` 반환, Repository 예외를 catch하여 변환 |
| **Controller** | `SampleOrderSystem/Controllers/` | View 입력 수집 → Service 호출 → View 출력. Repository 직접 호출 금지 |
| **View** | `SampleOrderSystem/Views/` | 콘솔 I/O 전담. 전부 `static` 메서드. `MainMenuView`에 UTF-8 폭 유틸(`padRight`/`padLeft`/`truncate`) 집중 |

### 의존성 조립 순서 (AppController 생성자)

C++는 멤버 **선언 순서**로 초기화하므로 `AppController.h`의 멤버 선언 순서가 의존성 위상 정렬을 반영해야 한다.

```
Managers → Repositories → invSvc_ → prodSvc_ → ordSvc_ → Sub-controllers
```

### 주문 상태 전이

```
RESERVED → CONFIRMED  (승인 + 재고 충분, 재고 즉시 차감)
         → PRODUCING  (승인 + 재고 부족, 생산 큐 등록)
         → REJECTED   (거절)
PRODUCING → CONFIRMED  (생산 완료: addStock + deductStock + 상태 갱신)
CONFIRMED → RELEASE    (출고 처리)
```

### 데이터 영속성

- JSON 파일 3개: `data/inventory.json`, `data/orders.json`, `data/production_queue.json`
- `inventory.json`은 `SampleItem` + `InventoryItem`을 **병합 레코드**로 저장 (`sample_id` 기준 1:1)
- ID 채번: `ORD-YYYYMMDD-XXXX`, `PROD-YYYYMMDD-XXXX`
- 빌드 시 `data/`가 출력 디렉토리(`x64/Debug/data/`)에 자동 복사됨 (CopyDataDir post-build 타겟)

### 생산 큐 FIFO

`queued_at` ISO 8601 타임스탬프 기준 `std::stable_sort` (동일 타임스탬프 시 삽입 순서 보존)

## 코드 규칙

- **멤버 변수**: `trailing underscore` (`orderService_`, `orderRepo_`)
- **새 계층 추가 순서**: Model → Persistence → Repository → Service → View → Controller → AppController DI 연결
- **열거형 직렬화**: JSON 저장 시 `toJsonString(enum)`, 화면 출력 시 `toString(enum)` (둘 다 `Enums.h`에 정의)
- **한글 테이블 정렬**: `MainMenuView::padRight/padLeft/truncate`는 UTF-8 표시 폭 기반 (한글 1자 = 화면 2칸)
- **ANSI 색상**: `namespace Clr` (MainMenuView.h) 또는 DataMonitor.cpp 내 익명 namespace 사용

## 프로젝트 구조

```
SampleOrderSystem/          ← 솔루션 루트
├── SampleOrderSystem.slnx  ← 4개 프로젝트 포함
├── data/                   ← 런타임 JSON 파일 (솔루션 공유)
├── include/nlohmann/       ← json.hpp v3.11.3 (헤더 온리)
├── packages/gmock.1.11.0/  ← gtest/gmock 소스 (NuGet 추출)
├── docs/                   ← PRD.md + phase0~6-design.md
├── SampleOrderSystem/      ← 메인 앱 (MVC 5계층)
├── Tests/                  ← gtest 프로젝트 (114개 테스트)
├── DummyDataGenerator/     ← 테스트 데이터 생성 도구
└── DataMonitor/            ← 실시간 콘솔 모니터 (백그라운드 스레드)
```

## 문서

- **전체 설계**: [`docs/PRD.md`](docs/PRD.md) — 비즈니스 흐름, 메뉴 구조, 아키텍처
- **Phase별 상세 설계**: `docs/phase0-design.md` ~ `docs/phase6-design.md`
