# CM source_editable/main_cm.cc

## 소스 경로

- path: `CM/source_editable/main_cm.cc`
- project: `CM`
- language: `C++`

## 단일 책임

- CM이 `main()`에 진입했음을 표시하고 CPU1 boot-ready token을 기다린 뒤 ACK token을 write한다.
- 현재 파일은 CM UART bring-up의 선행 gate만 제공한다.
- 이 파일은 C28x SCI, CPU2 fast loop, EPWM/ADC/CMPSS 설정을 맡지 않는다.

## 실행 코어와 초기화 순서

- 실행 코어: `CM`
- 호출 위치: CM release 후 `main()`
- 현재 흐름: `CM_init() -> main_entered latch -> CPU1 token polling -> ACK write -> idle loop`
- 선행 조건: CPU1이 CM을 release하고 CPU1->CM mailbox에 agreed token을 써야 한다.
- 후행 조건: CPU1이 CM ACK token을 읽을 수 있어야 한다.

## 주요 입력과 출력

- 입력: `g_cpu1_to_cm_mailbox`
- 출력: `g_cm_to_cpu1_mailbox`, `g_cm_main_entered`, `g_cm_local_handshake_status`, `g_cm_liveness_count`
- shared state:
  - `g_cpu1_to_cm_mailbox`
  - `g_cm_to_cpu1_mailbox`
- 외부 관측값: CM main 진입 변수, ACK token, 향후 UARTA TX GPIO84 파형

## Hardware / Driverlib 의존성

- peripheral: CM system init, CPU1-CM MSGRAM, 향후 UART0/UARTA
- driverlib/API: `CM_init()`, `DEVICE_DELAY_US()`
- pinmux 의존성: CM UARTA bring-up은 GPIO84/85 기준이며 CPU1 pinmux/SysConfig와 충돌하면 안 된다.
- clock source: CM init과 AUX clock 설정

## Memory / Startup / Linker 의존성

- section:
  - `MSGRAM_CPU1_TO_CM`
  - `MSGRAM_CM_TO_CPU1`
- linker command file:
  - `CM/2838x_RAM_lnk_cm.cmd`
  - `CM/2838x_FLASH_lnk_cm.cmd`
- startup/vector 의존성: `CM/source_editable/startup_cm.c` vector table은 현재 변경 대상이 아니다.
- IPC MSGRAM 의존성: CPU1 쪽 `MSGRAM_CPU_TO_CM`, `MSGRAM_CM_TO_CPU`와 주소가 대응되어야 한다.

## Interrupt / Trigger / Timing

- ISR: 없음
- trigger source: CPU1 boot-ready token polling
- bounded 조건: 현재 CM은 token 대기 loop에 timeout이 없다. CPU1 쪽 timeout으로 실패를 latch한다.
- background 공유 데이터: mailbox와 status 변수는 debugger 관측 대상이다.
- 동기화 방식: 초기 bring-up v1에서는 `volatile` mailbox polling

## 핵심 invariant

- CM UART는 CPU1 boot-ready token 확인과 ACK write 이후에만 붙인다.
- CM idle loop는 CPU1 heartbeat나 CPU2 fast loop에 의존하지 않는다.
- C++ 기능은 무거운 런타임 기능 없이 제한적으로만 사용한다.

## Fail-fast / 실패 처리

- 실패 조건: CPU1 token을 보지 못해 ACK를 쓰지 못함
- 시스템 동작: `CM_STATUS_WAITING_BOOT_READY` 상태 유지
- 후속 기능 차단 조건: ACK 전에는 UART/CAN/Ethernet/USB bring-up을 시작하지 않는다.
- debug 관측 방법: `g_cm_main_entered`, `g_cm_local_handshake_status`, MSGRAM mailbox 값 확인

## 관련 문서

- 기능 spec: `docs/specs/001-cpu1-cm-handshake/`
- architecture:
  - [boot-flow](../../../architecture/boot-flow.md)
  - [ipc-routing](../../../architecture/ipc-routing.md)
  - [memory-map](../../../architecture/memory-map.md)
- code collaborators:
  - [CPU1 main_cpu1.c](../../CPU1/source_editable/main_cpu1.c.md)

## 관련 테스트와 검증

- bench 절차: CM image load, CPU1에서 CM release, CM status 변수와 ACK token 확인
- 기대 관측값: `g_cm_main_entered == 1`, `g_cm_local_handshake_status == CM_STATUS_ACK_WRITTEN`
- 성공 조건: CPU1이 ACK token을 timeout 전에 관측
- 실패 시 확인 항목: CM boot mode, CM image load 여부, MSGRAM section 이름, CPU1 token 값

## 변경 시 주의점

- UART TX smoke를 추가할 때 GPIO84/85와 CM UART peripheral 기준을 code note와 spec에 남긴다.
- CM UART와 C28x SCIA/SCIB를 혼동하지 않는다.
- Ethernet/EtherCAT은 board 실장 상태 확인 없이 이 파일에 붙이지 않는다.
