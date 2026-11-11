# Mini Lexical Search — PRD

## 1. 프로젝트 개요

### 프로젝트명

**Mini Lexical Search**
로컬 문서에 대한 전문 검색(Full-text Search) 엔진

### 목적

다수의 텍스트 문서를 검색할 때 매 검색마다 모든 파일을 순차 탐색하는 대신, 문서를 사전에 인덱싱하여 Line-aware Inverted Index로 저장하고 관련 문서를 검색한다.

색인에는 단어의 등장 횟수(`termFrequency`)와, 그 단어가 나온 **줄 번호**를 함께 저장한다. 검색 결과는 매칭된 문서와 함께 grep처럼 해당 줄에서 검색어가 어떻게 나오는지를 보여 준다.

---

# 2. 문제 정의

가장 단순한 문서 검색은 사용자가 검색할 때마다 모든 파일의 내용을 확인하는 방식으로 구현할 수 있다.

```text
Query
  ↓
doc1 전체 검색
doc2 전체 검색
doc3 전체 검색
...
docN 전체 검색
```

문서 수와 데이터 크기가 증가하면 매 검색마다 전체 데이터를 읽어야 하므로 검색 비용이 증가한다.

본 프로젝트에서는 문서를 최초 등록 또는 갱신할 때 한 번 분석하여 검색용 Index를 생성한다.

```text
Documents
    ↓
줄 단위로 읽기
    ↓
Tokenization / Normalization
    ↓
Indexing - 등장 횟수와 출처 줄 번호 기록
    ↓
Line-aware Inverted Index
```

검색 시에는 어떤 문서가 매칭되는지를 Index로 찾는다. 줄 원문은 표시할 파일에서 필요한 범위만 읽으며, 펼치기는 직전 위치에서 이어 읽는다.

```text
Query: "자료구조가"

       ↓

Indexing과 동일한 Tokenizer / Normalizer

       ↓

Index Lookup ("자료구조")

       ↓

자료구조 → doc7: tf=6, lines=[1, 2]

       ↓

디렉터리 우선·이름순 파일 정렬

       ↓

파일별 결과·페이지 구성

       ↓

표시할 파일을 순차 Scan하여 매칭 줄 추출
  doc7
    1: ...자료구조...
    2: ...자료구조...
```

---

# 3. 목표

## 3.1 핵심 목표

1. 다수의 로컬 문서를 사전에 인덱싱할 수 있다.
2. 특정 키워드가 포함된 문서를 Line-aware Inverted Index로 검색할 수 있다.
3. 검색 결과에 그 단어가 나온 줄 번호와 줄 내용을 포함한다.
4. 결과를 디렉터리 우선·이름순 파일 그룹과 줄 번호순으로 표시한다.
5. TUI에서 파일별 펼치기·페이지 탐색을, no-TUI에서 전체 출력을 제공한다.
6. 여러 키워드에 대한 AND / OR 검색을 지원한다.

## 3.2 확장 목표(낮은 우선순위)

* Trie 기반 검색어 자동완성
* TF-IDF / BM25 기반 Ranking
* Semantic Search
* Lexical + Semantic Hybrid Search

---

# 4. 대상 사용자

본 프로그램은 대규모 웹 검색을 목적으로 하지 않는다.

주 사용 사례는 사용자의 컴퓨터에 존재하는 비교적 제한된 규모의 문서를 빠르게 검색하는 것이다.

예:

```text
documents/
├── data_structure.md
├── operating_system.txt
├── android_notes.md
├── algorithm.md
└── ...
```

사용자가 다음과 같이 검색한다.

```text
> 자료구조
```

출력:

```text
=============================
[algorithm.md] 1
3: 정렬 알고리즘과 자료구조를 함께 다룬다.
[cpp_notes.md] 1
18: STL은 자료구조와 알고리즘을 제공한다.
[data_structure.md] 2
12: 자료구조는 데이터를 효율적으로 저장하는 방법이다.
40: 배열과 자료구조를 비교하면 다음과 같다.
=============================
```

줄 번호는 원본 파일 기준 1부터 시작한다.

---

# 5. 지원 데이터

## P0

지원 파일:

* `.txt`
* `.md`

문자 인코딩:

* UTF-8

초기 구현에서는 파일 형식을 제한하여 Indexing과 검색 구현에 집중한다.

## 이후 확장

* PDF - P1
* HTML - P2
* DOCX - P1

