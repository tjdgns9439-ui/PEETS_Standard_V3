# GPIO 및 통신 Bring-Up 테스트 계획표

작성일: 2026-05-26
대상: PEETS_Standard_V3, F28388D CPU1 우선 bring-up

## 목적

이 문서는 현재 보드에서 이미 확인한 항목과 앞으로 확인해야 할 GPIO, 버퍼, ADC/DAC, 통신 인터페이스 테스트를 정리하기 위한 계획표다.

기본 원칙은 "테스트 하나당 성공 조건 하나"다. 검증되지 않은 기능 여러 개를 한 번에 묶어서 확인하지 않는다.

## 현재 확인 완료 항목

| 항목             | 상태    | 근거 / 성공 조건                                                                        |
| -------------- | ----- | --------------------------------------------------------------------------------- |
| CPU1 heartbeat | 완료    | GPIO31 / LD1이 예상 주기로 토글됨.                                                         |
| ePWM           | 부분 완료 | MCU 전단 및 SN74LVCH16T245 버퍼 후단에서 ePWM 확인. VCCB / IN_5V0 공급 시 버퍼 후단은 약 5 V 레벨로 출력됨. |
| DACA           | 완료    | DACA 출력에서 약 0 V ~ 3.3 V 톱니파 확인.                                                   |
| ADCA ADCIN0    | 완료    | ADC 결과가 DACA ramp를 따라감. 관찰된 min/max는 약 11 / 4087.                                 |
| SCIA TX        | 완료    | GPIO34 / SCIA_TX에서 115200 baud로 0x55 송신 확인. bit 폭은 약 8.68 us.                     |

## 남은 작업 요약

| 우선순위 | 항목               | 목표                                             |
| ---- | ---------------- | ---------------------------------------------- |
| 1    | SCIA RX loopback | CPU1이 SCIA로 수신까지 가능한지 확인.                      |
| 2    | GPIO 기본 테스트      | 안전한 GPIO 핀들이 high/low 출력 및 input read가 되는지 확인. |
| 3    | 버퍼 양방향 테스트       | SN74LVCH16T245의 OE/DIR 및 양방향 전달 확인.            |
| 4    | SCIB / CM UART   | SCIA 다음 통신 경로 확인.                              |
| 5    | CAN / MCAN       | transceiver enable, TXD/RXD, CAN bus 파형 확인.    |
| 6    | I2C / SPI        | pull-up, clock, CS, ACK, loopback 확인.          |
| 7    | Ethernet / USB   | protocol 이전에 PHY/USB 물리 bring-up 확인.           |
| 8    | CPU2 / CM IPC    | CPU1이 CPU2/CM release 및 handshake 가능한지 확인.     |

## 1단계: SCIA RX Loopback

### 준비

- SCIA 설정은 115200 baud, 8-N-1 유지.
- GPIO34를 SCIA_TX로 사용.
- GPIO49를 SCIA_RX로 사용.
- 보드 경로상 가능하면 GPIO34와 GPIO49를 짧은 점퍼로 연결.

### 펌웨어 목표

- `0x55`를 송신한다.
- 같은 값을 수신한다.
- CCS Watch에서 아래 변수를 확인한다.
  - `g_comm_test_tx_count`
  - `g_comm_test_rx_count`
  - `g_comm_test_rx_latest`
  - `g_comm_test_error_count`

### 성공 조건

| 신호 / 변수 | 기대값 |
| --- | --- |
| SCIA_TX | 반복되는 `0x55` 파형. |
| SCIA_RX | loopback 점퍼 연결 시 TX와 같은 파형. |
| `g_comm_test_rx_count` | 계속 증가. |
| `g_comm_test_rx_latest` | `0x55`. |
| `g_comm_test_error_count` | 0 유지. |

## 2단계: GPIO 기본 테스트

모든 GPIO를 한 번에 토글하지 않는다. 먼저 핀을 분류한다.

| 그룹 | 의미 | 테스트 방식 |
| --- | --- | --- |
| A | 바로 테스트 가능한 GPIO | output high/low 및 input readback 확인. |
| B | 외부 회로와 공유된 GPIO | 먼저 input으로 상태 확인, 회로도 확인 후 output 테스트. |
| C | 직접 토글 금지 | JTAG, reset, clock, ADC, DAC, PHY, transceiver output, power enable. |

### 우선 후보 핀

최종 테스트 전에는 반드시 현재 pinmux와 회로도를 다시 확인한다.

| 분류 | 후보 GPIO |
| --- | --- |
| 버퍼 OE | 42, 43, 46, 50, 100 |
| 버퍼 DIR | 58, 59, 120 |
| GPIO mode 후보 | 24, 25, 26, 27, 28, 29, 30 |
| GPIO mode 후보 | 35, 37, 38, 39, 40 |
| GPIO mode 후보 | 51, 52, 53, 54, 55, 56, 57 |
| GPIO mode 후보 | 68, 69, 70, 72, 74, 76, 77, 78, 79, 80, 81, 82, 83 |
| GPIO mode 후보 | 87, 88, 89, 90, 91, 92, 93, 94, 95 |
| GPIO mode 후보 | 96, 97, 98, 99, 100, 101, 102, 103, 104, 121, 133 |

### 성공 조건

