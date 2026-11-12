# easyDSP 오류 진단

다운로드 성공, 통신 성공, 올바른 변수 표시, 실제 보드 출력은 각각 확인한다. 특히 ROM 부트 통신이 성공해도 사용자 프로그램의 easyDSP RX ISR이 동작한다는 뜻은 아니다.

## 증상별 확인

| 증상 | 먼저 확인할 항목 |
| --- | --- |
| Pod를 인식하지 못함 | USB 연결, D2XX 드라이버와 장치 관리자. PC의 다른 USB 포트·케이블로 비교 |
| 모든 값이 `?` | 실행 중인 이미지, 통신 초기화 호출, 실제 baud rate, GPIO 중복 할당, RX ISR 실행 시간 |
| 처음에는 되다가 끊김 | 상위 ISR 부하 증가, 통신 변수·창 수, 갱신 주기, 스위칭 노이즈 |
| 변수 이름이 없음 | 올바른 출력 파일인지, 최적화·링커가 제거했는지 map에서 확인 |
| 형식이 전부 int 등으로 잘못 보임 | 출력 파일과 easyDSP 프로젝트의 DWARF/COFF 설정 일치 |
| 새 코드와 값이 맞지 않음 | 현재 실행 이미지의 출력 파일로 Load Symbol 수행 |
| Boot 성공 후 Verify 실패 | 사용자 통신 소스·초기화, 헤더 baud·실제 SCI baud·PC 설정 일치 여부 |
| Auto Bauding 실패 | ROM 부트 GPIO, BOOT/XRS 타이밍, RX/TX 에코를 scope로 확인 |
| RAM Boot에 FLASH 주소 오류 | RAM 구성으로 빌드했는지, initialized section 배치와 ROM 사용 영역 |
| RAM Boot가 초기화 중 리셋됨 | startup의 watchdog 처리와 entry point를 확인 |
| FLASH 주소·정렬 오류 | map의 문제 section, 해당 MCU linker ALIGN 조건 |
| `The address xxx is not flash area` | FLASH 이미지의 initialized section이 RAM에 위치했는지 확인 |
| CPU2 RAM 부팅이 멈춤 | 공식 easyDSP boot 경로를 쓰는 경우 각 코어 boot/sync 호출과 공유 메모리 배치 확인 |

출처: [공통 문제 해결](https://www.easydsp.com/Web%20Help/topics/FAQ%20trouble%20shooting%20General.html), [C28x 문제 해결](https://www.easydsp.com/Web%20Help/topics/C2000/FAQ%20trouble%20shooting%20TI.html), [공통 주의 사항](https://www.easydsp.com/Web%20Help/topics/Common%20Usage%20Caution_kor.htm).

## 통신 부하를 줄이는 순서

Command 창에서 변수 한 개만 조회해 기준을 잡는다. 이후 필요한 Watch 항목을 늘리고, 갱신 주기를 조절한다. 필요하면 `Wait-more time`을 늘리거나 baud rate를 낮추되 MCU 설정도 함께 맞춘다. 그래도 실패하면 RX ISR에 할당되는 실행 시간을 확인한다.

프로젝트의 `MCU > Communication Status`에서 실패율을 확인할 수 있다. 공식 메뉴 안내는 원활한 사용을 위한 성공률 기준으로 90% 이상을 제시한다. 이 값이 개별 읽기·쓰기의 정확성이나 제어 안전성을 보장하는 수치는 아니다.

출처: [공통 문제 해결](https://www.easydsp.com/Web%20Help/topics/FAQ%20trouble%20shooting%20General.html), [MCU 공통 메뉴](https://www.easydsp.com/Web%20Help/topics/MenuCommon.htm).

## 플래시 검증과 리셋

`Program` 성공 표시만으로 프로그래밍 내용을 검증했다고 기록하지 않는다. 별도 Verify 또는 `Program+Verify` 결과를 확인한다. 플래시 작업용 로더와 사용자 프로그램을 구분하고, 작업 후 `Reset > Exit`로 사용자 프로그램이 시작하는지도 확인한다.

RAM Boot Verify는 실행 중 바뀌는 데이터와 사용자 통신 경로의 영향을 받을 수 있다. 실패 주소·기대값·읽힌 값을 기록하고 원인을 분리한다. easyDSP는 변수 읽기·쓰기에 오류 가능성이 있다고 안내하므로 중요한 설정은 읽어 확인하고 실제 동작과 대조한다.

출처: [C28x MCU 메뉴](https://www.easydsp.com/Web%20Help/topics/C2000/Menu%20DSP%2028x.htm), [FAQ](https://www.easydsp.com/Web%20Help/topics/FAQ%20General.html).

## PEETS에서 추가 확인

- `?`가 계속되면 CPU1에서 `easyDSP_SCI_Init()`에 도달했는지 확인한다. 현재 코드는 handshake 전에 초기화한다.
- GPIO28/29 설정 후 GPIO34/49 등 다른 SCIA RX/TX 설정이 덮어쓰거나 중복되지 않는지 확인한다.
- PWM이 안 나오면 `g_pwm_status.off_reason`과 버퍼 상태를 먼저 읽는다. 화면에서 enable을 1로 쓴 사실만으로 출력 성공을 판단하지 않는다.
- CPU2 상태는 현재 CPU1 쪽 `g_cpu2_monitor` 미러를 통해 읽는다. CPU2 직접 Pod 연결 예제와 구분한다.

근거: [CPU1 main](../../CPU1/source_editable/main_cpu1.c), [CPU2 main](../../CPU2/source_editable/main_cpu2.c), [출력 상태 설명](f28388d-guide.md#출력-차단-이유).

## 버전 차이

공식 이력에는 v12에서 F2837xD/F2838xD CPU2 DriverLib 통신 소스 오류 수정이 기록되어 있다. PC 프로그램을 업데이트하는 것과 프로젝트에 포함된 소스를 교체하는 것은 별도 작업이다. 현재 저장소의 v12.2 소스를 과거 예제로 되돌리지 않는다.

v12.6에서 Plot2, 최소 갱신 주기 변경, Command의 delay 기능 등이 추가되었다. 설치된 PC 프로그램의 버전이 다르면 화면과 메뉴가 달라질 수 있다. 전체 이력과 다른 MCU의 제한은 통합 도움말에서 확인한다.

출처: [업데이트 이력](https://www.easydsp.com/Web%20Help/topics/Updates%20History.htm).
