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
> /index documents/recap
Saved <문서 수> documents to .data/documents.json
> /search 자료구조
(not implemented yet) Search arrives in P0.
> /exit
```

옵션:

* `-h`, `--help` — 도움말
* `-v`, `--version` — 버전

### 실행 중 명령

* `/index [디렉터리]` — `.txt`·`.md` 문서 목록을 `.data/documents.json`에 저장 (기본 입력 경로: `./documents`)
* `/search <검색어>` — 검색 (현재 미구현 안내 출력)
* `/help` — 도움말
* `/exit`, `/quit` — 종료

명령은 표준 입력으로 한 줄씩 전달한다. 빈 줄은 무시하며, 알 수 없는 명령이나 잘못된 인자는 오류를 출력한 뒤 다음 입력을 받는다. 입력 종료(EOF) 시에도 정상 종료한다. 문서 디렉터리는 실행 인자 대신 `/index`의 인자로 지정한다. 명령 뒤 나머지 문자열 전체를 경로로 해석하므로 공백이 포함된 경로도 따옴표 없이 입력한다. 경로 양 끝의 공백은 제거한다. 수집·저장 실패 시 오류를 출력하고 앱은 계속 실행된다.

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

실행 환경과 문서 메타데이터 저장을 구현한 상태이다.

* 완료: CMake 빌드, `mini_lexical_search` 실행 파일, 버전 헤더, Document 수집·JSON 저장, CTest 테스트
* 미구현: Tokenizer, Line-aware Inverted Index, 검색, Ranking, Top-K (P0)

`src/document.cpp`는 재귀 문서 수집과 Document 배열의 JSON 저장을 담당한다. 하위 디렉터리를 포함해 소문자 `.txt`·`.md` 일반 파일만 수집하며 심볼릭 링크는 제외한다. 절대 경로순으로 정렬하고 ID를 0부터 부여한다. `title`은 확장자를 포함한 파일명이며 `tokenCount`는 항상 0이다. 문서 원문은 읽지 않는다.

출력은 현재 작업 디렉터리 기준 `.data/documents.json`이며 매 실행마다 문서 목록 전체를 교체한다. `.data/`가 없으면 생성을 시도하므로 현재 작업 디렉터리 또는 `.data/`의 쓰기 권한이 필요하다. 완성된 임시 JSON 파일을 교체하여 저장 실패 시 기존 파일을 유지한다. 동시 저장은 `documents.json.tmp` 임시 디렉터리로 차단한다. 프로세스가 강제 종료되어 해당 디렉터리가 남은 경우, 실행 중인 저장 작업이 없는지 확인하고 제거한 뒤 재시도한다. 문자열 이스케이프를 처리하는 전용 직렬화기를 사용하므로 외부 JSON 라이브러리가 필요하지 않으며, 파일 경로는 UTF-8을 전제로 한다. 역색인 저장 및 JSON 로드는 미구현이다.

## 빌드 옵션

| 옵션 | 기본값 | 설명 |
| --- | --- | --- |
| `CMAKE_BUILD_TYPE` | `Debug` | 빌드 타입 |
| `BUILD_TESTING` | `ON` | 테스트 빌드 여부 |