등은 별도의 Text Extractor를 통해 동일한 Indexing Pipeline에 입력할 수 있다.

---

# 6. 전체 시스템 구조

```text
        Local Documents              Search Query
               │                          │
               ▼                          │
        Document Loader                   │
               │                          │
               └────────────┬─────────────┘
                            ▼
                 Tokenizer / Normalizer
                            │
             ┌──────────────┴──────────────┐
             │ 문서 토큰 → Index에 기록     │
             │ 질의 토큰 → Index를 조회     │
             └──────────────┬──────────────┘
                            ▼
             ┌────────────────────────────┐
             │ Line-aware Inverted Index  │
             │  term → (doc, tf, lines)   │
             └─────────────┬──────────────┘
                           │
                           ▼
                    Posting Lists
                           │
                           ▼
                    Query Processing
                     Keyword / AND / OR
                           │
                           ▼
                    Matching Document IDs
                           │
                           ▼
                    Directory/File Order
                           │
                           ▼
                    File Groups / Pages
                           │
                           ▼
                   Matching Lines
```

---

# 7. Indexing

프로젝트 루트에서 다음 명령으로 앱을 실행한다.

```bash
./build/mini_lexical_search start --no-tui
```

`start`의 기본 경로는 TUI이다. 인덱싱·검색 CLI는 `--no-tui`로 실행하고, 실행 중 표준 입력으로 `/index [디렉터리]`를 전달한다. 디렉터리를 생략하면 `./documents`를 사용한다. TUI의 `/index`는 홈 검색창 위치를 유지한 채 별도 스레드에서 인덱싱하고, 검색창 내부 진행률과 단계별 로그를 표시한다. 현재는 하위 디렉터리의 `.txt`·`.md` 일반 파일을 수집하여 Document 메타데이터를 저장하고, 추가·변경된 문서만 줄 단위로 토큰화하여 `tokenCount`를 기록한 뒤 Line-aware Inverted Index에 반영한다. 심볼릭 링크는 제외하고 첫 저장 시 절대 경로순으로 ID를 0부터 부여하며, 이후에는 기존 ID를 유지하고 새 문서는 이전 목록의 최대 ID 다음부터 부여한다. `path`는 절대 경로, `title`은 확장자를 포함한 파일명이다. `<검색어>`로 검색하고 `/exit` 또는 `/quit`로 종료한다. `-h`, `--help`, `-v`, `--version`, `--no-tui`는 실행 시 옵션으로 유지한다.

인덱스 저장 형식은 **바이너리**로 한다. 문서 테이블과 역색인(토큰별 문서 ID, 등장 횟수, 줄 번호)을 magic `MLSI`와 version 4, 절대 인덱싱 루트 및 헤더 뒤 unsigned Base-128 Varint 정수, 길이 기반 UTF-8 문자열, 32바이트 SHA-256 digest로 저장하며, 문서 원문은 원본 파일에서 읽는다. 데이터 저장 위치는 현재 작업 디렉터리 기준 상대 경로 `.data/`로 한다. 앱은 시작할 때 `.data/index.bin` 스냅샷을 로드해 문서 테이블과 역색인을 메모리에 유지하고, `/index`마다 스냅샷의 문서 테이블과 절대 경로·원문 SHA-256 `hash`를 비교한다. `n not updated, n updated, n added, n deleted`를 출력하며 동일 해시의 기존 문서는 유지한다. 문서와 루트에 변화가 없으면 스냅샷을 다시 쓰지 않고, 변화가 있으면 문서 테이블과 역색인 전체를 하나의 스냅샷으로 저장한다. 이번 입력 목록에서 빠진 이전 문서는 삭제하므로 디렉터리를 바꾸면 이전 디렉터리의 문서도 삭제된다. 해시가 없는 기존 문서는 `updated`로 처리한다. 변경 없는 문서는 토큰화를 건너뛰고 기존 `tokenCount`와 Posting을 유지한다. 토큰화 트리거는 문서 인덱싱과 동일하게 `hash`(또는 제목) 변경이며, 변경된 문서는 기존 Posting을 버리고 처음부터 다시 만든다. 스냅샷이 없으면 전체 문서를 한 번 토큰화해 새로 만든다. 손상된 스냅샷은 시작 시 다시 만들지 확인한 뒤에만 `./documents`로 교체한다. 확인 없이 호출한 `/index`는 그 실행에서 갱신을 거부하고 파일을 유지한다. 저장 경로의 부모 디렉터리가 없으면 생성을 시도하며 쓰기 권한이 필요하다. 로드·인덱스 갱신·검색은 `.data/log.log`에 이벤트당 한 줄로 추가한다. 인덱스 단계와 검색의 화면 시간은 마이크로초를 측정해 소수점 셋째 자리 밀리초로 표시한다.

