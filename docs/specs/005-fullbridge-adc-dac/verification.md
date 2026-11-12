# 005 Full-Bridge PWM x2 + ADC 2CH + DAC 3CH — Verification

## 상태

- 빌드: 완료 (2026-11-12). `cl2000 25.11.0.LTS`로 CPU1 전체 소스 컴파일 + `2838x_RAM_lnk_cpu1.cmd` 링크 성공.
- 보드 검증: **PENDING**. 아래 항목은 아직 실제 보드에서 확인하지 않았다.

## 사전 조건

- 전력단 전원은 넣지 않는다(무전력 scope 검증).
- easyDSP 연결(CPU1 SCI-A).

## 체크리스트

### PWM

| # | 조작 (easyDSP) | 관측 | 성공 조건 | 결과 |
| --- | --- | --- | --- | --- |
| 1 | 부팅 직후 | PWM1A – PWM4B | 모두 low | PENDING |
| 2 | `g_board_io_enable_request = 1` (enable은 0 유지) | PWM1A – PWM4B | 모두 low 유지. 펄스 없음 | PENDING |
| 3 | `g_pwm_config.enable = 1` | PWM1A, PWM1B | 360 kHz, duty 50 %, 상보, dead-time 약 100 ns | PENDING |
| 4 | 같은 상태 | PWM1A vs PWM2A | 180° 위상차 | PENDING |
| 5 | 같은 상태 | PWM1A vs PWM3A | 위상차 0 | PENDING |
| 6 | `phase_deg[1] = 90` | PWM1A vs PWM2A | PWM2A가 90° 지연 | PENDING |
| 7 | `phase_deg[2] = -45`, `phase_deg[3] = 135` | PWM1A vs PWM3A | PWM3A가 45° 앞섬 | PENDING |
| 8 | `duty[0] = 0.3` | PWM1A | duty 30 %, 중심 위치 유지 | PENDING |
| 9 | `channel_sel = 3` | PWM3A – PWM4B | 브릿지 2만 low | PENDING |
| 10 | `g_cpu1_fault_inject = 1` | PWM1A – PWM4B | 100 ms 안에 모두 low | PENDING |

### PWM 상태 표시 (`g_pwm_status`)

| # | 조작 (easyDSP) | 관측 | 성공 조건 | 결과 |
| --- | --- | --- | --- | --- |
| S1 | 부팅 직후 | `off_reason` | 0x11 (DISABLED + BUFFER) | PENDING |
| S2 | `g_pwm_config.enable = 1` | `off_reason`, `leg_switching[]` | 0x10, 네 레그 모두 1, `output_at_board` = 0 | PENDING |
| S3 | `g_board_io_enable_request = 1` | `off_reason`, `output_at_board` | 0x00, 1 | PENDING |
| S4 | `g_cpu1_fault_inject = 1` | `off_reason`, `leg_switching[]` | 0x04 포함, 네 레그 모두 0 | PENDING |
| S5 | `g_cpu1_fault_inject = 0` | `off_reason`, `fault_latched` | 0x04 사라짐, `fault_latched`는 0x04 유지 | PENDING |
| S6 | `g_pwm_status.fault_latched = 0` | `fault_latched` | 0 | PENDING |

### ADC

| # | 조작 | 관측 | 성공 조건 | 결과 |
| --- | --- | --- | --- | --- |
| 11 | 부팅 후 | `g_analog.sample_count` | 계속 증가 | PENDING |
| 12 | 부팅 후 | `g_analog.sample_rate_hz` | 45000 (360 kHz / 8) | PENDING |
| 13 | 테스트포인트 ADC(A2)에 알려진 전압 인가 | `g_analog.adc_volt[0]` | 인가 전압과 일치 | PENDING |
| 14 | 테스트포인트 ADC(B2)에 알려진 전압 인가 | `g_analog.adc_volt[1]` | 인가 전압과 일치 | PENDING |
| 15 | 장시간 동작 | `g_analog.overflow_count` | 0 유지 | PENDING |

### DAC

| # | 조작 | 관측 | 성공 조건 | 결과 |
| --- | --- | --- | --- | --- |
| 16 | 기본 상태 | DAC1 커넥터 | ADC(A2) 전압을 따라감 | PENDING |
| 17 | 기본 상태 | DAC2 커넥터 | ADC(B2) 전압을 따라감 | PENDING |
| 18 | `dac_code[2] = 2048` | DAC3 커넥터 | 약 VREF / 2 | PENDING |
| 19 | `dac_mode[0] = 0`, `dac_code[0] = 1000` | DAC1 커넥터 | 약 VREF × 1000 / 4095 | PENDING |

### 회귀

| # | 관측 | 성공 조건 | 결과 |
| --- | --- | --- | --- |
| 20 | easyDSP 통신 | 끊김 없음 | PENDING |
| 21 | LED1 heartbeat, CM / CPU2 handshake | 기존과 동일 | PENDING |
