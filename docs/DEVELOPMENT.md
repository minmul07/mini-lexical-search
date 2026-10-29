# 개발 환경

## 요구 사항

| 항목 | 버전 | 비고 |
| --- | --- | --- |
| C++ | ISO C++20 (`-std=c++20`) | `g++ 13.3.0`에서 확인 |
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

현재 개발 빌드는 CMake를 사용한다. 제출용 `make` 단일 명령 빌드는 아직 구현되지 않았다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Release 빌드는 `-DCMAKE_BUILD_TYPE=Release`로 지정한다.

## 외부 라이브러리 사용 조건

FTXUI는 UI 용도로 사용 승인을 받았다. 검색어 입력·결과 표시·키보드 처리·화면 전환·상태 시각화에만 사용하며, 핵심 자료구조와 검색 알고리즘은 STL과 직접 작성한 코드로 구현한다. FTXUI 이외의 외부 라이브러리는 사용하지 않는다.

FTXUI 소스를 함께 제출하고 전체 제출 용량을 10MB 이하로 유지해야 한다. amalgamated 7.0.3은 `third_party/ftxui/`에 있으며 CMake 타깃 `ftxui`로 빌드한다. 채점 환경은 Ubuntu 24.04 LTS 64-bit / GNU g++ / C++20이며, 별도 설치·다운로드 없이 `make` 한 번으로 빌드해야 한다. 핵심 로직과 CLI는 공통 Application 계층으로 분리했다. `start`의 기본 경로는 FTXUI TUI이며, `--no-tui`로 기존 CLI REPL을 실행한다. TUI는 같은 명령을 Enter로 실행하고 출력을 입력 상자 아래에 표시한다. 상세 승인 기록과 검증 조건은 [FTXUI 사용 승인](FTXUI_APPROVAL.md)을 참고한다.

## 실행

```bash
./build/mini_lexical_search start
./build/mini_lexical_search start --no-tui
```

`start`는 기본적으로 TUI를 연다. 터미널 중앙에 최대 너비 100열의 흰색 테두리 입력 상자를 두며, 입력이 상자 너비를 넘으면 줄이 늘어난다. Enter는 CLI와 같은 명령을 실행하고 상자를 비운다. 출력은 상자 아래, 상자 내용 너비를 넘지 않게 줄바꿈되어 나타나고 다음 명령이 교체한다. 화면 높이를 넘는 출력은 그대로 둔다. 우측 상단에는 전역 `workingJob` 라벨을 단순 텍스트로 표시한다. 로드 성공은 `Ready`, 실패는 `Failed to load index`다. 손상된 스냅샷이면 CLI는 `Failed to load the index. Recreate the index? (Y/n)`를 묻고, `Y`·`y`·Enter는 `./documents`로 다시 만들며 `n`은 파일을 유지한 채 종료한다. TUI는 입력 상자 전에 `reset index`와 `quit` 팝업을 연다. 화면과 입력 규칙은 [TUI 디자인](TUI_DESIGN.md)을 따른다. 인덱싱 중에는 `updating index...`와 `Tokenizing..`을 기록하고, 성공하면 `Ready`, 실패하면 `Failed to update index`가 된다. 화면은 프레임이 그려질 때의 라벨을 보여 준다. `/exit`, `/quit`, Esc, Ctrl+C로 종료한다. `--no-tui`는 같은 명령의 줄 단위 REPL이다.

```text
$ ./build/mini_lexical_search start --no-tui
Mini Lexical Search started.
Type '/help' for commands, '/exit' to quit.
> /index documents/recap
updating file metadata..
updated file metadata (2.000 ms)
0 not updated, 0 updated, 3 added, 0 deleted
tokenizing documents..
tokenized 3 documents (1.000 ms)
> /search 자료구조
searched (0.123 ms)
No matches.
> /exit
```

옵션:

* `-h`, `--help` — 도움말
* `-v`, `--version` — 버전
* `--no-tui` — TUI 없이 CLI로 실행. `start`와 함께 쓴다.

### 실행 중 명령

* `/index [디렉터리]` — `.txt`·`.md` 문서 메타데이터와 Line-aware Inverted Index를 `.data/index.json` 스냅샷으로 저장 (기본 입력 경로: `./documents`)
* `/search <검색어>` — 단일 키워드 또는 대문자 AND/OR로 검색. TF 합산 점수 내림차순으로 상위 10개 문서와 매칭 행을 출력한다. 동점은 문서 ID 오름차순이고, 줄은 줄 번호순이다. 검색이 끝나면 `searched (<시간> ms)`를 소수점 셋째 자리 밀리초로 먼저 출력한다.
* `/help` — 도움말
* `/exit`, `/quit` — 종료