```text
magic            4 bytes  "MLSI"
version          u32      4 (little-endian)
rootLen          varuint32
root             rootLen bytes (정규화된 절대 인덱싱 경로, UTF-8)
documentCount    varuint32
documents:
  id             varuint32
  pathLen        varuint32
  path           pathLen bytes (UTF-8)
  titleLen       varuint32
  title          titleLen bytes (UTF-8)
  tokenCount     varuint32
  hashLen        u8       0 또는 32
  hash           hashLen bytes (raw SHA-256 digest)
termCount        varuint32
terms (byte 순 오름차순):
  termLen        varuint32
  term           termLen bytes (UTF-8)
  postingCount   varuint32
  postings:
    documentIdGap varuint32 (첫 ID는 절댓값)
    termFrequency varuint32
    lineCount    varuint32
    lineGaps     varuint32 * lineCount (첫 줄 번호는 절댓값)
```

version은 고정 4바이트 little-endian이며, 이후 u32 필드는 unsigned Base-128 Varint(1~5바이트)이다. `hashLen`은 기존 u8을 유지한다. 읽기와 저장 모두 version 4의 Varint + Delta 형식만 지원한다. v1~v3 스냅샷은 거부하며, 기존 파일을 백업·제거한 뒤 `/index`로 다시 구축하거나 시작 시 재생성을 선택한다. Posting 문서 ID는 term별, 줄 번호는 Posting별로 첫 절댓값 이후의 차이를 Varint로 저장한다. 첫 문서 ID 0 외에 0 차이를 허용하지 않으며 복원 시 덧셈 오버플로를 검사한다. 문서와 루트에 변경이 없는 인덱싱은 기존 파일을 재저장하지 않는다. 루트만 바뀌거나 빈 디렉터리를 인덱싱한 경우도 루트를 저장한다. 루트·문서·역색인은 함께 원자적으로 저장·복구하며 절대 루트와 문서 경로 포함 관계를 검증한다. Varint의 잘림·오버플로·최소 길이가 아닌 표현은 거부한다. 로더는 magic/version, 중복 문서 ID·경로·term, 존재하지 않는 문서를 참조하는 Posting, Posting 정렬·TF·줄 번호, 잘린 데이터와 trailing byte를 검증한다. 기존 `.data/index.json`은 읽지 않으며, 필요하면 `/index`로 새 스냅샷을 만든다.

문서 메타데이터만 다루는 `saveDocuments`/`loadDocuments`와 `.data/documents.json` 배열 형식은 라이브러리 API와 테스트에서 그대로 유지한다.

## 7.1 Document ID

모든 문서에는 내부적으로 고유한 정수 ID를 부여한다.

```cpp
using DocumentId = uint32_t;
```

예:

```text
0 → data_structure.md
1 → android.md
2 → operating_system.md
```

별도의 Document Table에서 ID와 문서 Metadata를 관리한다.

```cpp
struct Document {
    DocumentId id;
    std::string path;
    std::string title;
    uint32_t tokenCount;
    std::string hash; // 원문 바이트의 SHA-256 (소문자 16진수 64자리)
};
```

`tokenCount`는 정규화 이후 토큰 수이며, 문서 길이로 사용한다. `collectDocuments`는 파일 수집 단계이므로 `0`으로 두고, 변경된 문서를 토큰화하는 단계에서 실제 토큰 수로 채운다. 변경 없는 문서는 기존 값을 유지한다.

---

## 7.2 Tokenization

문서의 원문을 검색 가능한 Token으로 분할한다. Tokenizer는 한 줄의 텍스트만 받고, 줄 번호는 반환하지 않는다. 파일을 읽는 쪽에서 줄 번호를 붙인다. 줄 번호는 1부터 시작하고, 줄의 경계는 개행이다.

```cpp
class Tokenizer {
public:
    virtual std::vector<std::string>
    tokenize(const std::string& text) = 0;
};
```

```cpp
for (uint32_t line = 1; std::getline(file, text); ++line) {
    auto tokens = tokenizer.tokenize(text);

    for (const auto& token : tokens) {
        index.add(token, documentId, line);
    }
}
```

