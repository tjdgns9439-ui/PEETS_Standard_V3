# easyDSP 연결과 프로젝트 설정

easyDSP는 MCU의 SCI/UART 통신으로 실행 중인 변수와 메모리를 관찰·변경한다. 컴파일과 링크는 CCS 등 기존 IDE가 담당하고, easyDSP는 해당 출력 파일의 심볼 정보를 사용한다.

## Pod와 보드 연결

| Pod 종류 | 대상 | 절연 방식 |
| --- | --- | --- |
| 1 | TI C2000 | 기본 절연 |
| 2 | TI C2000 | 광케이블 |
| 3 | C2000 이외 지원 MCU | 기본 절연 |
| 4 | C2000 이외 지원 MCU | 광케이블 |

정확한 MCU 지원 목록은 [공식 제품 종류](https://www.easydsp.com/Web%20Help/topics/easyDSPproductportfolio.htm)와 통합 도움말에서 확인한다. F28388D CPU1용 연결은 SCI-A GPIO28(RX), GPIO29(TX)를 기준으로 한다. 보드 커넥터와 핀 번호는 [보드 I/O 기준](../board/board-io-reference.md)을 함께 본다.

공식 F2838x 연결 안내는 헤더 전원 핀 #4에 3.3 V를 연결하고 RX/TX를 해당 MCU 핀에 연결하도록 한다. 다운로드에는 부트·리셋 연결도 필요하다. 단순 모니터링과 다운로드의 연결 조건을 구분한다. 기본 부트 모드 표는 공장 출하 설정을 전제로 하므로 사용자 부트 설정이 바뀐 칩에는 그대로 적용하지 않는다.

출처: [F2838x 보드 설정](https://www.easydsp.com/Web%20Help/topics/C2000/28x%20-%20Usage%20TMS320F2838xD%20HW%20Setting.htm).

## Windows 드라이버

공식 안내는 64비트 Windows를 대상으로 한다. Pod를 PC에 연결하기 전에 easyDSP 설치 폴더의 `Driver` 폴더에 포함된 드라이버를 설치한다. 현재 설치 안내에 기재된 파일명은 `CDM212364_Setup.exe`다. 과거 업데이트 이력의 파일명과 다를 수 있으므로 실제 배포 패키지의 설치 안내를 우선한다.

설치 후 Pod를 연결하면 장치 관리자에서 `USB Serial Converter A/B`를 확인한다. 드라이버는 FTDI FT2232의 D2XX 계열이다. 일반 USB-UART의 COM 포트를 고르는 절차와 혼동하지 않는다.

출처: [드라이버 설치](https://www.easydsp.com/Web%20Help/topics/DriverInstallation/Driver_Installation_General.htm). 드라이버 설치·제거를 이번 문서 작업에서 실행하지 않았다.

## MCU 프로그램 준비

1. 설치 폴더의 `Source`에서 대상 MCU와 개발 방식에 맞는 통신 소스·헤더를 고른다. C28x는 Bitfield와 DriverLib 경로를 구분한다.
2. 헤더의 대상 MCU, 보드레이트, 인터럽트 관련 설정을 맞춘다.
3. 초기화 경로에서 통신 초기화 함수를 호출하고 필요한 RX ISR을 포함한다.
4. 디버깅 정보가 포함된 출력 파일을 빌드한다.

이 저장소에는 이미 통신 소스가 들어 있다. 현재 초기화 순서는 [F28388D 안내](f28388d-guide.md)에서 확인한다. 공식 예제의 초기화 코드를 중복 추가하지 않는다.

출처: [시작하기](https://www.easydsp.com/Web%20Help/topics/How%20to%20start%20easyDSP.htm), [DriverLib 사용](https://www.easydsp.com/Web%20Help/topics/C2000/28x%20-%20Usage%20TMS320F28XX%20Programming%20DriverLib_kor.htm).

## easyDSP 프로젝트 생성

`Project > New`에서 `.ezd` 파일 이름을 지정한다. Basic 페이지에서 MCU, 링크 출력 파일, 필요한 debugging model을 선택한다. 출력 파일은 프로젝트 생성 전에 존재해야 한다. Basic 속성은 생성 후 수정할 수 없으므로 이 항목을 바꿔야 하면 새 프로젝트를 만든다.

Hardware 페이지의 baud rate는 MCU의 실제 통신 속도와 일치시킨다. `Wait-more time`은 응답 대기 시간을 추가하는 값이며 최대 300 ms다. Miscellaneous 페이지에서는 변수 이름 자동 찾기, 연속 실패 시 통신 정지, 변경 값 강조 등을 설정한다.

멀티코어에서는 다운로드할 코어의 출력 파일과 실제 Pod가 통신할 코어를 구분해서 지정한다. F2838x의 CPU2/CM 다운로드는 CPU1에 연결된 프로젝트에서 수행한다.

출처: [Project 메뉴](https://www.easydsp.com/Web%20Help/topics/Menu%20Project.htm).

## 저장 파일과 심볼 갱신

| 파일 | 역할 |
| --- | --- |
| `.ezd` | 프로젝트 환경 |
| `.cfg` | 창 배치, 감시 목록 등 기타 환경 |
| `.vars` | 변수 정보 |
| `easyDSP_FlashApiWrapper*` | C28x 플래시 처리를 위해 생성하는 임시 산출물 |
| `.ez.bin`, `.ez.hex` | MCU 계열에 따라 생성되는 다운로드 산출물 |

CCS로 다시 프로그램한 뒤에는 easyDSP의 `MCU > Load Symbol`로 현재 실행 이미지와 같은 출력 파일의 변수 정보를 다시 읽는다. 심볼만 다시 읽는 동작은 MCU를 다시 프로그램하는 동작과 다르다. 메뉴 표기는 프로그램 버전에 따라 과거 문서의 `Reload *.out`과 다를 수 있다.

출처: [Project 메뉴](https://www.easydsp.com/Web%20Help/topics/Menu%20Project.htm), [MCU 공통 메뉴](https://www.easydsp.com/Web%20Help/topics/MenuCommon.htm).
