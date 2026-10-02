# Mini Lexical Search — PRD

## 1. 프로젝트 개요

### 프로젝트명

**Mini Lexical Search**
로컬 문서에 대한 전문 검색(Full-text Search) 엔진

### 목적

다수의 텍스트 문서를 검색할 때 매 검색마다 모든 파일을 순차 탐색하는 대신, 문서를 사전에 인덱싱하여 Line-aware Inverted Index로 저장하고 관련 문서를 검색한다.

색인에는 단어의 등장 횟수(`termFrequency`)와, 그 단어가 나온 **줄 번호**를 함께 저장한다. 점수는 등장 횟수로 계산하고, 검색 결과는 매칭된 문서와 함께 grep처럼 해당 줄에서 검색어가 어떻게 나오는지를 보여 준다.

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

검색 시에는 어떤 문서가 매칭되는지를 Index로 찾는다. 줄 원문은 Top-K로 선정된 파일만 처음부터 한 번 순차적으로 읽으며 고른다.

```text
Query: "자료구조가"

       ↓

Indexing과 동일한 Tokenizer / Normalizer

       ↓

Index Lookup ("자료구조")

       ↓

자료구조 → doc7: tf=6, lines=[1, 2]

       ↓

Ranking (score = termFrequency)

       ↓

Heap Top-K

       ↓

Top-K 파일을 순차 Scan하여 매칭 줄 추출
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
4. 검색 결과에 관련도 점수를 부여할 수 있다.
5. Heap을 이용해 관련도가 높은 상위 K개의 결과를 반환할 수 있다.
6. 여러 키워드에 대한 AND / OR 검색을 지원한다.

## 3.2 확장 목표(낮은 우선순위)

* Trie 기반 검색어 자동완성
* 한국어 형태소 분석
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
> /search 자료구조
```

출력:

```text
1. data_structure.md       score: 8.72
   12: 자료구조는 데이터를 효율적으로 저장하는 방법이다.
   40: 배열과 자료구조를 비교하면 다음과 같다.

2. algorithm.md            score: 5.31
   3: 정렬 알고리즘과 자료구조를 함께 다룬다.

3. cpp_notes.md            score: 3.98
   18: STL은 자료구조와 알고리즘을 제공한다.
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
                    SearchCandidate
                           │
                           ▼
                       Ranking
                           │
                           ▼
                      Top-K Heap
                           │
                           ▼
                   Matching Lines
```

---

# 7. Indexing

프로젝트 루트에서 다음 명령으로 앱을 실행한다.

```bash
./build/mini_lexical_search start
```

실행 중 표준 입력으로 `/index [디렉터리]`를 전달한다. 디렉터리를 생략하면 `./documents`를 사용한다. 현재는 하위 디렉터리의 `.txt`·`.md` 일반 파일을 수집하여 Document 메타데이터를 JSON으로 저장하며, 토큰화와 역색인 생성은 추후 구현한다. 심볼릭 링크는 제외하고 절대 경로순으로 ID를 0부터 부여한다. `path`는 절대 경로, `title`은 확장자를 포함한 파일명이다. `/search <검색어>`로 검색하고 `/exit` 또는 `/quit`로 종료한다. `-h`, `--help`, `-v`, `--version`은 실행 시 옵션으로 유지한다.

인덱스 저장 형식은 **JSON**으로 한다. 문서 테이블과 역색인(토큰별 문서 ID, 등장 횟수, 줄 번호)을 저장하며, 문서 원문은 원본 파일에서 읽는다. 데이터 저장 위치는 현재 작업 디렉터리 기준 상대 경로 `.data/`로 한다. 현재 Document 테이블은 `.data/documents.json`에 아래 JSON 배열로 저장하며, 재실행 시 전체 교체한다. 디렉터리가 없으면 생성을 시도하며 쓰기 권한이 필요하다. 역색인 저장 및 JSON 로드는 추후 구현한다.

```json
[
  {"id": 0, "path": "/absolute/path/documents/example.md", "title": "example.md", "tokenCount": 0}
]
```

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
};
```

`tokenCount`는 정규화 이후 토큰 수이며, 문서 길이로 사용한다. 현재 Document 메타데이터 저장 단계에서는 계산하지 않고 항상 `0`으로 유지한다 (no-op).

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

`index.add`는 같은 문서·같은 토큰에 대해 다음을 갱신한다.

* 토큰이 나올 때마다 `termFrequency`를 1 증가시킨다.
* `lines`에는 그 줄 번호가 아직 없을 때만 추가한다.

파일을 위에서 아래로 읽으므로, 같은 줄이 연속해서 들어오면 `lines.back()`과 비교해 중복을 거른다. 한 줄에 같은 단어가 여러 번 나와도 줄 번호는 하나이고, 등장 횟수는 `termFrequency`에 남는다.

```text
1| 자료구조 자료구조 자료구조 자료구조 자료구조
2| C++에서 자료구조를 배운다
```

단순 공백 분리에서는 둘째 줄의 `자료구조를`가 `자료구조`와 다른 토큰이다. 형태소 분석 이후 둘 다 `자료구조`로 색인되면 다음과 같다.

```text
termFrequency = 6
lines = [1, 2]
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