예:

```text
1| 자료구조를 공부하고
2| 있습니다.
```

초기 구현:

```text
줄 1  자료구조를
줄 1  공부하고
줄 2  있습니다
```

한국어 처리 기능 적용 시에도 호출 방식은 같다. 한 줄을 분석기에 넣고, 나온 토큰 전부에 그 줄 번호를 붙인다.

```text
줄 1  자료구조
줄 1  공부
줄 2  있다
```

P0에서는 `SimpleTokenizer`를 사용하며 규칙은 다음과 같다.

* ASCII 공백으로 분리한다.
* ASCII 영문은 소문자로 변환한다.
* ASCII 문장부호와 제어문자는 제거한다.
* `0x80` 이상 바이트(UTF-8 한국어 등)는 그대로 유지한다.

파일을 읽는 쪽(`tokenizeDocuments`)이 줄마다 1부터 번호를 붙여 `Tokenizer::tokenize`를 호출하고, 돌려받은 토큰 전부를 그 줄 번호와 함께 방출한다. 문서별 정규화 토큰 수는 `Document.tokenCount`에 기록한다. 토큰화는 추가·변경된 문서에 대해서만 수행하고 변경 없는 문서는 건너뛴다. 방출된 토큰은 `InvertedIndex::add`로 전달되어 역색인에 기록된다.

`index.add`는 같은 문서·같은 토큰에 대해 다음을 갱신한다.

* 토큰이 나올 때마다 `termFrequency`를 1 증가시킨다.
* `lines`에는 그 줄 번호가 아직 없을 때만 추가한다.

파일을 위에서 아래로 읽으므로, 같은 줄이 연속해서 들어오면 `lines.back()`과 비교해 중복을 거른다. 한 줄에 같은 단어가 여러 번 나와도 줄 번호는 하나이고, 등장 횟수는 `termFrequency`에 남는다.

```text
1| 자료구조 자료구조 자료구조 자료구조 자료구조
2| C++에서 자료구조를 배운다
```

현재 SimpleTokenizer에서는 둘째 줄의 `자료구조를`가 `자료구조`와 다른 토큰이다. 각 토큰은 별도로 색인된다.

```text
자료구조:   termFrequency = 5, lines = [1]
자료구조를: termFrequency = 1, lines = [2]
```

---

# 8. Line-aware Inverted Index

검색의 기본 색인이다.

일반적인 문서 저장 구조가

```text
Document → Words
```

라면 Inverted Index에서는 관계를 반대로 저장한다.

```text
term
 ↓
document
 ↓
termFrequency, line numbers
```

이 색인의 위치는 줄 번호이다. 정보검색에서 Positional Inverted Index는 문서 안 토큰 오프셋을 저장해 구문 검색과 근접 검색에 쓰는 구조를 가리킨다. 본 프로젝트는 그 구조를 쓰지 않으므로 Line-aware Inverted Index라고 부른다.

예:

```text
자료구조
→ doc1: tf=7, lines=[3, 18, 42]
→ doc7: tf=2, lines=[12, 40]
→ doc21: tf=1, lines=[1]

알고리즘
→ doc2: tf=4, lines=[1, 6]
→ doc7: tf=3, lines=[12, 30]

안드로이드
→ doc4: tf=1, lines=[2]
→ doc12: tf=5, lines=[1, 15]
```

C++ 구조:

```cpp
struct Posting {
    DocumentId documentId;
    uint32_t termFrequency;
    std::vector<uint32_t> lines;
};

using PostingList = std::vector<Posting>;

std::unordered_map<
    std::string,
    PostingList
> invertedIndex;
```

각 Token이 Index의 Key가 되고, 해당 Token이 등장한 문서의 Posting List가 값이 된다.

* `termFrequency`는 그 문서에서 토큰이 나온 횟수이다. 저장 필드는 유지하며 현재 파일순 검색은 순위 계산에 사용하지 않는다.
* `lines`는 토큰이 나온 서로 다른 원본 줄 번호이다. Snippet이 보여 줄 줄의 출처이다.
* `lines.size()`는 Term Frequency가 아니다.

저장 순서:

* Posting List는 `documentId` 오름차순
* 한 Posting의 `lines`는 중복 없는 오름차순