명령은 표준 입력으로 한 줄씩 전달한다. 빈 줄은 무시하며, 알 수 없는 명령이나 잘못된 인자는 오류를 출력한 뒤 다음 입력을 받는다. 입력 종료(EOF) 시에도 정상 종료한다. 문서 디렉터리는 실행 인자 대신 `/index`의 인자로 지정한다. 명령 뒤 나머지 문자열 전체를 경로로 해석하므로 공백이 포함된 경로도 따옴표 없이 입력한다. 경로 양 끝의 공백은 제거한다. 앱은 시작할 때 `.data/index.json` 스냅샷을 메모리에 로드한다. 파일이 없으면 빈 인덱스로 시작한다. 손상되면 CLI는 다시 만들지 묻고, 거절하거나 재생성이 실패하면 파일을 유지한다. 확인 없이 호출한 `/index`는 그 실행에서 갱신을 거부한다. `/index`는 시작 시 `updating file metadata..`, 성공 시 `updated file metadata (<시간> ms)`와 변경 집계를 출력하고, 이어서 `tokenizing documents..`와 `tokenized <문서 수> documents (<시간> ms)`를 출력한다. 메타데이터 시간은 문서 수집·해시 계산·기존 스냅샷 문서 테이블과의 비교를 포함한 경과 시간이고, 토큰화 시간은 변경된 문서를 읽어 토큰화하고 Posting을 다시 만든 경과 시간이다. 둘 다 마이크로초로 측정해 소수점 셋째 자리 밀리초로 표시한다. 변경 없는 문서는 토큰화하지 않으며 `tokenized 0 documents (0.000 ms)`로 표시된다. 실패 시 `error: indexing failed: <원인>`을 출력하고 앱은 계속 실행된다. 메타데이터 준비 단계에서 실패하면 완료 메시지 없이 오류만 출력하며, 이후 단계(토큰화·저장)에서 실패하면 그 단계 이후의 완료 메시지만 생략된다. `/search`는 질의 처리·Top-K 선정·선정 문서의 매칭 행 읽기를 포함한 경과 시간을 마이크로초로 측정해 `searched (0.123 ms)`처럼 소수점 셋째 자리 밀리초로 출력한 뒤 결과를 출력한다. 무매칭이면 `No matches.`를 이어서 출력한다. 질의 오류나 스냅샷 미로드는 완료 시간 없이 오류만 출력한다. 시작 시 로드 성공·실패, 성공한 인덱스 갱신의 단계 시간과 변경 집계, 실패한 인덱스 갱신, 성공한 검색의 질의·결과·시간은 `.data/log.log`에 이벤트당 한 줄로 추가한다. 시각은 그 줄 앞에 붙고, 기록 실패는 명령 결과를 바꾸지 않는다.

## 테스트

```bash
ctest --test-dir build --output-on-failure
```

`BUILD_TESTING=OFF`로 테스트를 끌 수 있다. 검증 범위는 [테스트 가이드](TESTING.md)를 참고한다.

## 디렉터리 구조

```text
mini-lexical-search/
├── CMakeLists.txt
├── include/mini_lexical_search/  # src와 같은 계층별 공개 헤더
├── src/
│   ├── main.cpp                # 실행 인자 처리·앱 구성
│   ├── domain/                 # 데이터 모델·핵심 자료구조·알고리즘
│   ├── application/            # 인덱스 상태·인덱싱·검색 절차
│   ├── infrastructure/         # 파일 접근·해시·JSON 저장/로드
│   └── presentation/
│       ├── cli/                # 명령 입력·출력
│       └── tui/                # FTXUI 입력·명령 출력
├── tests/                      # CTest 기반 테스트
└── docs/                       # PRD 및 개발 문서
```

빌드 산출물은 `build/`에 생성되며 git에서 제외한다.

### 계층 의존성과 공통 API

CLI → Application → Infrastructure → Domain 방향으로 연결하며 Application도 Domain을 직접 사용한다. TUI는 FTXUI로 화면을 그리고 `cli::handleLine`으로 같은 명령을 실행한다. Domain은 다른 계층에 의존하지 않는다. Application은 파일 처리 함수를 직접 호출하고, UI 타입·콘솔 출력에 의존하지 않는다. CMake도 `mini_lexical_search_domain`, `mini_lexical_search_infrastructure`, `mini_lexical_search_application`, `mini_lexical_search_cli`, `mini_lexical_search_tui` 타깃으로 나눈다. `ftxui`는 `mini_lexical_search_tui`에만 링크하고, TUI는 CLI 타깃을 링크한다. `mini_lexical_search_core`는 공통 계층 테스트를 위한 INTERFACE 타깃이다.

