# F28388D와 PEETS 프로젝트의 easyDSP 사용

공식 C28x 사용 조건과 이 저장소의 현재 구현을 구분한다. 아래 프로젝트 설명은 2026-11-12에 읽은 소스 기준이며, 보드에서 새로 관측한 시험 결과를 뜻하지 않는다.

## C28x 통신 소스와 빌드

F2838x CPU1/CPU2의 공식 DriverLib 안내는 `easy28x_DriverLib_v12.2.c/h`를 사용한다. CM용은 별도의 `easy28x_cm_DriverLib_v10.1.c/h`와 `easyDSP_UART_Init()`을 사용한다. C28x SCI와 CM UART는 같은 주변장치가 아니다.

대상 코어의 `CPU1`, `CPU2`, `CM` 및 FLASH 구성의 `_FLASH` 정의가 코드 경로를 결정한다. C28x debugging model은 출력 파일과 easyDSP 프로젝트에서 맞춰야 하며 공식 안내는 DWARF를 권장한다. SCI 속도 계산에 사용하는 `DEVICE_LSPCLK_FREQ`도 실제 클럭과 맞아야 한다.

RX ISR이 실행될 시간을 확보하고 인터럽트 nesting 구성을 확인한다. FLASH 동작에서 ISR을 RAM으로 배치할 때는 pragma뿐 아니라 linker section과 복사 경로를 함께 확인한다. 통신 내부 상태인 `ezDSP_*` 변수는 사용자 제어 변수로 취급하지 않는다.