구현에서는 `InvertedIndex` 클래스가 `std::unordered_map<std::string, PostingList>`를 소유한다. `add`가 토큰이 나올 때마다 호출되어 정렬 순서를 유지하며, 같은 줄 번호는 한 번만 넣고 `termFrequency`는 매번 증가시킨다. 문서가 변경되면 `removeDocument`로 그 문서의 Posting을 모두 버린 뒤 새 토큰으로 다시 채우고, 삭제된 문서도 같은 방식으로 제거한다. Posting List가 비면 term 자체를 지운다. 변경 없는 문서의 Posting은 건드리지 않는다.

줄 원문과 줄의 byte offset은 Index에 넣지 않는다.

조회 이후의 흐름:

```text
Hash Table
   │
   │ term lookup
   ▼
Posting List
   │
   ├── documentId      → 어떤 문서?
   ├── termFrequency   → 얼마나 많이 등장?
   └── lines           → 어느 줄?
         │
         ▼
   Matching Document IDs
         │
         ▼
   Directory/File Order
         │
         ▼
   File Groups / Pages
         │
         ▼
표시할 원문의 필요한 범위 Scan
         │
         ▼
     Matching Lines
```

---

# 9. Posting List

Posting은 특정 단어가 어떤 문서에 몇 번, 어느 줄에 등장했는지를 표현한다.

예:

```text
"자료구조"

[
    {docId: 1, termFrequency: 7, lines: [3, 18, 42]},
    {docId: 7, termFrequency: 2, lines: [12, 40]}
]
```

한 줄에 다섯 번, 다른 줄에 한 번 나온 경우는 다음과 같다.

```text
Posting {
    documentId = 7,
    termFrequency = 6,
    lines = [1, 2]
}
```

P0에서 저장하는 정보:

```text
Document ID
Term Frequency
Line Numbers
```

---

# 10. Query Processing

검색 질의 역시 Indexing 시 사용한 것과 동일한 Tokenizer와 Normalizer를 적용한 후 Index를 조회한다. P0의 SimpleTokenizer에도 같은 규칙을 적용한다.

```text
문서:   "자료구조" → SimpleTokenizer → "자료구조" → Index
검색어: "자료구조" → 동일한 SimpleTokenizer → "자료구조" → Index Lookup
```

질의 형태:

```text
자료구조
자료구조 AND 알고리즘
자료구조 OR 안드로이드
```

검색 조건을 만족하는 모든 문서를 인덱싱 루트 기준 상대 경로로 정렬한다. 각 부모에서 하위 디렉터리를 먼저, 이후 파일을 방문하는 깊이 우선 순서다. 이름은 로케일에 의존하지 않는 UTF-8 바이트순이며 문서 ID와 TF 점수는 표시 순서에 사용하지 않는다. 검색 시 원본 디렉터리를 다시 순회하지 않는다.

연산자는 공백으로 구분된 대문자 `AND`/`OR`이며 각 피연산자는 하나의 정규화 토큰이어야 한다. 혼합 연산자·괄호·따옴표·암묵적 연산자는 거부한다. 반복된 검색어는 한 번만 처리한다.

## 문서 선택과 매칭 행

`QueryProcessor::matchingDocuments`는 문서 ID순 Posting을 병합하여 단일 검색어의 문서, AND의 교집합, OR의 합집합을 반환한다. AND는 문서 기준이며 검색어가 같은 행에 있을 필요는 없다.

Application은 선택된 문서를 트리 순서로 정렬한 뒤, 방문하는 파일의 검색어별 Posting 행을 커서로 병합한다. 파일 내부 매칭 행은 검색어별 행의 합집합이며 한 행의 반복 일치·여러 검색어 일치는 한 번만 센다. 원본 Posting은 수정하지 않고 전체 후보의 매칭 행 벡터와 원문을 미리 복사하지 않는다.

## 파일별 출력과 탐색

파일 제목은 `[상대 경로] 건수`이며 파일 전체의 중복 제거된 매칭 행이 1~10개면 실제 숫자, 11개 이상이면 `10+`다. 매칭 행이 없는 파일은 제외한다. 본문은 `{1-based 줄번호}: {원문}`이며 파일 내 줄 번호 오름차순이다. no-TUI는 결과 블록 앞뒤에 `=============================`를 출력한다. TUI는 파일 사이 수평선과 구조화된 결과 화면을 사용한다.

