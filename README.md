# Sample Server Project

> **문서 작성**: Claude Haiku 4.5

## 개요

개인적으로 작업하던 코드에서 일부를 가져와 구성한 모듈 기반 C++ 서버 프레임워크입니다. 
플러그인 아키텍처를 기반으로 각 기능을 독립적인 DLL 모듈로 구성하였으며, JSON 설정 파일을 통해 동적으로 로드 및 관리합니다.

## 기술 스택

- **언어**: C++ 23 (`stdcpplatest`)
- **개발 환경**: Visual Studio 2026 (v145)
- **빌드 시스템**: MSBuild (.vcxproj)
- **패키징/직렬화**: zpp_bits (high-performance binary serialization)
- **JSON 처리**: nlohmann/json
- **웹 프레임워크**: C++ REST SDK (Casablanca)
- **네트워크**: ASIO (Asynchronous I/O)
- **HTTP 클라이언트**: curl
- **스케줄링**: croncpp

## 프로젝트 구조

### 디렉토리 구성
```
sample/
├── business_common_lib/           # 비즈니스 공통 라이브러리
├── code_gen/                      # 코드 생성 도구 (C#)
├── common/                        # 공용 유틸리티 라이브러리
├── config/                        # 모듈 설정 파일들
├── network/                       # 네트워크 모듈 (DLL)
├── resource/                      # 리소스 파일 (형상관리에 없음. 설정의 resource root 경로에 필요)
├── sdk/                           # 외부 라이브러리 SDK
├── server/                        # 서버 모듈 (DLL)
├── test_client/                   # 테스트 클라이언트 (DLL)
├── timer/                         # 타이머 모듈 (DLL)
├── web/                           # 웹/REST API 모듈 (DLL)
├── x64/                           # 빌드 출력 (Debug/Release, 형상관리에 없음)
├── sample.sln                     # Visual Studio 솔루션
├── sample.cpp/.vcxproj            # 메인 애플리케이션
├── sample_*_debug*.bat            # 디버그 실행 스크립트
├── README.md                      # 이 문서
└── 경력기술서.pdf                # 포트폴리오 문서
```

### 핵심 모듈

#### 1. **business_common_lib** (비즈니스 공통 라이브러리)
- 서버 및 비즈니스 로직에서 공유하는 기본 클래스 및 구조체 정의
- `ServerPacketBase`: 서버 패킷의 기본 클래스
- `ServerSessionBase`: 서버 세션 관리의 기초 클래스
- zpp_bits 기반 패킷 핸들러

#### 2. **network** (네트워크 모듈)
- TCP/IP 기반 연결 관리 (`ConnectionManager`)
- 비동기 I/O를 통한 고성능 소켓 통신
- 클라이언트 연결 및 서버-간 통신 지원
- `NetworkAccessor`: 네트워크 기능 인터페이스
- **strand 기반 스레드 안전성**
  - `SocketConnectionImpl`: 소켓 조작 개시와 종료를 `m_strand`로 직렬화. `Close()`는 호출자에게 비동기이며, 종료 통지(`ConnectionManager::OnClosed`)까지 strand 위에서 실행되어 상위 → 하위 재진입 데드락을 방지
  - `Listener`: `asio::ip::tcp::acceptor`는 스레드 안전하지 않으므로 accept 개시/종료를 `m_acceptStrand`로 직렬화. Debug 빌드에서 `CHECK_ACCEPT_STRAND`로 strand 밖 호출을 감지
  - closedHandler는 `ConnectionManager`가 연결과 함께 보관하고 연결 제거 시점에 호출

#### 3. **server** (게임/애플리케이션 서버)
- 메인 서버 로직
- 사용자 세션 관리 (`UserSession`)
- 서버 간 세션 관리 (`ServerSession`)
- 타이머 작업 관리자 통합
- REST API 핸들러 연동

#### 4. **web** (웹/REST API 모듈)
- C++ REST SDK 기반 RESTful API 서버
- HTTP 요청 핸들링
- JSON 기반 요청/응답 처리
- `WebAccessor`: 웹 기능 인터페이스
- **기본 API**: `/shutdown` - 서버 정상 종료 (GET 요청)

