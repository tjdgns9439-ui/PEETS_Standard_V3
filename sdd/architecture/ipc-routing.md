# IPC Routing

이 문서는 초기 bring-up에서 사용하는 MSGRAM 기반 최소 IPC 경로를 정리한다.

## Boundary

- v1 IPC는 단순 token 왕복과 상태 latch만 다룬다.
- queue, protocol framing, interrupt-driven IPC, telemetry stream은 후속 spec 대상이다.
- MSGRAM section 이름과 linker 배치는 각 프로젝트 `.cmd` 파일이 기준이다.

## Current Routes

| Route | CPU1 section | Remote section | 목적 |
| --- | --- | --- | --- |
| CPU1 -> CM | `MSGRAM_CPU_TO_CM` | `MSGRAM_CPU1_TO_CM` | CPU1 boot-ready token |
| CM -> CPU1 | `MSGRAM_CM_TO_CPU` | `MSGRAM_CM_TO_CPU1` | CM ACK token |
| CPU1 -> CPU2 | `MSGRAM_CPU1_TO_CPU2` | `MSGRAM_CPU1_TO_CPU2` | CPU2 boot-ready token |
| CPU2 -> CPU1 | `MSGRAM_CPU2_TO_CPU1` | `MSGRAM_CPU2_TO_CPU1` | CPU2 ACK token + liveness heartbeat |

## Mailbox 주소 정렬

- 각 방향 mailbox는 양쪽 코어가 **같은 section offset(0)** 을 보도록 둔다.
- raw mailbox polling만 사용하므로 TI IPC driverlib buffer pad를 두지 않는다.
  (한쪽만 pad를 두면 read/write 주소가 어긋나 handshake와 heartbeat가 깨진다.)
- CPU1/CPU2/CM 모든 mailbox는 plain `uint32_t` 한 워드이며 section 시작에 배치한다.

## Flow

- CPU1은 shared mailbox를 0으로 초기화한 뒤 remote core를 release한다.
- CPU1은 agreed boot-ready token을 write한다.
- remote core는 token을 관측한 뒤 ACK token을 write한다.
- CPU1은 timeout 안에 ACK를 관측하면 OK latch를 남긴다.
- timeout이면 실패 latch를 남기고 후속 기능 enable을 막는다.

## Liveness Heartbeat & Recovery

CM과 CPU2 모두 동일한 패턴을 쓴다.

- handshake OK 이후 remote core는 자신의 `*_TO_CPU1` mailbox를 정적 ACK token 대신
  단조 증가하는 heartbeat 값으로 재사용한다.
  - CM: `comm_cm_liveness_service` (main loop 끝에서 호출)
  - CPU2: `intercore_cpu2_service` 내부 (handshake 완료 후 분기)
- CPU1 supervisor는 이 값의 변화 유무로 remote 생존을 판단한다.
  - CM: `cpu1_service_cm_liveness_supervisor` → stale 시 `SysCtl_controlCMReset` + 재핸드셰이크
  - CPU2: `cpu1_service_cpu2_liveness_supervisor` → stale 시 `SysCtl_controlCPU2Reset` + 재핸드셰이크
  - 값이 `*_STALE_LIMIT`회 연속 멈추고 handshake가 OK 상태였을 때만 recovery한다.
- remote core는 첫 heartbeat write를 ACK write보다 한 service 주기 뒤로 미뤄,
  CPU1 handshake polling이 ACK token을 latch하기 전에 덮어쓰는 race를 피한다.
- recovery 경로에서 CPU1은 mailbox를 0으로 초기화한 뒤 재핸드셰이크하므로, remote의
  heartbeat 카운터는 reset 후 다시 0부터 시작한다.

## Invariants

- token 값은 양쪽 core에서 동일해야 한다.
- MSGRAM section 이름은 source pragma와 linker command file이 일치해야 한다.
- polling loop에는 bounded timeout이 있어야 한다.
- handshake 실패는 UART, fast ISR, power-stage enable 같은 후속 기능을 차단해야 한다.

## Related Code Notes

- [CPU1 main_cpu1.c](../code/CPU1/source_editable/main_cpu1.c.md)
- [CM main_cm.cc](../code/CM/source_editable/main_cm.cc.md)
- [CPU2 main_cpu2.c](../code/CPU2/source_editable/main_cpu2.c.md)

## 변경 시 주의점

- IPC flag interrupt 방식으로 바꾸는 작업은 별도 spec으로 분리한다.
- MSGRAM section 이름을 바꾸면 CPU1, CPU2, CM linker command file과 code note를 같이 갱신한다.
- shared state는 `volatile` 또는 명시적 동기화 계약을 둔다.