TUI는 파일 추가 전에 현재 페이지의 매칭 행 수가 50 미만인지 확인하고 다음 파일의 처음 최대 10행을 통째로 추가한다. 49행에서 10행을 추가하면 59행이다. 파일 제목·경고는 행 수에 포함하지 않는다. 더보기 버튼은 해당 파일의 다음 최대 10행을 추가하며 남은 행 수를 표시한다. 더보기·이전·다음 페이지는 버튼 클릭 또는 키보드 선택 후 Space·Enter로만 실행한다. 펼치기는 50행을 넘어도 허용하고 파일 배치와 페이지 경계는 유지한다. 이전·다음 버튼은 파일 페이지를 이동하며 첫·마지막 페이지에서는 해당 방향의 버튼을 비활성화한다. 이전 페이지는 읽은 원문과 펼친 상태를 재사용한다.

no-TUI는 파일 수·행 수 제한 없이 모든 결과를 순회하며 출력한다. more·페이지·탐색은 제공하지 않고 파일별 원문 전체를 메모리에 보관하지 않는다.

원문은 표시 범위만 읽고, 펼치기는 세션에 기록한 바이트 위치에서 이어 읽는다. 읽기 실패 시 기존 행을 보존하고 파일별 경고를 남긴 뒤 다른 파일 처리를 계속한다. 실패한 파일은 해당 세션에서 더 읽지 않으며 새 검색에서 재시도한다. 스냅샷은 검색으로 변경하지 않는다. 원문 변경은 `/index`로 갱신해야 한다.

새 검색 시도·인덱싱·재생성 시작과 TUI의 Esc 홈 복귀 시 이전 세션을 무효화하며 실패 후에도 이전 Posting 참조·페이지를 재사용하지 않는다. TUI 표시와 스크롤은 [TUI 디자인](TUI_DESIGN.md)을 따른다.

---

# 11. Ranking

현재 사용자 검색은 TF 순위와 Top-K를 적용하지 않는다. TF 저장 필드, `QueryProcessor::process`의 점수 후보와 독립 Top-K 모듈은 알고리즘·테스트용으로 유지한다. 실제 검색 경로는 문서 ID 선택과 트리순 결과 탐색을 사용한다.

## P1 Ranking

TF-IDF 또는 BM25 방식으로 확장한다. `termFrequency`를 각 term의 빈도 입력값으로 활용한다. 각 query term의 점수를 구한 뒤 더한다.

이를 통해 모든 문서에서 자주 등장하는 일반적인 단어의 영향력을 낮추고 특정 문서를 잘 구분하는 단어의 중요도를 높인다. 문서 길이는 `Document.tokenCount`를 사용한다.

---

# 12. Heap / Priority Queue

이 절은 유지 중인 독립 Top-K 알고리즘의 설명이다. 현재 Application·TUI·no-TUI 검색 경로에서는 호출하지 않는다.

Ranking 결과 전체를 완전히 정렬할 필요 없이 가장 높은 K개의 검색 결과만 필요하다.

예:

```text
후보 문서: 10,000개
사용자가 보는 검색 결과: 10개
```

모든 결과 정렬:

```text
O(N log N)
```

크기 K의 Min Heap을 사용하는 Top-K:

```text
O(N log K)
```

구조:

```text
Candidates
    ↓
Score Calculation
    ↓
Min Heap (size = K)
    ↓
Top-K Results
```

Heap은 검색 Index를 저장하는 구조가 아니라 **검색 과정에서 상위 결과를 선택하기 위한 임시 자료구조**로 사용한다. 구현은 `std::priority_queue`를 크기 K의 min heap으로 쓴다. Heap에는 후보의 `matchedLines` 대신 후보 배열 인덱스만 저장하고, 루트는 현재 선택된 후보 중 가장 낮은 순위다. 순위는 점수 내림차순이고 동점은 문서 ID 오름차순이다. 선택 뒤에 그 K개를 같은 순서로 정렬한다. 후보 수를 M이라 하면 선택은 O(M log K), 정렬은 O(K log K), heap의 추가 공간은 O(K)이다. `QueryProcessor`가 만든 전체 후보 배열은 그대로 있으므로 검색 전체의 메모리가 O(K)가 되지는 않는다. 기본 K는 10이다. K가 0이면 결과가 없고, 후보가 K보다 적으면 모두 반환한다. 이 알고리즘 모듈은 원문을 읽지 않으며 현재 UI 검색의 파일 선택·표시에 관여하지 않는다.

---

# 13. Trie 자동완성

