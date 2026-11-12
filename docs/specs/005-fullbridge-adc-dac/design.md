# 005 Full-Bridge PWM x2 + ADC 2CH + DAC 3CH — Design

## 목적

- 현재 보드 세트(컨트롤보드 + Inverter_Interface_v3)에서 배선이 이미 있는 기능만으로 전력단 제어의 기본 I/O를 연다.
- 범위: 풀브릿지 PWM 2개, PWM 동기 ADC 2채널, DAC 3채널.
- 범위 밖: 제어 법칙, HRPWM, 보호 체인(CMPSS/TZ/INPUTXBAR), ADC 3·4번째 채널(인터페이스보드 배선 없음).

## 하드웨어 매핑

근거: `docs/board/board-io-reference.md`

| 기능 | MCU | 보드 |
| --- | --- | --- |
| 브릿지 1 레그 A | EPWM1A/B (GPIO0/1) | PWM1A / PWM1B |
| 브릿지 1 레그 B | EPWM2A/B (GPIO2/3) | PWM2A / PWM2B |
| 브릿지 2 레그 A | EPWM3A/B (GPIO4/5) | PWM3A / PWM3B |
| 브릿지 2 레그 B | EPWM4A/B (GPIO6/7) | PWM4A / PWM4B |
| ADC ch0 (Vout) | ADCINA2 | J15.4 / J15.2 → U9 |
| ADC ch1 (IL) | ADCINB2 | J15.8 → U8 |
| DAC ch0/1/2 | DACA / DACB / DACC | DAC1 / DAC2 / DAC3 |

## PWM (`CPU1/source_editable/pwm.cpp`, `g_pwm_config`)

- 레그 1개 = EPWM 1개. EPWMxA = 상측, EPWMxB = 하측. 상보 출력 + dead-band.
- up-down(center-aligned) 카운트. EPWMxA는 CTR = 0을 중심으로 `duty * 주기` 동안 high다.
- EPWM1이 time-base master다(CTR = 0에서 sync-out). EPWM2, 3, 4는 그 sync로 TBPHS를 다시 load한다.
  - `phase_deg[i]`: EPWM1 기준 위상. + = 지연, − = 앞섬. 범위 −180 – +180.
  - sync 전달 지연 2 TBCLK를 보정한다(`PWM_SYNC_DELAY_TICKS`).
  - 위상 변경은 다음 master zero에서 적용된다.
- 주파수는 `pwm_config_init()`에서만 적용한다. duty, phase, dead-time, enable, channel_sel은 `pwm_config_service()`가 실시간 적용한다(현재 main loop에서 약 100 ms마다).
- 기본값: enable = 0, 360 kHz, dead-time 100 ns, 모든 레그 duty 0.5, phase = {0, 180, 0, 180}.
- 사용 예
  - phase-shift 풀브릿지 / DAB: duty는 0.5로 두고 `phase_deg[1]`(브릿지 1 내부), `phase_deg[2]`(브릿지 간), `phase_deg[3]`을 제어한다.
  - unipolar PWM(인버터): phase는 0으로 두고 `duty[레그B] = 1 − duty[레그A]`.
- 분해능: TBCLK 100 MHz. 360 kHz에서 TBPRD = 139라 duty는 약 0.7 %, phase는 약 1.3° 단위다.

### 출력 게이팅

- 레그 출력은 trip-zone one-shot으로 막는다(양쪽 low).
- 레그가 켜지는 조건: `g_pwm_config.enable = 1` AND `channel_sel`에 포함 AND safety_ok(시스템 fault 없음, CPU2 fault 없음).
- 보드 버퍼(`g_board_io_enable_request`)는 별도 게이트다. 둘 다 켜야 전력단에 신호가 나간다.
- 버퍼를 켤 때 EPWM trip을 일괄 해제하지 않는다. trip 해제는 `pwm_config_service()`만 한다.

### 상태 표시 (`g_pwm_status`)

- "PWM이 왜 안 나오는가"를 easyDSP에서 바로 볼 수 있게 한다. `pwm_config_service()`가 호출될 때마다(약 100 ms) 갱신한다.