#### 5. **timer** (타이머 모듈)
- 스케줄된 작업 관리
- `TimerJobManager`: 타이머 작업 정렬 및 실행
- croncpp 기반 주기적 작업 지원
- `TimerAccessor`: 타이머 기능 인터페이스

#### 6. **test_client** (테스트 클라이언트)
- 서버 기능 테스트용 클라이언트
- 프로토콜 테스트 및 통신 검증

#### 7. **common** (공용 유틸리티)
- 애플리케이션 라이프사이클 관리 (`Application`)
- 메모리 풀 관리 (`MemoryPool`)
- 로거 (`Logger`)
- 모듈 기본 클래스 (`Module`)
- 종료 조율 (`ShutdownCoordinator`, `UseShutdown`) - [종료 처리](#종료-처리) 참고
- 스레드 풀 (`ThreadPool`), 직렬 작업 큐 (`SerializedJobQueue`)
- 에러 처리 및 타입 정의
- 수학, 시간, 문자열 유틸리티
- 치트 커맨드 (`CheatCommandProcessor`) - 개발/디버그용 커맨드 (현재 사용처 없음)
- 강한 타입 ID (`StrongId` / `DEFINE_STRONG_ID`), 난수 유틸리티

#### 8. **sample** (메인 애플리케이션)
- 설정 파일을 받아 모듈 로드 및 초기화
- 애플리케이션 라이프사이클 관리 (`Application` 클래스 사용)
- 공통 초기화 (`InitCommon`) 및 비즈니스 초기화 (`InitBusiness`) 단계 실행
- 종료 신호까지 대기 (메인 루프)
- 사용법: `sample.exe <config_file_path>`

### Code Generation (codegen)

> **개발**: Claude Haiku 4.5

C# 기반 코드 생성 도구로, JSON 정의 파일을 입력받아 C++ 패킷 구조체 자동 생성합니다.

**주요 기능**:
- **ZppBitsPacketCodegen**: zpp_bits 직렬화 기반 패킷 구조체 생성
- 입력: JSON 형식의 패킷 정의 파일
- 출력: 헤더 파일 형식의 C++ 패킷 클래스
- 자동 직렬화/역직렬화 코드 생성
- 서버/클라이언트 패킷 타입 구분

**사용법**:
```
code_gen.exe packet <input_directory> <output_directory>
```

**예시 입력 (JSON)**:
```json
{
  "namespace": "game",
  "packets": [
    {
      "name": "LoginRequest",
      "type": "client",
      "fields": [
        { "name": "userId", "type": "uint64_t" },
        { "name": "token", "type": "std::string" }
      ]
    }
  ]
}
```

## 모듈 아키텍처

### 플러그인 시스템
각 모듈은 독립적인 DLL로 컴파일되어 런타임에 동적 로드됩니다.

```
Application
├── Module Interface (추상 기본 클래스)
│   ├── network.dll (네트워크 통신)
│   ├── web.dll (REST API)
│   ├── timer.dll (스케줄링)
│   └── server.dll (비즈니스 로직)
└── Configuration (JSON)
    └── 모듈별 설정 파일
```

### 설정 시스템
JSON 기반 설정으로 모듈 로드 및 초기화 관리:

**예시 (sample_1.config)**:
```json
{
  "application": {
    "name": "Sample Server",
    "config root": "./config/",
    "resource root": "./config/"
  },
  "modules": [
    { "dll": "network.dll", "config": "network.config", "use": true },
    { "dll": "web.dll", "config": "web.config", "use": true },
    { "dll": "timer.dll", "config": "timer.config", "use": true },
    { "dll": "server.dll", "config": "first_server.config", "use": true }
  ]
}
```

## 빌드 및 실행

### 개발 환경
- Visual Studio 2026 (v145)
- Windows 11 이상
- x64 플랫폼

### 사전 준비

#### 코드 생성 도구

`business_common_lib`의 사전 빌드 단계가 `code_gen.exe`로 패킷 헤더를 생성하므로 코드 생성 도구를 먼저 빌드합니다.

```bash
dotnet build code_gen/code_gen.csproj -c Release
```

### 빌드
```bash
# Visual Studio에서 sample.sln 열기
# 또는 msbuild 커맨드라인 사용
msbuild sample.sln /p:Configuration=Release /p:Platform=x64
```

### 실행

실행 파일은 `sample.exe` 하나이며, 어떤 모듈을 올릴지는 인자로 받은 설정 파일이 결정합니다.

```bash
# 실행 예제
sample_server_debug_1.bat
sample_server_debug_2.bat
sample_test_client_debug.bat
```

### 정상 종료

```bash
http://127.0.0.1:30001/shutdown
```

`/shutdown` 을 받은 서버는 접속된 다른 서버에 `ServerCommon::Shutdown` 패킷을 브로드캐스트한 뒤 자신을 종료합니다.
패킷을 받은 서버는 `ShutdownApplicationByRemote()` 로 동일한 종료 절차를 밟습니다.

## 종료 처리

모듈과 그 하위 객체들을 정해진 순서로, 서로의 스레드를 기다려가며 안전하게 내리기 위한 구조입니다.

### ShutdownCoordinator

종료 작업을 단계 큐에 쌓아두고 한 스레드에서 순서대로 실행합니다.

- **단계 결과**: `Done`(완료) / `Wait`(미완료 - 같은 단계를 다시 호출)
- **대기 로그**: 1초 이상 머무는 단계만 `shutdown : waiting - <단계명> <상세>` 로 보고하여, 어디서 멈췄는지 즉시 드러남
- **`Run()` 실행 스레드 제약**: 종료 대상이 소유한 스레드에서 호출하면 자기 자신을 join 하게 되므로 금지

### 종료 상태

각 종료 대상(`UseShutdown` 파생 객체)은 아래 상태를 앞으로만 진행합니다.

| 상태 | 의미 |
|------|------|
| `Running` | 정상 동작 중 |
| `Requested` | 종료 요청됨. 새 작업 유입 차단 |
| `Stopping` | 스레드를 종료시켜도 되는 시점 |
| `Completed` | 해당 대상의 모든 단계 완료 |

- `Push()` - `Requested` 시점에 실행할 단계 등록 (작업 배수, 연결 정리 등)
- `PushStopping()` - `Stopping` 시점에 실행할 단계 등록 (스레드 release/join)
- 상태는 되돌아가지 않으므로, 이미 올라간 상태보다 낮은 단계를 등록하면 오류로 보고
- `TargetSetter`가 중첩되어 상위 대상이 하위 대상의 단계를 자기 단계 중간에 끼워 넣을 수 있음 (예: `ConnectionManager` → `ThreadPool`)

### 사용 방법

종료 대상은 `UseShutdown`을 상속하고 `RegisterShutdownSteps()`만 구현합니다.

```cpp
class Foo : public UseShutdown
{
protected:
    virtual void RegisterShutdownSteps(ShutdownCoordinator& _coordinator) override
    {
        _coordinator.Push("foo drain",
            [this]() { return m_jobs.empty() ? EStepResult::Done : EStepResult::Wait; },
            [this]() { return std::format("remain={}", m_jobs.unsafe_size()); });

        _coordinator.PushStopping("foo join threads", [this]()
            {
                for (auto& thread : m_threads) { if (thread.joinable()) thread.join(); }
                return EStepResult::Done;
            });
    }
};
```

모듈은 `Shutdown()`에서 소유한 대상들을 등록한 뒤 한 번에 실행합니다.

```cpp
void Timer::Shutdown()
{
    m_timerJobManagerAllocator.Shutdown(m_shutdownCoordinator, "timer ticker allocator shutdown.");

    m_shutdownCoordinator.Run();
}
```

`Module::Shutdown()`은 `protected`이며 `Application`만 호출할 수 있습니다. 모듈이 소유한 스레드에서 호출되면 self join 이 되기 때문입니다.

### 작업 유입 차단

`SerializedJobQueue`는 종료 대상이 아니라 **입구만 닫는** 방식입니다.
`StopPush()` 이후의 `PushJob()`은 무시되고, 남은 작업은 소멸자에서 정리됩니다.
세션(`UserSession`, `ServerSession`)의 `Closed()`가 이 경로를 사용합니다.

### 종료 순서

```
Application 종료 요청 (restful /shutdown 또는 원격 ServerCommon::Shutdown 패킷)
  └ ShutdownBusiness()   # server, test_client
      ├ public 리스너 정지 → public 연결이 비워질 때까지 대기
      ├ 서버 세션 종료 → 세션이 비워질 때까지 대기
      ├ 타이머 작업 관리자 종료
      └ 스레드 풀 배수 → release → join
  └ ShutdownCommon()     # network, web, timer
      ├ web    : 진행 중 요청 소진 → 대기 요청 응답 → 리스너 close
      ├ timer  : 관리자별 tick 스레드 release → join
      └ network: listener / connecter / imn 순으로 연결 정리 후 스레드 풀 종료
```

## 세션 관리

### UserSession
- 클라이언트의 연결을 대표하는 세션
- 사용자 데이터 및 상태 유지
- 메시지 핸들링

### ServerSession
- 서버 간 통신을 위한 세션
- 다중 서버 환경에서 서버-서버 연결 관리

## 메모리 관리

- **MemoryPool**: 고성능 메모리 할당/해제
- **InstancePool**: 객체 풀링 지원
- STL 커스텀 할당자: 메모리 풀과 연동

## 외부 라이브러리

| 라이브러리 | 버전 | 용도 |
|-----------|------|------|
| ASIO | 1.38.0 | 네트워크 비동기 I/O |
| C++ REST SDK | - | RESTful API 서버 |
| nlohmann/json | 3.11.2 | JSON 파싱 및 생성 |
| zpp_bits | - | 고성능 직렬화 |
| curl | 8.5.0 | HTTP 클라이언트 |
| croncpp | - | 주기적 작업 스케줄링 |
| magic_enum | - | 열거형 이름 문자열화 (로그/포맷터) |

## 프로젝트 구성 파일

### 메인 솔루션 및 프로젝트
- **sample.sln**: 메인 Visual Studio 솔루션
- **sample.vcxproj**: 메인 애플리케이션 프로젝트
- **sample.vcxproj.filters**: 프로젝트 필터 설정

### 모듈 프로젝트
- **business_common_lib.vcxproj**: 비즈니스 공통 라이브러리
- **network.vcxproj**: 네트워크 모듈
- **server.vcxproj**: 서버 모듈
- **web.vcxproj**: 웹/REST API 모듈
- **timer.vcxproj**: 타이머 모듈
- **test_client.vcxproj**: 테스트 클라이언트

### 코드 생성 도구
- **code_gen.sln**: 코드 생성 솔루션
- **code_gen.csproj**: 코드 생성 도구 프로젝트

### 빌드 출력
- **x64/**: 컴파일된 바이너리 및 결과물 (Debug/Release 구성)

## 디버그 설정

워크스페이스에 포함된 실행 스크립트는 모두 `sample.exe`에 설정 파일 경로를 인자로 넘깁니다.
경로가 저장소 루트 기준 상대 경로이므로 체크아웃 위치와 무관하게 동작하지만, 루트에서 실행해야 합니다.

| 스크립트 | 설정 파일 | 올라가는 모듈 |
|---|---|---|
| `sample_server_debug_1.bat` | `config/sample_1.config` | network, web, timer, server (`first_server.config`) |
| `sample_server_debug_2.bat` | `config/sample_2.config` | network, web, timer, server (`second_server.config`) |
| `sample_test_client_debug.bat` | `config/test.config` | network, timer, test_client |

```bat
.\x64\Debug\sample.exe .\config\sample_1.config
```

---

**마지막 업데이트**: 2026년 8월 13일  
**문서 작성자**: Claude Haiku 4.5 (종료 처리 및 빌드/실행 항목 개정: Claude Opus 5)