P1 또는 P2에서 검색어 자동완성을 제공한다.

예:

```text
사용자 입력:

"안드"
```

결과:

```text
안드로이드
안드로이드스튜디오
안드로이드앱
```

Indexing 과정에서 발견된 Vocabulary를 Trie에 저장한다.

```text
          root
           │
           안
           │
           드
           │
           로
           │
           이
           │
           드
```

검색어 Prefix를 기준으로 관련 Token을 탐색한다.

---

# 14. 한국어 검색

한국어에서는 조사와 어미로 인해 동일한 단어가 다른 Token으로 인식될 수 있다.

예:

```text
자료구조를
자료구조가
자료구조는
자료구조의
```

단순 공백 Tokenizer에서는 서로 다른 네 개의 Token이 된다.

Tokenizer 계층은 문서와 질의에 동일한 토큰화 규칙을 적용한다. 인터페이스는 7.2와 같다. 한 줄의 텍스트를 받아 토큰 목록만 반환하고, 줄 번호는 파일을 읽는 쪽이 붙인다.

### SimpleTokenizer

P0에서 사용한다.

* ASCII 영문·숫자와 한글(완성형·자모)만 토큰으로 유지
* 그 밖의 문자(공백·특수문자·이모지·BOM·기타 언어)는 토큰을 분리하고 제외
* 영문 lowercase 변환

---

# 15. 개발 단계

## P0 — Core Search Engine

**목표: 줄 번호를 포함한 검색으로 키워드가 나온 줄을 찾는다**

필수 기능:

* TXT / Markdown 문서 읽기
* 줄 단위 Tokenization과 출처 줄 번호 기록
* Line-aware Inverted Index 생성
* Line-aware Inverted Index 바이너리 저장 / 로드
* Posting List (`documentId`, `termFrequency`, `lines`)
* 단일 Keyword Search
* 파일별 매칭 줄·건수 출력
* 디렉터리 우선·이름순 결과 탐색
* TUI 파일별 10행 펼치기·페이지 탐색 / no-TUI 전체 출력
* 문서 추가 및 전체 Re-indexing

완료 조건:

```text
문서 디렉토리 입력
       ↓
Line-aware Index 생성
       ↓
검색어 입력
       ↓
모든 일치 문서의 파일별 매칭 줄 탐색
```

---

## P1 — Advanced Lexical Search

**목표: 검색 품질 개선**

기능:

* AND / OR 검색 (Sorted Posting List Merge)
* TF-IDF 또는 BM25 Ranking
* Trie 기반 자동완성
* Stop Word 처리

---

## P2 — Semantic Search Experimental

**목표: Keyword Search의 한계 비교**

EmbeddingGemma 등의 Embedding Model을 이용하여 각 문서를 Vector로 표현한다.

```text
Document
   ↓
EmbeddingGemma
   ↓
Embedding Vector
```

사용자 Query 역시 Embedding으로 변환한다.

```text
Query
   ↓
EmbeddingGemma
   ↓
Query Vector
```

두 Vector의 Cosine Similarity를 이용하여 의미적으로 유사한 문서를 검색한다.

P2에서는 단순 Linear Search도 허용한다.

이 단계의 목적은 Semantic Search 자체를 프로젝트의 핵심으로 삼는 것이 아니라 기존 Keyword Search와 결과 특성을 비교하는 것이다.

---

## P3 — Hybrid / ANN Search

시간 및 프로젝트 범위가 허용될 경우 구현한다.

후보 기능:

* Keyword + Semantic Hybrid Search
* ANN Index
* HNSW 등 Graph 기반 Vector Search
* Keyword 결과와 Semantic 결과 결합

예:

```text
Final Score
 =
α × BM25
+
β × Semantic Similarity
```

P3는 선택적인 연구/확장 단계이며 프로젝트 핵심 평가 범위에는 포함하지 않는다.

---

# 16. 검색 방식 비교

최종 프로젝트에서는 최소 두 가지 검색 방식을 비교한다.

## Naive Search

매 검색마다 모든 문서 검색:

```text
Query
  ↓
All Documents Scan
```

## Indexed Search

```text
Query
  ↓
Indexing과 동일한 Tokenizer / Normalizer
  ↓
Index Lookup
  ↓
Posting List (문서 + termFrequency + 줄 번호)
  ↓
Directory/File Order
  ↓
표시할 파일의 매칭 줄을 필요한 범위만 Scan
```

