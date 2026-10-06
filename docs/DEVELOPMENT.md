# 개발 환경

## 요구 사항

| 항목 | 버전 | 비고 |
| --- | --- | --- |
| C++ | ISO C++20 (`-std=c++20`) | `g++ 13.3.0`에서 확인 |
| CMake | 3.20 이상 | 빌드/테스트 |
| OpenSSL | — | SHA-256 계산용 libcrypto 헤더·라이브러리 (`libssl-dev`) |
| Compiler | GCC / Clang | `-Wall -Wextra -Wpedantic` 적용 |

`cmake`와 `ctest`는 시스템에 설치되어 있다 (`/usr/bin/cmake`, 3.28.3에서 확인).

```bash
cmake --version
```

없는 경우:

```bash
sudo apt install cmake libssl-dev
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
updating file metadata..
updated file metadata (2 ms)
0 not updated, 0 updated, 3 added, 0 deleted
tokenizing documents..
tokenized 3 documents (1 ms)
> /search 자료구조
(not implemented yet) Search arrives in P0.
> /exit
```

옵션:

* `-h`, `--help` — 도움말
* `-v`, `--version` — 버전

### 실행 중 명령

* `/index [디렉터리]` — `.txt`·`.md` 문서 메타데이터와 변경된 문서의 토큰화 결과(`tokenCount`)를 `.data/documents.json`에 저장 (기본 입력 경로: `./documents`)
* `/search <검색어>` — 검색 (현재 미구현 안내 출력)
* `/help` — 도움말
* `/exit`, `/quit` — 종료

명령은 표준 입력으로 한 줄씩 전달한다. 빈 줄은 무시하며, 알 수 없는 명령이나 잘못된 인자는 오류를 출력한 뒤 다음 입력을 받는다. 입력 종료(EOF) 시에도 정상 종료한다. 문서 디렉터리는 실행 인자 대신 `/index`의 인자로 지정한다. 명령 뒤 나머지 문자열 전체를 경로로 해석하므로 공백이 포함된 경로도 따옴표 없이 입력한다. 경로 양 끝의 공백은 제거한다. `/index`는 시작 시 `updating file metadata..`, 성공 시 `updated file metadata (<시간> ms)`와 변경 집계를 출력하고, 이어서 `tokenizing documents..`와 `tokenized <문서 수> documents (<시간> ms)`를 출력한다. 메타데이터 시간은 문서 수집·해시 계산·기존 JSON 로드·비교를 포함한 경과 시간이고, 토큰화 시간은 변경된 문서를 읽어 토큰화한 경과 시간이다. 둘 다 정수 밀리초로 표시한다. 변경 없는 문서는 토큰화하지 않으며 `tokenized 0 documents (0 ms)`로 표시된다. 실패 시 `error: indexing failed: <원인>`을 출력하고 앱은 계속 실행된다. 메타데이터 준비 단계에서 실패하면 완료 메시지 없이 오류만 출력하며, 이후 단계(토큰화·저장)에서 실패하면 그 단계 이후의 완료 메시지만 생략된다.

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

실행 환경, 문서 메타데이터 저장, 변경 문서 토큰화를 구현한 상태이다.

* 완료: CMake 빌드, `mini_lexical_search` 실행 파일, 버전 헤더, Document 수집·JSON 저장, SimpleTokenizer 기반 토큰화와 `tokenCount`, 변경 문서 토큰화 게이트, CTest 테스트
* 미구현: Line-aware Inverted Index, 검색, Ranking, Top-K (P0)

`src/document.cpp`는 재귀 문서 수집, Document 배열의 JSON 저장, 그리고 변경 문서 토큰화를 담당한다. 하위 디렉터리를 포함해 소문자 `.txt`·`.md` 일반 파일만 수집하며 심볼릭 링크는 제외한다. 첫 저장 시 절대 경로순으로 정렬하고 ID를 0부터 부여한다. 이후에는 기존 문서 ID를 유지하고 새 문서는 이전 목록의 최대 ID 다음부터 부여한다. `title`은 확장자를 포함한 파일명이다. 문서 원문을 바이너리 스트림으로 읽어 SHA-256 `hash`를 계산한다. `prepareIndex`가 이전 JSON과 비교해 `notUpdated`/`updated`/`added`/`deleted`를 분류하고, 추가·변경된 문서의 인덱스만 `toTokenize`에 담는다. `tokenizeDocuments`는 그 문서들만 열어 `std::getline`으로 줄 단위(1부터 시작)로 읽고 `Tokenizer::tokenize` 결과를 줄 번호와 함께 방출하며, 문서별 토큰 수를 `tokenCount`에 기록한다. 같은 해시·제목인 문서는 재토큰화하지 않고 기존 `tokenCount`를 유지하며, 변경이 감지된 문서는 이전 토큰 결과를 버리고 처음부터 다시 만든다.

`src/tokenizer.cpp`는 `SimpleTokenizer`를 구현한다. ASCII 공백으로 분리하고 ASCII 영문은 소문자로 바꾸며 ASCII 문장부호·제어문자는 제거한다. `0x80` 이상 바이트는 UTF-8 한국어 등을 위해 그대로 유지한다.

`src/document_json.cpp`는 기존 문서 테이블의 로드를 담당한다. 앱 시작 시에는 로드하지 않으며 `/index`마다 JSON을 읽어 메모리에서 절대 경로와 해시를 비교한다. 해시가 없는 기존 스키마는 `updated`로 처리해 해시를 추가한다. JSON이 손상됐거나 경로·ID가 중복되면 저장하지 않고 오류를 출력한다.

출력은 현재 작업 디렉터리 기준 `.data/documents.json`이다. 같은 경로·해시·제목인 문서는 `not updated`(토큰화도 생략), 해시나 제목이 달라진 문서는 `updated`, 새 경로는 `added`, 이번 입력 목록에 없는 이전 경로는 `deleted`로 집계한다. 해시가 없는 기존 스키마는 `updated`로 처리해 해시를 추가한다. 토큰화 트리거는 이 변경 감지와 동일하며, 변경된 문서는 이전 토큰 결과를 버리고 다시 토큰화한다. 동일 내용의 파일도 경로가 다르면 별도 문서이며, 파일 이동은 삭제와 추가로 처리한다. 하나의 테이블을 선택한 디렉터리 목록과 동기화하므로 입력 디렉터리를 바꾸면 이전 디렉터리의 문서도 삭제 대상이다. 모든 문서가 동일하면 JSON 내용과 수정 시간을 유지하고, 변경이 있으면 배열 전체를 저장한다. `.data/`가 없으면 생성을 시도하므로 현재 작업 디렉터리 또는 `.data/`의 쓰기 권한이 필요하다. 완성된 임시 JSON 파일을 교체하여 저장 실패 시 기존 파일을 유지한다. 동시 저장은 `documents.json.tmp` 임시 디렉터리로 차단한다. 프로세스가 강제 종료되어 해당 디렉터리가 남은 경우, 실행 중인 저장 작업이 없는지 확인하고 제거한 뒤 재시도한다. 문자열 이스케이프를 처리하는 전용 직렬화기를 사용하므로 외부 JSON 라이브러리가 필요하지 않으며, 파일 경로는 UTF-8을 전제로 한다. 역색인 저장·로드는 미구현이다.

## 빌드 옵션

| 옵션 | 기본값 | 설명 |
| --- | --- | --- |
| `CMAKE_BUILD_TYPE` | `Debug` | 빌드 타입 |
| `BUILD_TESTING` | `ON` | 테스트 빌드 여부 |