| 테스트 | 기대값 |
| --- | --- |
| Output low | 약 0 V. |
| Output high | MCU 전단 기준 약 3.3 V. |
| 버퍼 후단 output high | 버퍼 VCCB 기준 전압. 현재 보드에서는 보통 약 5 V. |
| Input low | register read 값 0. |
| Input high | register read 값 1. |

### 기록표

| GPIO | Net 이름 | 방향 | 기대값 | 측정값 | 결과 | 비고 |
| --- | --- | --- | --- | --- | --- | --- |
|  |  | Output low | 0 V |  |  |  |
|  |  | Output high | 3.3 V |  |  |  |
|  |  | Input low | 0 |  |  |  |
|  |  | Input high | 1 |  |  |  |

## 3단계: SN74LVCH16T245 버퍼 테스트

### 중요 동작

SN74LVCH16T245는 dual-supply 양방향 bus transceiver다.

| 핀 / 개념 | 의미 |
| --- | --- |
| VCCA | A side logic 전원. 현재 보드에서는 3.3 V. |
| VCCB | B side logic 전원. 현재 보드에서는 IN_5V0. |
| `/OE` | active-low output enable. |
| DIR | 방향 선택. 일반적으로 DIR = 1이면 A to B, DIR = 0이면 B to A. 실제 회로 기준으로 다시 확인 필요. |

### 테스트

| 테스트 | 설정 | 기대값 |
| --- | --- | --- |
| OE inactive | `/OE = 1` | 출력 side가 high impedance. |
| MCU to Board | VCCA = 3.3 V, VCCB = 5 V, `/OE = 0`, DIR = MCU to Board | MCU 3.3 V 신호가 B side에서 약 5 V로 출력됨. |
| Board to MCU | VCCA = 3.3 V, VCCB = 5 V, `/OE = 0`, DIR = Board to MCU | 5 V side 입력이 MCU side에서 약 3.3 V로 출력됨. |

### 주의

- 5 V 버퍼 후단을 MCU GPIO에 직접 되돌려 연결하지 않는다.
- VCCB / IN_5V0이 공급되지 않은 상태에서는 B side 결과를 신뢰하지 않는다.

## 4단계: UART 경로

| 인터페이스 | 핀 / 신호 | 첫 테스트 | 성공 조건 |
| --- | --- | --- | --- |
| SCIA | GPIO34 TX, GPIO49 RX | TX-to-RX loopback | RX가 `0x55`를 수신. |
| SCIB | GPIO86 TX, GPIO71 RX | TX 파형 확인 후 loopback | SCIA와 동일. |
| CM UART | 보드 CM UART 핀 | TX 파형 확인 후 PC terminal | PC에서 지정 문자열 수신. |

## 5단계: CAN / MCAN

### Bring-Up 순서

1. Transceiver 전원 확인.
2. STB / EN 상태 확인.
3. 종단저항 확인.
4. MCU TXD 신호 확인.
5. Transceiver RXD 응답 확인.
6. CANH / CANL dominant, recessive 레벨 확인.
7. internal 또는 external loopback 실행.

### 성공 조건

| 항목 | 기대값 |
| --- | --- |
| Recessive bus | CANH/CANL이 공통모드 근처. |
| Dominant bus | CANH 상승, CANL 하강. |
| TXD/RXD | RXD가 bus activity를 따라감. |
| Loopback | 송신 frame이 수신되고 error counter가 증가하지 않음. |

## 6단계: I2C 및 SPI

### I2C

| 테스트 | 기대값 |
| --- | --- |
| SCL/SDA idle | pull-up에 의해 둘 다 high. |
| Start/stop | SCL high 중 SDA transition 정상. |
| Address scan | 예상 slave가 ACK. |

### SPI

| 테스트 | 기대값 |
| --- | --- |
| CLK | 설정한 주파수와 polarity. |
| MOSI | 알려진 byte pattern. |
| CS | transfer 동안만 active. |
| MISO loopback | 수신 byte가 송신 byte와 동일. |

## 7단계: Ethernet 및 USB

### Ethernet

| 테스트 | 기대값 |
| --- | --- |
| PHY 전원 | 필요한 rail 모두 정상. |
| PHY reset | 전원 안정 후 reset release. |
| Reference clock | PHY 기준 clock 존재. |
| MDIO/MDC | PHY register read 성공. |
| Link | Link LED 또는 PHY status가 link를 표시. |

### USB

| 테스트 | 기대값 |
| --- | --- |
| VBUS | 케이블 연결 시 VBUS 존재. |
| D+/D- | enumeration 중 정상 activity. |
| PC enumeration | PC OS에서 장치 인식. |

## 8단계: CPU2 및 CM

| 테스트 | 기대값 |
| --- | --- |
| CPU2 release | CPU2 heartbeat 토글. |
| CPU1 to CPU2 IPC | flag 또는 MSGRAM 값 왕복. |
| CM release | CM heartbeat 또는 UART 출력 확인. |
| CPU1 to CM IPC | flag 또는 MSGRAM 값 왕복. |

## 바로 다음 작업

1. SCIA RX loopback 코드 추가.
2. `g_comm_test_rx_latest == 0x55` 확인.
3. GPIO 단일 핀 테스트 표를 만들고 safe GPIO부터 채우기.
4. VCCB를 공급한 상태에서 버퍼 OE / DIR 테스트.
5. GPIO와 버퍼 동작이 안정된 뒤 CAN 테스트로 이동.