* `termFrequency`는 그 문서에서 토큰이 나온 횟수이다. 검색 시 점수 계산이 이 값을 읽는다.
* `lines`는 토큰이 나온 서로 다른 원본 줄 번호이다. Snippet이 보여 줄 줄의 출처이다.
* `lines.size()`는 Term Frequency가 아니다.

저장 순서:

* Posting List는 `documentId` 오름차순
* 한 Posting의 `lines`는 중복 없는 오름차순

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
   SearchCandidate
         │
         ▼
      Ranking
         │
         ▼
      Heap Top-K
         │
         ▼
Top-K 원본 파일을 한 번 순차 Scan
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
문서:   "자료구조를" → Kiwi → "자료구조" → Index
검색어: "자료구조가" → 동일한 Kiwi / Normalizer → "자료구조" → Index Lookup
```

질의 형태:

```text
/search 자료구조
/search 자료구조 AND 알고리즘
/search 자료구조 OR 안드로이드
```

## 단일 키워드

```text
Query:
자료구조
```

Index에서 해당 Token의 Posting List를 가져온다. 매칭 문서를 찾기 위해 검색 대상 문서 전체를 읽지 않는다.

```cpp
index.find("자료구조");
```

조회한 Posting은 바꾸지 않는다. 검색 결과는 별도 타입으로 만든다.

```cpp
struct SearchCandidate {
    DocumentId documentId;
    double score;
    std::vector<uint32_t> matchedLines;
};
```

단일 키워드에서는 Posting마다 다음 후보를 만든다.

```text
score = termFrequency
matchedLines = lines
```

---

## 매칭 줄 출력

Index에는 매칭된 줄을 모두 저장한다. 화면에는 문서당 앞에서부터 3줄만 보여 준다.

```cpp
constexpr size_t MAX_SNIPPETS_PER_DOCUMENT = 3;
```

```text
1. textbook.md    score: 8.72
   12: 자료구조는 데이터를 효율적으로...
   40: 배열과 자료구조를 비교하면...
   71: 적절한 자료구조를 선택하면...

   + 14 more matches
```

줄 순서는 `matchedLines`의 오름차순이다. 3줄을 넘는 개수는 `+ N more matches`로 표시한다.

줄 번호만 저장하고 byte offset은 저장하지 않는다. 일반 텍스트 파일은 1203번째 줄로 바로 이동할 수 없으므로, Top-K로 선정된 문서만 다시 열어 처음부터 한 번 순차적으로 읽으며 Index에 기록된 줄을 추출한다.

```text
전체 문서 10,000개
      ↓
Index Search
      ↓
후보 2,000개
      ↓
Heap
      ↓
Top 10
      ↓
10개 파일만 다시 순차 Scan
```

AND / OR의 `matchedLines`는 매칭에 기여한 각 Posting의 `lines`를 줄 번호 순으로 합친 것이다. 같은 줄은 한 번만 두고, 문서당 3줄 제한은 그대로 적용한다.

---

## AND Search

```text
자료구조 AND 알고리즘
```

Posting List는 `documentId` 오름차순이다. 별도의 집합을 만들지 않고 두 리스트를 two pointer로 합쳐 교집합을 구한다. 두 단어가 같은 줄에 있는지는 보지 않는다.

```text
i ↓
자료구조  [1, 3, 7, 12]

j ↓
알고리즘  [2, 3, 7, 15]
```

둘 중 `documentId`가 작은 쪽을 한 칸 옮긴다. 값이 같으면 결과에 넣고 둘 다 옮긴다.

```text
결과 → [3, 7]
```

복잡도는 두 리스트 길이를 n, m이라 할 때 O(n + m)이다. 키워드가 셋 이상이면, `documentId` 순인 `SearchCandidate` 목록을 다음 Posting List와 같은 방식으로 다시 합친다.

교집합에 들어간 문서마다 `SearchCandidate` 하나를 만든다. P0에서 `score`는 각 Posting의 `termFrequency`를 더한 값이다. `matchedLines`는 두 `lines`를 줄 번호 순으로 합친 것이다. Index의 Posting은 그대로 둔다.

```text
자료구조 Posting ─┐
                  ├→ QueryProcessor → SearchCandidate
알고리즘 Posting ─┘
```

```text
자료구조 tf=5, 알고리즘 tf=3
score = 5 + 3 = 8
```

---

## OR Search

AND와 같은 Sorted Posting List Merge로 합집합을 구한다. 같은 문서가 양쪽에 있으면 `SearchCandidate`는 하나이다. P0에서 `score`는 각 `termFrequency`의 합이고, `matchedLines`는 줄 번호 순으로 합친다. Index의 Posting은 그대로 둔다.

```text
{1, 2, 3, 7, 12, 15}
```

---

# 11. Ranking

검색 조건을 만족하는 모든 문서를 동일하게 표시하지 않고 관련도를 계산하여 순위를 부여한다. 사용자에게는 상위 K개만 반환한다.

## P0 Ranking

점수는 `termFrequency`이다.

```text
score = termFrequency
```

```text
검색어: android

