# 테스트 가이드

CLI 또는 명령 처리 변경 시 다음 검증을 수행한다.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

* `version_test`: 버전 문자열 확인
* `document_test`: 재귀 수집, 확장자·심볼릭 링크 필터링, ID·경로·제목·`tokenCount=0`, JSON 파싱·이스케이프·UTF-8, 빈 목록 저장, 재저장, SHA-256 표준 벡터·다중 청크·바이너리, 네 가지 변경 집계, 기존 ID 보존, 변경 없는 JSON 내용·수정 시간 보존, 기존 스키마 전환, 잘못된 JSON 거부, 잘못된 입력 및 저장 실패 시 기존 파일 보존 확인
* `cli_test`: `start` 실행, 실행 시 도움말·버전 옵션, 잘못된 실행 인자, 실행 중 슬래시 명령, `documents/recap` 입력 및 기본 `./documents` 입력의 실제 JSON 저장, 앱 재실행 후 해시 비교·변경 집계, 잘못된 명령 이후 입력 처리, 빈 입력, EOF 및 종료 명령 확인

대화형 입력과 프롬프트는 `./build/mini_lexical_search start`로 직접 실행하여 `/index`, `/search 자료구조`, `/help`, `/exit`를 입력해 확인한다. `/index [디렉터리]`는 `.data/documents.json`을 저장하므로 현재 작업 디렉터리의 `.data/`에 쓰기 권한이 필요하다. 자동 저장 테스트는 빌드 디렉터리의 격리된 경로를 사용한다. 토큰화·역색인·검색은 미구현이므로 검색 결과는 검증하지 않는다.
