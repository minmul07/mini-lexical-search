# 테스트 가이드

CLI·명령 처리 또는 공통 Application 변경 시 다음 검증을 수행한다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

* `version_test`: 버전 문자열 확인
* `query_processor_test`: 단일 검색, AND 교집합·OR 합집합, 다중 term, TF 합산, 줄 정렬·중복 제거, 반복 term 점수 중복 방지, 없는 term·빈 인덱스, 질의 문법 오류, 대소문자 정책, Posting 불변성 확인
* `matching_lines_test`: 1-based 줄 번호, CRLF·빈 줄·마지막 개행 없는 파일, 기본 무제한 및 양수 한도(매칭 행 수보다 작음·같음·큼), 빈 결과, 잘못된 줄 목록, 원본 삭제·기록된 줄 부재 확인
* 검색 CLI 추가 검증: `/index` 직후 및 재실행 후 전체 문서·전체 매칭 행 출력, ID순 출력, 비매칭 행 제외, 비매칭 파일 미열람, TF 점수, AND/OR, 무매칭·질의 오류 후 입력 처리, 스냅샷 내용·수정 시간 보존, 파일 읽기 실패 후 결과 출력 지속, 손상 스냅샷 검색 거부
* `tokenizer_test`: 공백 분리, ASCII 영숫자 유지, 비허용 문자(특수문자·전각 문장부호·전각 영숫자·BOM·이모지·기타 언어) 분리·제외, 한글 완성형·호환 자모 유지, 한글+ASCII 연속 유지, ASCII 소문자 변환, 잘못된 UTF-8(오버롱·서로게이트·불완전 바이트) 분리, 빈 줄·문장부호 전용 줄, 탭·CR 분리, 반복 토큰 유지 확인
* `sha256_test`: FIPS 180-4/NIST 표준 벡터(빈 문자열, `abc`, 2·3블록, 100만 `a`), 패딩 경계(55·56·57·63·64·65·119·120·128바이트), 청크 분할 `update`와 일괄 해시 결과 일치, 64자 소문자 16진수 인코딩 확인
* `document_test`: 재귀 수집, 확장자·심볼릭 링크 필터링, ID·경로·제목·`tokenCount=0`(수집 단계), JSON 파싱·이스케이프·UTF-8, 빈 목록 저장, 재저장, SHA-256 표준 벡터·다중 청크·바이너리, 네 가지 변경 집계, 기존 ID 보존, 변경 없는 JSON 내용·수정 시간 보존, 기존 스키마 전환, 잘못된 JSON 거부, 잘못된 입력 및 저장 실패 시 기존 파일 보존 확인. 추가로 변경 문서만 토큰화되는지, 토큰의 원본 1-based 줄 번호, 줄을 넘는 토큰 수 집계, `tokenCount` 기록, 게이트된 문서의 파일 미열람·JSON 미재저장, 빈 토큰 문서 처리 확인
* `inverted_index_test`: term 추가, `termFrequency` 누적, 같은 줄 중복 제거, 줄·문서 정렬 유지, 없는 term 조회, 문서 제거와 빈 term 정리, `merge`와 중복 문서 거부, 뒤쪽 term 충돌 시 앞쪽 term 추가·확장을 포함한 전체 Posting 상태 보존, 병합 정렬·원본 보존·빈 인덱스 및 자기 병합, `fromPostings` 형식 불변식(정렬·줄 번호·TF) 검증
* `indexer_test`: 스냅샷 왕복 저장·로드, 결정적 JSON 형식, 최초 전체 구축, 변경 없는 실행의 토큰화 생략·파일 미재저장, 변경 문서 Posting 재구축, 문서 삭제 시 Posting 제거, 빈 토큰 문서, 저장 실패 시 파일·메모리 보존과 재시도, 손상 스냅샷 거부 확인
* `search_application_test`: CLI 없이 인덱싱·재로드·검색, 진행 이벤트 순서·집계, 대소문자·AND/OR·질의 오류 후 재검색, 원문 행과 양수 한도·전체 매칭 행 수, 변경 없는 재실행의 내용·수정 시간 보존, 메타데이터·토큰화 실패 시 완료 이벤트 생략, 저장·콜백 실패 후 복구·재시도(최초 저장 포함), 반환 결과의 값 소유, 문서 읽기 경고 후 결과 지속, 검색의 스냅샷 불변성, 손상 스냅샷에서 `index` 거절과 `rebuildIndex`의 저장 실패·성공 확인. 스냅샷이 없으면 `workingJob`은 `Ready`, 손상되거나 재생성에 실패하면 `Failed to load index`, 재생성이나 인덱싱 성공 후에는 `Ready`
* `cli_test`: `start --no-tui` 실행, `--no-tui start` 동일 REPL 진입, `--no-tui`만 있으면 사용법 오류, 도움말에 `--no-tui` 표시, 실행 시 도움말·버전 옵션, 잘못된 실행 인자, 실행 중 슬래시 명령, `documents/recap` 입력 및 기본 `./documents` 입력의 실제 스냅샷 저장(`version`·`documents`·`invertedIndex`), 메타데이터·토큰화 단계 메시지와 ms 시간, 앱 재실행 후 스냅샷 로드·해시 비교·변경 집계 및 변경 없는 문서의 토큰화·재저장 생략, `tokenCount`와 Posting 저장, 삭제 문서 Posting 제거, 손상된 스냅샷에서 `n`을 입력하면 종료하고 파일을 유지, 잘못된 명령 이후 입력 처리, 빈 입력, EOF 및 종료 명령 확인

대화형 입력과 프롬프트는 `./build/mini_lexical_search start --no-tui`로 직접 실행하여 `/index`, `/search 자료구조`, `/search 자료구조 AND 알고리즘`, `/search 자료구조 OR 알고리즘`, `/help`, `/exit`를 입력해 확인한다. TUI는 `./build/mini_lexical_search start`로 열어 확인한다. 터미널 중앙의 흰색 테두리 입력 상자(내용 영역 최대 100열, 너비 초과 시 줄바꿈·높이 증가)에 같은 명령을 입력하고 Enter로 실행한다. 상자는 비워지고, 출력은 상자 아래 같은 너비 안에 나타나며 다음 명령이 그 출력을 교체한다. 공백 Enter는 기존 출력을 유지한다. 우측 상단에는 `workingJob` 라벨이 단순 텍스트로 표시된다. 시작 직후는 `Ready`다. 스냅샷이 손상되면 입력 상자 전에 `reset index`와 `quit` 팝업이 열리고, 좌우 화살표·Enter·마우스 클릭으로 고른다. 마우스가 움직이면 그 위치만 반전되고, 이후 키 입력은 키보드 선택만 반전한다. `/exit`, `/quit`, Esc, Ctrl+C로 종료한다. 화면 높이를 넘는 출력은 스크롤하지 않는다. TUI 화면은 CTest로 검증하지 않는다. `/index [디렉터리]`는 `.data/index.json`을 저장하므로 현재 작업 디렉터리의 `.data/`에 쓰기 권한이 필요하다. 자동 저장 테스트는 빌드 디렉터리의 격리된 경로를 사용한다. 검색은 기본 한도 0으로 모든 매칭 문서의 모든 매칭 행을 출력한다.
