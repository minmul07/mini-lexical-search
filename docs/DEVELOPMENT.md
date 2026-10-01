# 개발 환경

## 요구 사항

| 항목 | 버전 | 비고 |
| --- | --- | --- |
| C++ | C++17 이상 | `g++ 13.3.0`에서 확인 |
| CMake | 3.20 이상 | 빌드/테스트 |
| Compiler | GCC / Clang | `-Wall -Wextra -Wpedantic` 적용 |

`cmake`와 `ctest`는 시스템에 설치되어 있다 (`/usr/bin/cmake`, 3.28.3에서 확인).

```bash
cmake --version
```

없는 경우:

```bash
sudo apt install cmake
```

## 빌드

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Release 빌드는 `-DCMAKE_BUILD_TYPE=Release`로 지정한다.

## 실행

```bash
./build/mini_lexical_search start
```

```text
$ ./build/mini_lexical_search start
Mini Lexical Search started.
Type '/help' for commands, '/exit' to quit.
> /index
(not implemented yet) Indexing arrives in P0.
> /search 자료구조
(not implemented yet) Search arrives in P0.
> /exit
```

옵션:

* `-h`, `--help` — 도움말
* `-v`, `--version` — 버전

### 실행 중 명령

* `/index` — 인덱싱 시작 (현재 미구현 안내 출력)
* `/search <검색어>` — 검색 (현재 미구현 안내 출력)
* `/help` — 도움말
* `/exit`, `/quit` — 종료

명령은 표준 입력으로 한 줄씩 전달한다. 빈 줄은 무시하며, 알 수 없는 명령이나 잘못된 인자는 오류를 출력한 뒤 다음 입력을 받는다. 입력 종료(EOF) 시에도 정상 종료한다. 문서 디렉터리를 실행 인자로 받는 방식은 지원하지 않으며, 인덱싱 대상 경로 지정 방식은 추후 정의한다.

## 테스트

```bash
ctest --test-dir build --output-on-failure
```

`BUILD_TESTING=OFF`로 테스트를 끌 수 있다. 검증 범위는 [테스트 가이드](TESTING.md)를 참고한다.

## 디렉터리 구조

```text
mini-lexical-search/
├── CMakeLists.txt
├── include/mini_lexical_search/   # 공개 헤더
├── src/                   # 라이브러리 + 실행 파일 (main.cpp)
├── tests/                 # CTest 기반 테스트
└── docs/                  # PRD 및 개발 문서
```

빌드 산출물은 `build/`에 생성되며 git에서 제외한다.

## 현재 상태

실행 환경 스캐폴딩만 구축된 상태이다.

* 완료: CMake 빌드, `mini_lexical_search` 실행 파일, 버전 헤더, CTest 스모크 테스트
* 미구현: Tokenizer, Line-aware Inverted Index, 검색, Ranking, Top-K (P0)

인덱스 저장·로드는 P1에서 JSON 파일 기반으로 구현할 예정이다. 데이터 저장 위치는 절대 경로 `/data`이며, 실행 시 해당 디렉터리에 대한 읽기·쓰기 권한이 필요하다. 상세 스키마, 파일명 및 사용할 JSON 라이브러리는 추후 정의한다.

## 빌드 옵션

| 옵션 | 기본값 | 설명 |
| --- | --- | --- |
| `CMAKE_BUILD_TYPE` | `Debug` | 빌드 타입 |
| `BUILD_TESTING` | `ON` | 테스트 빌드 여부 |