비교 항목:

* 검색 시간
* 문서 수 증가에 따른 검색 시간 변화
* Indexing 시간
* Index 메모리 사용량

테스트 예:

```text
100 documents
1,000 documents
10,000 documents
```

---

# 17. 성공 기준

P0 기준 프로젝트 성공 조건:

1. 최소 수백~수천 개 이상의 문서를 인덱싱할 수 있다.
2. 매칭 문서를 찾을 때 모든 원본 문서를 다시 읽지 않는다.
3. Line-aware Inverted Index로 키워드가 포함된 문서, 등장 횟수, 줄 번호를 탐색한다.
4. 검색 결과에 해당 줄 번호와 줄 내용을 포함한다.
5. 모든 일치 파일을 트리 순서로 탐색하며 TUI 펼치기·페이지 이동 시 누락·중복이 없다.
6. 동일 데이터에 대해 Naive Search보다 검색 시간이 감소한다.

추가 평가:

* Indexing 시간
* Query Latency
* Memory Usage
* 검색 결과 Precision
* 문서 수 증가에 따른 성능 변화

---

# 18. 비목표 / Out of Scope

기본 프로젝트에서는 다음 기능을 목표로 하지 않는다.

* 웹 전체 Crawling
* Google 수준의 검색 품질
* 분산 검색 시스템
* 대규모 Vector Database
* LLM 기반 답변 생성
* 이미지 / 영상 검색
* 실시간 인터넷 검색

---

# 19. 예상 기술 스택

과제 제출에서는 FTXUI만 외부 라이브러리로 허용한다. 어승경(2023203068)이 UI 용도로 개인 승인을 받았으며, 검색 핵심 자료구조·알고리즘에는 사용하지 않는다. 다른 절의 Embedding 모델 등 외부 도구 후보는 현재 허용된 구현 범위에 포함되지 않는다. 상세 조건은 [FTXUI 사용 승인](FTXUI_APPROVAL.md)을 따른다.

### Core

* ISO C++20 (`-std=c++20`)
* CMake
* STL

### Optional

* FTXUI — amalgamated 7.0.3 vendoring 완료. `start` 기본 TUI는 홈의 중앙 입력 상자에서 검색 후 좌측 정렬의 전체 너비 결과 화면으로 전환한다. 검색창·실행 검색어·하단 페이지 컨트롤을 고정하고, 파일 사이 수평선·최대 3줄의 첫 일치 주변 발췌·토큰 강조를 제공한다. ↑↓로 검색창과 결과 사이를 이동하고 행·버튼을 선택하며 휠·PgUp/PgDn으로 스크롤한다. Esc는 검색 상태를 지우고 홈으로 복귀하며 종료하지 않는다. Ctrl+C·`/exit`·`/quit`과 복구 팝업의 `quit`으로 종료한다. 상세 기준은 [TUI 디자인](TUI_DESIGN.md)을 따른다.

핵심 로직과 CLI는 공통 Application 계층으로 분리했다. `--no-tui`로 CLI REPL을 실행한다. FTXUI 소스를 함께 제출하며 전체 용량 10MB 이하, 채점 환경에서 별도 설치·다운로드 없이 `make` 한 번으로 빌드하는 조건을 충족해야 한다. 테스트는 외부 테스트 라이브러리 없이 CTest 기반으로 수행한다.

---

# 20. 프로젝트 핵심 설명

본 프로젝트의 핵심 아이디어는 다음 한 문장으로 정리할 수 있다.

> 모든 문서를 검색 시점에 순차 탐색하는 대신, 토큰의 등장 횟수와 나온 줄을 Line-aware Inverted Index로 사전에 저장하여 관련 문서를 찾고, 트리 순서의 파일별 그룹에서 그 줄이 어떻게 나오는지를 함께 반환한다.

프로젝트가 확장되더라도 중심 구조는 다음과 같이 유지한다.

```text
                  Search Engine
                       │
          ┌────────────┴────────────┐
          │                         │
   Lexical Search             Semantic Search
   [Main / P0-P1]             [Optional / P2-P3]
          │                         │
   Line-aware Inverted Index   Embedding
   (문서 + TF + 줄 번호)               │
          │                         │
          └────────────┬────────────┘
                       │
                 Hybrid Ranking
```

Semantic Search를 추가하더라도 프로젝트의 핵심은 Line-aware Inverted Index 기반 Lexical Search Engine으로 유지한다.