`application/search_application.hpp`의 `SearchApplication`이 스냅샷 경로, 메모리 인덱스, Tokenizer와 초기 로드 상태를 소유한다. 생성 시 로드하며 파일이 없으면 빈 인덱스, 손상됐으면 `ready() == false`와 `loadError()`로 원인을 제공한다. 이 실행에서는 인덱싱과 검색을 거부한다.

* `index(directory, progressCallback)`는 동기적으로 인덱싱하고 `IndexUpdate`를 반환한다. 콜백은 선택 사항이며 메타데이터 시작·완료, 토큰화 시작·완료 이벤트를 순서대로 받는다. 완료 이벤트에는 해당 단계의 집계와 마이크로초 시간이 있다. CLI는 이를 소수점 셋째 자리 밀리초로 출력한다. 토큰화 완료는 저장 완료를 의미하지 않는다. 단계 실패 시 해당 완료 이벤트는 없으며 예외를 전달한다. 저장 또는 Posting 갱신 후 콜백 실패 시 마지막 저장 상태로 복구한다. 콜백에서 앱을 재호출하거나 다른 스레드에서 동시에 접근하지 않는다.
* `search(query, maxMatchingLines = 0, topK = 10)`는 TF 점수 내림차순 `SearchResult` 배열을 반환한다. 동점은 문서 ID 오름차순이다. `topK`가 0이면 빈 결과이고, 후보보다 크면 모든 후보를 반환한다. 매칭 줄 원문은 선정된 문서만 읽는다. 문서 정보·TF 점수·전체 매칭 행 수·추출한 원문 행·문서별 읽기 경고를 값으로 소유한다. `maxMatchingLines`의 `0`은 모든 행이며 양수는 문서당 한도다. 질의 오류는 예외로, 파일 읽기 오류는 문서별 경고로 전달하며 차순위 문서로 대체하지 않는다. 검색은 스냅샷을 저장하지 않는다.

공개 헤더 경로는 계층별로 변경되었다. 기존 함수 이름과 네임스페이스는 유지하지만 옛 평면 헤더 경로의 호환 래퍼는 제공하지 않는다. Document·IndexSnapshot은 Domain, 인덱싱 실행 API는 Application, 문서·스냅샷 저장 API는 Infrastructure 헤더를 사용한다. `main.cpp`는 인자를 파싱해 `SearchApplication`을 구성한 뒤 기본 `start`에서는 TUI를, `--no-tui`에서는 CLI REPL을 실행한다. 두 경로는 `cli::handleLine`으로 명령을 처리한다.

## 현재 상태

실행 환경, 문서 메타데이터·토큰화, Line-aware Inverted Index와 JSON 스냅샷 저장·로드를 구현한 상태이다.

* 완료: CMake 빌드, `mini_lexical_search` 실행 파일, 버전 헤더, Document 수집·SHA-256 해시·JSON 저장, SimpleTokenizer 기반 토큰화와 `tokenCount`, 변경 문서 토큰화 게이트, `unordered_map` 기반 Inverted Index, `.data/index.json` 스냅샷 저장·로드, CTest 테스트
* 완료: 단일 키워드·AND/OR Query Processing, TF 합산 점수
* 완료: TF 점수 내림차순 Ranking, 동점 시 문서 ID 오름차순, 크기 K의 Min Heap Top-K(기본 10)
* 완료: 계층 분리, UI 독립적인 `SearchApplication` 인덱싱·검색 API와 진행 이벤트
* 완료: `start` 기본 TUI(중앙 입력 상자, Enter로 CLI와 같은 명령 실행, 상자 아래 출력, 우측 상단 `workingJob` 텍스트)와 `--no-tui` CLI 경로
* 미구현: 화면을 넘는 TUI 출력 처리, 검색 결과 전용 화면

