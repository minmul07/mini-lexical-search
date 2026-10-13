## 추후 작업 필요

- 제출용 Makefile 작성 및 검증: Ubuntu 24.04 / GNU g++ / C++20에서 별도 설치·네트워크 없이 `make` 한 번으로 실행 파일 생성
- [승인 범위](FTXUI_APPROVAL.md)에 따라 FTXUI 버전·배포 파일·라이선스 확인 후 vendoring 및 UI 구현. 핵심 모듈의 FTXUI 의존성 없이 검색어 입력·결과 표시·키보드 처리·화면 전환·상태 표시에만 사용
- `--no-tui` CLI 실행 경로 구현 및 TUI와 동일한 핵심 검색 결과 검증
- FTXUI를 포함한 최종 제출 용량 10MB 이하 확인
- `/search`에서 문서당 최대 매칭 행 수를 지정하는 CLI 옵션 추가. 현재 내부 인자 `maxMatchingLines`는 기본 `0`(무제한)이며, 양수는 앞 N개 행으로 제한한다.