출처: [C28x 일반 사항](https://www.easydsp.com/Web%20Help/topics/C2000/28x_Usage_TMS320F28XX_Programming_common.htm), [DriverLib 사용](https://www.easydsp.com/Web%20Help/topics/C2000/28x%20-%20Usage%20TMS320F28XX%20Programming%20DriverLib_kor.htm).

## 공식 멀티코어 구성

공식 F2838xD 예제는 코어별 직접 모니터링에 각각 Pod와 easyDSP 인스턴스를 사용한다. CPU1은 SCI-A GPIO28/29, 제공 예제의 CPU2는 SCI-B GPIO15(RX)/14(TX), CM은 UART GPIO85(RX)/84(TX)를 사용한다. CPU1에 연결된 Pod 하나로 CPU1/CPU2/CM의 RAM 다운로드와 플래시 프로그래밍을 할 수 있지만, 다른 코어의 직접 모니터링 연결까지 생기는 것은 아니다.

CPU1용 easyDSP 프로젝트에 사용하는 코어의 출력 파일을 모두 지정한다. CPU2/CM의 별도 프로젝트를 사용한다면 CPU1 프로젝트에서 지정한 해당 코어 이미지와 일치시킨다.

출처: [F2838xD 사용](https://www.easydsp.com/Web%20Help/topics/C2000/TMS320F2838xD_operation.htm).

## 공식 RAM 부팅에서의 자원 제약

다음 내용은 공식 `easyDSP_Boot_Sync()` 기반 다운로드 경로의 제약이다.

| 항목 | 공식 F2838x 안내 |
| --- | --- |
| 부팅 지원 자원 | IPC_FLAG0/5/6/30/31, CPU1→CPU2 MSGRAM1, CPU1→CM MSGRAM1 |
| CPU2 RAM | `0x400–0x7F7`은 initialized section으로 사용 불가 |
| CM RAM | `0x20000800–0x20000FFF`는 initialized section으로 사용 불가 |
| 디버거로 로드 | easyDSP의 멀티코어 boot/sync 함수를 동작시키지 않도록 구분 |
| 공유 RAM | v11 이후 소스에서는 CPU2가 사용할 공유 메모리를 호출 전에 CPU1이 할당 |

공식 예제의 FLASH 부팅 지령과 RAM 부팅 종료 동기화는 다르게 동작한다. 이 제약을 검토하지 않은 상태에서 기존 linker·IPC 배치나 boot flow를 바꾸지 않는다.

출처: [멀티코어 주의 사항](https://www.easydsp.com/Web%20Help/topics/C2000/28x_MultiCoreCautions.htm).

## 이 저장소의 현재 연결

| 항목 | 현재 소스에서 확인한 구성 |
| --- | --- |
| CPU1 통신 파일 | `CPU1/source_editable/easy28x_driverlib_v12.2.c/h` |
| MCU 선택 | CPU1 헤더의 `F2838xD_CPU1_CPU2 = 1` |
| baud rate | CPU1 헤더의 `BAUDRATE = 115200L` |
| CPU1 초기화 | `main_cpu1.c`에서 인터럽트 모듈·벡터 초기화 후 `easyDSP_SCI_Init()` 호출 |
| 초기화 위치 | CPU1→CM/CPU2 handshake 호출 전 |
| 핀 충돌 처리 | 구 SCIA 핀 GPIO34/49를 일반 GPIO로 되돌린 뒤 GPIO28/29 적용 |
| CPU2 관측 | CPU2가 `g_cpu2_monitor`에 상태를 미러링하고 CPU1 easyDSP로 읽는 구성 |
| CPU2 직접 커널 | 현재 `main_cpu2.c`에는 easyDSP 초기화 호출 없음 |
| boot 소유 | 현재 intercore 함수가 handshake와 boot를 처리. main에 `easyDSP_Boot_Sync()` 호출 없음 |

근거: [CPU1 main](../../CPU1/source_editable/main_cpu1.c), [CPU1 통신 헤더](../../CPU1/source_editable/easy28x_driverlib_v12.2.h), [CPU2 main](../../CPU2/source_editable/main_cpu2.c).

`EZD_PRESET/harry`의 `.ezd/.cfg/.vars`는 개인 실행 환경 파일이다. 이 문서 작업 중 최종 확인에서는 Flash wrapper 파일만 남아 있었으므로 프리셋 내용을 현재 기준으로 고정하지 않는다. 실제 easyDSP에서 연 프로젝트의 MCU·출력 경로·baud rate를 확인한다.

## Watch에서 enable 조작

| 변수 | 역할 | 초기값 |
| --- | --- | --- |
| `g_board_io_enable_request` | 보드 출력 버퍼 활성화 요청 | 0 |
| `g_board_io_outputs_enabled` | 소프트웨어가 적용한 버퍼 상태 | 0 |
| `g_pwm_config.enable` | EPWM1~4 출력 활성화 | 0 |
| `g_pwm_config.channel_sel` | 출력할 레그 선택 bitmask | 15, 두 브릿지 |
| `g_pwm_config.freq_hz` | 초기화 시 적용할 주파수 | 360000 Hz |
| `g_pwm_status.off_reason` | 현재 출력 차단 이유 | 초기 구조체 값 0x11 |
| `g_pwm_status.fault_latched` | 과거 fault 이유 누적 | 0 |

보드 출력에는 버퍼 활성화와 PWM 활성화가 함께 필요하다. `channel_sel`과 fault 조건도 통과해야 한다. `freq_hz`는 Watch에 값을 써도 현재 runtime service에서 주파수를 다시 설정하지 않는다. duty, phase, dead-time, enable, channel selection은 service에서 적용한다.

무전력 scope 시험 순서와 성공 조건은 [005 검증 절차](../specs/005-fullbridge-adc-dac/verification.md)를 따른다. Command 창에서 조회하려면 다음을 한 줄씩 실행할 수 있다.

```text
g_board_io_enable_request =
g_board_io_outputs_enabled =
g_pwm_config.enable =
g_pwm_status.off_reason =
g_pwm_status.fault_latched =
```

현재 `pwm_config_service()`는 100 ms delay chunk마다 호출된다. 반면 버퍼 요청을 실제 적용하는 비교·호출은 바깥 main loop에 있고 이 루프에는 1초 serviced delay가 있다. 따라서 버퍼 요청과 PWM enable의 반응 시간을 같다고 가정하지 않는다. Watch 화면의 갱신 주기 또한 별개다.

근거: [main_cpu1.c](../../CPU1/source_editable/main_cpu1.c), [pwm.cpp](../../CPU1/source_editable/pwm.cpp), [pwm.h](../../CPU1/include_editable/pwm.h).

## 출력 차단 이유

| bit | 이름 | 의미 |
| --- | --- | --- |
| 0x01 | `PWM_OFF_DISABLED` | PWM enable이 0 |
| 0x02 | `PWM_OFF_NO_CHANNEL` | 선택된 레그 없음 |
| 0x04 | `PWM_OFF_SYSTEM_FAULT` | CPU1 시스템 fault |
| 0x08 | `PWM_OFF_CPU2_FAULT` | CPU2가 fault 보고 |
| 0x10 | `PWM_OFF_BUFFER` | 보드 버퍼 꺼짐 |

`off_reason`은 bitmask이므로 여러 이유가 합쳐진다. `fault_latched`를 0으로 쓰는 것은 기록을 지우는 동작이며 현재 fault 원인을 제거하는 동작과 다르다. `leg_switching[]`과 `output_at_board`는 trip flag와 버퍼 상태에서 계산한 소프트웨어 표시다. 실제 커넥터 파형은 scope로 별도 확인한다.