검색은 시작 시 로드한 스냅샷 또는 `/index`로 갱신한 메모리 인덱스를 조회하며 스냅샷 로드 실패 시 거부한다. `src/domain/query_processor.cpp`는 동일한 Tokenizer를 사용하는 질의 파싱과 정렬된 Posting List의 교집합·합집합을 담당한다. 반복 term은 한 번만 계산하고, 후보 전체는 문서 ID순으로 반환한다. 혼합 AND/OR, 괄호, 따옴표, 암묵적 연산자는 지원하지 않는다. `src/domain/top_k.cpp`의 `selectTopK`는 그 후보에서 상위 K개만 순위순으로 고른다. Heap은 `std::priority_queue`이며 후보 인덱스만 저장해 `matchedLines`를 선택 중에 복사하지 않는다. 루트는 현재 선택된 후보 중 가장 낮은 순위다. 후보 수 M에 대해 선택은 O(M log K), 결과 정렬은 O(K log K), heap의 추가 공간은 O(K)이다. `QueryProcessor`의 전체 후보 배열은 별도로 남으므로 검색 전체 메모리가 O(K)가 되지는 않는다. K가 0이면 빈 결과이고, 후보가 K보다 적으면 모두 반환한다. `src/infrastructure/matching_lines.cpp`는 선정된 문서만 한 번씩 순차적으로 읽으며 기본 한도 `0`은 전체 매칭 행, 양수 한도는 문서당 앞 N개 행이다. 현재 CLI는 한도 `0`과 상위 10개를 사용한다. 파일 읽기 실패나 기록된 줄 부재는 문서별 경고로 처리하고 차순위 문서로 대체하지 않는다. 인덱싱 후 원문 변경은 자동 검증하지 않으므로 `/index`로 갱신해야 한다.

`src/infrastructure/document_io.cpp`는 재귀 문서 수집, Document 배열의 JSON 저장, 원문 줄 읽기를 담당한다. `src/application/documents.cpp`는 변경 문서 판별, 선택 문서 토큰화, 메타데이터 전용 인덱싱을 담당한다. 하위 디렉터리를 포함해 소문자 `.txt`·`.md` 일반 파일만 수집하며 심볼릭 링크는 제외한다. 첫 저장 시 절대 경로순으로 정렬하고 ID를 0부터 부여한다. 이후에는 기존 문서 ID를 유지하고 새 문서는 이전 목록의 최대 ID 다음부터 부여한다. `title`은 확장자를 포함한 파일명이다. 문서 원문을 바이너리 스트림으로 읽어 자체 구현한 SHA-256으로 `hash`를 계산한다. `prepareIndex`가 이전 문서 테이블과 비교해 `notUpdated`/`updated`/`added`/`deleted`를 분류하고, 추가·변경된 문서의 인덱스만 `toTokenize`에, 변경·삭제된 문서 ID를 `removed`에 담는다. `tokenizeDocuments`는 `toTokenize`의 문서만 `readDocumentLines`로 줄 단위(1부터 시작)로 읽고 `Tokenizer::tokenize` 결과를 줄 번호와 함께 방출하며, 문서별 토큰 수를 `tokenCount`에 기록한다. 같은 해시·제목인 문서는 재토큰화하지 않고 기존 `tokenCount`를 유지하며, 변경이 감지된 문서는 이전 토큰 결과를 버리고 처음부터 다시 만든다.

`src/infrastructure/sha256.cpp`는 FIPS 180-4 SHA-256을 외부 라이브러리 없이 구현한다. `Sha256` 클래스가 `update`로 임의 길이의 바이트를 이어 받아 64바이트 블록 단위로 압축하고, `finish`에서 `0x80` 패딩과 64비트 길이를 붙여 32바이트 다이제스트를 만든다. 입력을 호출 경계와 무관하게 이어 붙이므로 대용량 파일을 청크로 나눠 읽어도 같은 결과를 내며, `toHex`가 소문자 16진수 64자를 반환한다. `sha256`은 한 번에 해시하는 편의 함수이다.

`src/domain/tokenizer.cpp`는 `SimpleTokenizer`를 구현한다. UTF-8을 코드 포인트 단위로 해석하여 ASCII 영문·숫자와 한글(완성형 음절·자모·호환 자모·확장 자모)만 토큰 문자로 유지하고, ASCII 영문은 소문자로 바꾼다. 그 밖의 모든 코드 포인트(공백·문장부호·이모지·BOM·기타 언어)는 토큰 구분자로 삼아 앞뒤 토큰을 분리하고 결과에서 제외한다. 연속된 구분자는 빈 토큰을 만들지 않으며, 허용 문자끼리는 언어가 달라도 합쳐서 하나의 토큰으로 둔다. 오버롱 인코딩·서로게이트·`U+10FFFF` 초과·잘린 시퀀스 등 잘못된 UTF-8 바이트도 구분자로 처리한다.