doc1 → termFrequency 12
doc2 → termFrequency 5
doc3 → termFrequency 2
```

따라서:

```text
doc1 > doc2 > doc3
```

AND / OR에서 여러 단어가 매칭되면, P0에서는 매칭된 각 단어의 `termFrequency`를 `SearchCandidate.score`에 합산한다. 이 합은 검색 결과의 점수이고, Index에 저장된 `termFrequency`는 단어별로 유지한다.

---

## P1 Ranking

TF-IDF 또는 BM25 방식으로 확장한다. `termFrequency`를 각 term의 빈도 입력값으로 활용한다. 각 query term의 점수를 구한 뒤 더한다.

이를 통해 모든 문서에서 자주 등장하는 일반적인 단어의 영향력을 낮추고 특정 문서를 잘 구분하는 단어의 중요도를 높인다. 문서 길이는 `Document.tokenCount`를 사용한다.

---

# 12. Heap / Priority Queue

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

Heap은 검색 Index를 저장하는 구조가 아니라 **검색 과정에서 상위 결과를 선택하기 위한 임시 자료구조**로 사용한다. 매칭 줄 원문은 이 Top-K 문서만 다시 열어 순차적으로 읽으며 추출한다.

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

따라서 검색 성능 개선을 위해 Tokenizer 계층을 별도로 구성한다. 인터페이스는 7.2와 같다. 한 줄의 텍스트를 받아 토큰 목록만 반환하고, 줄 번호는 파일을 읽는 쪽이 붙인다.

### SimpleTokenizer

P0에서 사용한다.

* 공백 분리
* 특수문자 제거
* 영문 lowercase 변환

### KoreanTokenizer

P1 이후 선택적으로 적용한다.

후보:

* Kiwi C++ 형태소 분석기

형태소 분석을 통해 조사/어미 등의 기능어를 제거하고 의미 있는 형태소를 Index에 저장한다. 분석기에는 한 줄만 넣고, 반환된 토큰마다 그 줄 번호를 붙인다. `termFrequency`는 토큰이 나온 횟수만큼 증가한다.

```text
12| 자료구조를 공부한다

↓ Kiwi

자료구조 / NNG
를       / JKO
공부     / NNG
하       / VV
ㄴ다     / EF

↓

줄 12  자료구조
줄 12  공부
```

한국어 NLP 구현 자체가 프로젝트의 주요 목표가 되지 않도록 외부 라이브러리를 활용한다.

---

# 15. 개발 단계

## P0 — Core Search Engine

**목표: 줄 번호를 포함한 검색으로 키워드가 나온 줄을 찾는다**

필수 기능:

* TXT / Markdown 문서 읽기
* 줄 단위 Tokenization과 출처 줄 번호 기록
* Line-aware Inverted Index 생성
* Posting List (`documentId`, `termFrequency`, `lines`)
* 단일 Keyword Search
* 문서당 최대 3개의 매칭 줄 출력
* `termFrequency` 기반 Ranking
* Heap을 이용한 Top-K 결과
* 문서 추가 및 전체 Re-indexing

완료 조건:

```text
문서 디렉토리 입력
       ↓
Line-aware Index 생성
       ↓
검색어 입력
       ↓
관련 문서 Top-K와 매칭 줄 반환
```

---

## P1 — Advanced Lexical Search

**목표: 검색 품질 개선**

기능:

* AND / OR 검색 (Sorted Posting List Merge)
* TF-IDF 또는 BM25 Ranking
* Trie 기반 자동완성
* 한국어 Tokenization 개선
* Kiwi 연동 검토
* Stop Word 처리
* JSON 파일 기반 Index 저장 / 로드

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
Heap Top-K
  ↓
Top-K 파일을 한 번 순차 Scan하여 매칭 줄 추출
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
5. 관련도에 따라 Top-K 결과를 제공한다.
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
* 자체 형태소 분석기 개발

---

# 19. 예상 기술 스택

### Core

* C++17 이상
* CMake
* STL

### Optional

* Kiwi — 한국어 형태소 분석
* EmbeddingGemma — Semantic Search
* GoogleTest / Catch2 — Unit Test

---

# 20. 프로젝트 핵심 설명

본 프로젝트의 핵심 아이디어는 다음 한 문장으로 정리할 수 있다.

> 모든 문서를 검색 시점에 순차 탐색하는 대신, 토큰의 등장 횟수와 나온 줄을 Line-aware Inverted Index로 사전에 저장하여 관련 문서를 찾고, Top-K 문서에서 그 줄이 어떻게 나오는지를 함께 반환한다.

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