| 필드 | 의미 |
| --- | --- |
| `leg_switching[0..3]` | 1 = 해당 레그가 MCU 핀에서 스위칭 중. trip 플래그를 하드웨어에서 되읽은 값 |
| `output_at_board` | 1 = 스위칭 중인 레그가 있고 버퍼도 켜져 있음 (전력단에 신호가 나감) |
| `off_reason` | 지금 PWM을 막고 있는 이유 bitmask. 0이면 막는 것 없음 |
| `fault_latched` | 발생했던 fault 이유. fault가 사라져도 남는다. easyDSP에서 0을 쓰면 지워진다 |

| `off_reason` bit | 값 | 의미 |
| --- | --- | --- |
| `PWM_OFF_DISABLED` | 0x01 | `g_pwm_config.enable = 0` |
| `PWM_OFF_NO_CHANNEL` | 0x02 | `channel_sel`에 선택된 레그 없음 |
| `PWM_OFF_SYSTEM_FAULT` | 0x04 | CPU1 시스템 fault (CPU2 무응답, fault inject) |
| `PWM_OFF_CPU2_FAULT` | 0x08 | CPU2가 fault 보고 (e-stop 등) |
| `PWM_OFF_BUFFER` | 0x10 | 보드 버퍼 꺼짐 (`g_board_io_enable_request = 0`) |

- 부팅 직후 값은 0x11(enable = 0, 버퍼 꺼짐)이다.
- fault bit(0x04, 0x08)만 레그를 trip으로 끈다. `PWM_OFF_BUFFER`만 있으면 MCU 핀은 스위칭하지만 보드에는 나가지 않는다.

## ADC / DAC (`CPU1/source_editable/analog.c`, `g_analog`)

- ADCA SOC0 = ADCINA2, ADCB SOC0 = ADCINB2. 둘 다 EPWM1 SOCA로 트리거해 같은 순간에 샘플링한다.
- 트리거 시점은 EPWM1 CTR = 0(EPWM1A on 구간의 중앙)이다.
- ADCA INT1(변환 종료)이 sample ISR `analog_adca1_isr`를 실행한다. 이후 제어 법칙을 넣을 자리다.
- ISR 주파수는 `ANALOG_MAX_SAMPLE_RATE_HZ`(50 kHz) 이하가 되도록 SOCA를 1 – 15 주기마다 한 번으로 나눈다. 360 kHz에서는 8주기마다 한 번, 45 kHz다. 실제 값은 `g_analog.sample_rate_hz`에 나온다.
- `adc_volt[]`는 MCU ADC 핀 전압이다. 센서 스케일(이득, 오프셋)은 아직 적용하지 않는다.
- `vref_v` 기본값은 3.3이다. 보드 VREF 스위치가 REF5030(3.0 V)이면 3.0으로 바꾼다.
- DAC: `dac_mode[ch]`로 출력 내용을 고른다.
  - 0 = 수동(`dac_code[ch]`), 1 = ADC ch0(Vout) raw 미러, 2 = ADC ch1(IL) raw 미러
  - 기본값: DAC1 = Vout 미러, DAC2 = IL 미러, DAC3 = 수동 0
  - DAC 값은 sample ISR에서 갱신한다.

## 변경으로 꺼진 기능

- `CPU1_ADCMON_ENABLE`(ADCINA0 폴링)은 0으로 바꿨다. ADCINA0이 DACA 출력 핀이고, ADCA SOC0/INT1을 같이 쓰기 때문이다.

## 알려진 한계

- ADC 입력단 U8/U9는 ±15 V 전원이다. ADC 핀 과전압 보호 여부는 회로에서 확인이 필요하다.
- RAM 빌드에서 RAMLS5(.bss/.data/.const) 여유가 14 word다. 전역 변수를 더 늘리면 링커 배치를 조정해야 한다.
- FLASH 빌드에서 ISR을 RAM에서 실행하도록 옮기는 작업은 하지 않았다.
