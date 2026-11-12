# easyDSP 사용 안내와 공식 자료

F28388D에서 easyDSP를 연결하고, 실행 중인 변수 값을 읽고 쓰며, 통신 실패를 진단할 때 사용하는 자료다. 공식 웹 도움말은 한국어·영어 HTML 두 파일로 묶고, 이 저장소에 필요한 절차는 Markdown으로 정리했다.

확인일은 2026-11-12다. 공식 사이트에서 안내하는 PC 프로그램은 v12.7이고, 이 프로젝트에 포함된 C28x 통신 소스는 v12.2다. 두 버전 번호는 별개다.

## 파일 구성

| 파일 | 내용 |
| --- | --- |
| [README.md](README.md) | 읽는 순서와 자료 구성 |
| [setup.md](setup.md) | Pod, Windows 드라이버, 프로젝트 생성과 심볼 갱신 |
| [f28388d-guide.md](f28388d-guide.md) | C28x·멀티코어 주의점과 PEETS의 enable 제어 |
| [windows-and-commands.md](windows-and-commands.md) | Watch, Command, Plot, Chart 등 조작법 |
| [troubleshooting.md](troubleshooting.md) | 통신, 변수, 부팅·플래시 오류 진단 |
| [source-index.md](source-index.md) | 전체 원문 목록, 다운로드 링크, 수집 오류 |
| [official-ko.html](official-ko.html) | 한국어 원문 116개와 내장 그림 457개 |
| [official-en.html](official-en.html) | 영어 원문 113개와 내장 그림 410개 |

## 읽는 순서

1. 현재 enable 조작을 이해하려면 [F28388D 안내](f28388d-guide.md)의 프로젝트 적용 부분부터 본다.
2. easyDSP 환경을 처음 구성하면 [연결과 설정](setup.md)을 따른다.
3. 값을 바꾸거나 기록하려면 [창과 명령어](windows-and-commands.md)를 본다.
4. 값이 `?`로 보이거나 부팅이 실패하면 [오류 진단](troubleshooting.md)에서 증상을 찾는다.
5. 다른 MCU나 상세한 그림·표가 필요하면 통합 HTML 또는 [전체 출처 목록](source-index.md)을 연다.

## 통합 HTML 사용

HTML을 브라우저에서 직접 연다. 서버나 추가 이미지 폴더는 필요 없다. 왼쪽 목차에서 제목을 검색하고, 본문 검색에는 `Ctrl+F`를 쓴다. 그림은 HTML 내부에 포함되어 있으며 표시에 JavaScript를 사용한다.

본문·표·코드·수집된 그림을 보존하고 기존 프레임 탐색은 새 목차로 바꿨다. PDF와 설치 프로그램은 [공식 다운로드 링크](source-index.md#공식-다운로드)로 제공한다. 전체 수집 기록과 원문 SHA-256은 각 HTML 내부의 `collection-manifest`에 들어 있다.

## 프로젝트와 공식 예제의 경계

공식 멀티코어 예제와 이 저장소의 boot·IPC 구성은 다르다. 공식 예제의 `easyDSP_Boot_Sync()`를 현재 코드에 그대로 추가하거나, GPIO·링커·IPC 배치를 바꾸는 근거로 사용하지 않는다. 현재 실행 경로와 비교한 설명은 [F28388D 안내](f28388d-guide.md)에 둔다.

이 자료 정리는 펌웨어, CCS 설정, easyDSP 프리셋을 변경하지 않는다. 실보드 시험 결과는 기존 [005 검증 문서](../specs/005-fullbridge-adc-dac/verification.md)에 별도로 기록한다.

공식 출처: [easyDSP 홈페이지](https://www.easydsp.com/), [한국어 웹 도움말](https://www.easydsp.com/Web%20Help/index.htm).
