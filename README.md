# Sample Server Project

> **문서 작성**: Claude Haiku 4.5

## 개요

개인적으로 작업하던 코드에서 일부를 가져와 구성한 모듈 기반 C++ 서버 프레임워크입니다. 
플러그인 아키텍처를 기반으로 각 기능을 독립적인 DLL 모듈로 구성하였으며, JSON 설정 파일을 통해 동적으로 로드 및 관리합니다.

## 기술 스택

- **언어**: C++ 23
- **개발 환경**: Visual Studio 2022
- **빌드 시스템**: MSBuild (.vcxproj)
- **패키징/직렬화**: zpp_bits (high-performance binary serialization)
- **JSON 처리**: nlohmann/json
- **웹 프레임워크**: C++ REST SDK (Casablanca)
- **네트워크**: ASIO (Asynchronous I/O)
- **HTTP 클라이언트**: curl
- **스케줄링**: croncpp

## 프로젝트 구조

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
- 에러 처리 및 타입 정의
- 수학, 시간, 문자열 유틸리티

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
    "config root": "../../config/",
    "resource root": "../../resource/"
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

### 필수 요구사항
- Visual Studio 2022 (C++23 지원)
- Windows 10 이상
- x64 플랫폼

### 빌드
```bash
# Visual Studio에서 sample.sln 열기
# 또는 msbuild 커맨드라인 사용
msbuild sample.sln /p:Configuration=Release /p:Platform=x64
```

### 실행
```bash
# 설정 파일 경로를 인자로 전달
sample.exe config/sample_1.config

# 또는
first_server.exe config/first_server.config
second_server.exe config/second_server.config
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

## 프로젝트 구성 파일

- **sample.sln**: 메인 Visual Studio 솔루션
- **sample.vcxproj**: 메인 서버 프로젝트
- **sample.vcxproj.user**: 실행 설정 (디버그 설정 등)
- **code_gen.sln**: 코드 생성 솔루션
- **code_gen.csproj**: 코드 생성 도구 프로젝트

## 디버그 설정

워크스페이스에 포함된 .lnk 파일들:
- `sample_server_debug_1.lnk`: 첫 번째 서버 디버그 실행
- `sample_server_debug_2.lnk`: 두 번째 서버 디버그 실행
- `sample_test_client_debug.lnk`: 테스트 클라이언트 디버그 실행

---

**마지막 업데이트**: 2026년 5월 13일  
**문서 작성자**: Claude Haiku 4.5