`src/domain/inverted_index.cpp`는 `InvertedIndex`를 구현한다. `std::unordered_map<std::string, PostingList>`를 소유하며, `add`로 Posting List를 `documentId` 오름차순으로 유지하고 `lines`는 중복 없이 오름차순으로 넣는다. `removeDocument`는 문서의 Posting을 모두 제거하고 빈 term을 지우며, `merge`는 새 Posting을 기존 목록과 정렬을 유지한 채 합친다. 두 목록에 같은 문서가 있으면 변경 없이 오류를 낸다.

`src/application/indexer.cpp`는 인덱싱 파이프라인을 담당한다. `applyIndexWork`는 변경 문서를 임시 인덱스에 토큰화한 뒤 성공하면 기존 Posting을 제거하고 병합하므로, 파일 읽기 실패 시 메모리 인덱스를 바꾸지 않는다. `commitIndexSnapshot`은 새 문서 테이블과 인덱스를 저장하고 성공한 뒤 메모리 스냅샷을 교체하며, 저장 실패 시 마지막으로 저장된 상태를 다시 로드해 메모리와 파일을 일치시킨다. `updateIndexSnapshot`은 전체 파이프라인을 묶은 함수로 테스트에서 사용한다.

`src/infrastructure/index_json.cpp`는 `.data/index.json` 스냅샷(`version`, `documents`, `invertedIndex`)의 직렬화·파싱을 담당한다. term은 정렬해 저장하고, 문서 ID 참조·Posting 정렬·줄 번호 등 형식 불변식을 검증한다. 손상된 데이터는 거부한다.

`src/infrastructure/json.cpp`와 `src/infrastructure/json_internal.hpp`는 문서 테이블과 스냅샷이 공유하는 JSON 파서·직렬화기와, 임시 디렉터리를 거쳐 교체하는 원자적 파일 저장을 제공한다.

`src/infrastructure/document_json.cpp`는 문서 테이블 전용 배열 형식(`saveDocuments`/`loadDocuments`)의 로드를 담당한다. 앱 스냅샷과 달리 앱 시작 시 로드하지 않으며, 메타데이터 전용 API와 테스트에서 사용한다. 해시가 없는 기존 스키마를 허용하며, JSON이 손상됐거나 경로·ID가 중복되면 오류를 출력한다.

출력은 현재 작업 디렉터리 기준 `.data/index.json`이다. 같은 디렉터리의 `.data/log.log`에는 로드, 인덱스 갱신, 검색을 이벤트당 한 줄로 추가한다. 앱은 시작 시 이 스냅샷을 메모리에 로드하고, 로드에 실패하면 `/index`가 갱신을 거부한다. 같은 경로·해시·제목인 문서는 `not updated`(토큰화·Posting 갱신 생략), 해시나 제목이 달라진 문서는 `updated`, 새 경로는 `added`, 이번 입력 목록에 없는 이전 경로는 `deleted`로 집계한다. 해시가 없는 기존 문서는 `updated`로 처리한다. 토큰화 트리거는 이 변경 감지와 동일하며, 변경된 문서는 이전 Posting을 버리고 다시 토큰화한다. 동일 내용의 파일도 경로가 다르면 별도 문서이며, 파일 이동은 삭제와 추가로 처리한다. 하나의 테이블을 선택한 디렉터리 목록과 동기화하므로 입력 디렉터리를 바꾸면 이전 디렉터리의 문서도 삭제 대상이다. 모든 문서가 동일하면 스냅샷 내용과 수정 시간을 유지하고, 변경이 있으면 문서 테이블과 역색인 전체를 저장한다. `.data/`가 없으면 생성을 시도하므로 현재 작업 디렉터리 또는 `.data/`의 쓰기 권한이 필요하다. 완성된 임시 파일을 교체하여 저장 실패 시 기존 파일을 유지하며, 실패 시 메모리 스냅샷도 마지막 저장 상태로 되돌린다. 동시 저장은 `index.json.tmp` 임시 디렉터리로 차단한다. 프로세스가 강제 종료되어 해당 디렉터리가 남은 경우, 실행 중인 저장 작업이 없는지 확인하고 제거한 뒤 재시도한다. 문자열 이스케이프를 처리하는 전용 직렬화기를 사용하므로 외부 JSON 라이브러리가 필요하지 않으며, 파일 경로는 UTF-8을 전제로 한다. 스냅샷이 없으면 전체 문서를 한 번 토큰화해 새로 만든다.

## 빌드 옵션

| 옵션 | 기본값 | 설명 |
| --- | --- | --- |
| `CMAKE_BUILD_TYPE` | `Debug` | 빌드 타입 |
| `BUILD_TESTING` | `ON` | 테스트 빌드 여부 |
