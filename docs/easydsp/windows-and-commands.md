# easyDSP 창과 명령어

변수를 계속 보면서 값을 바꾸려면 Watch, 명령을 한 줄씩 실행하거나 저장하려면 Command를 사용한다. 느린 추세는 Plot, MCU에 미리 저장한 샘플 배열은 Chart로 확인한다.

## 창 선택

| 창 | 용도 | 핵심 조작과 조건 |
| --- | --- | --- |
| Watch | 변수 조회·변경 | Insert 등록, Delete 삭제. Value 좌클릭 또는 Enter로 변경, 우클릭으로 진수 전환 |
| Command | 값 조회·대입과 명령 파일 | 해당 행에서 Enter로 실행. Ctrl+Enter는 실행 없이 줄 삽입 |
| Plot | PC에서 주기적으로 읽은 값의 추세 | 최소 입력 간격 15 ms, 최대 15개 창 |
| Plot2 | 더 짧은 간격의 PC 측 읽기 | 최소 입력 간격 2 ms, 1개 창. 실행 중 Command 외 창의 갱신 중지 |
| Chart | MCU에 저장한 1차원 샘플 배열 | 최대 8채널, 읽기용. MCU 쪽 샘플 저장 코드는 별도 필요 |
| Record | 저장된 `.rec` 다시 보기 | Chart/Plot 기록을 열어 그래프와 CSV로 확인 |
| Memory | 메모리 주소로 읽기·RAM 변경 | 주소나 `&변수`로 접근. FLASH 직접 변경 불가 |
| Array | 기본형 1·2차원 배열 | 셀 변경, 선택 영역 복사. 구조체 배열은 Tree 사용 |
| Tree | 구조체·배열 펼쳐 보기 | 값 좌클릭/Enter로 변경, 우클릭으로 진수 전환 |

Watch·Array·Tree·Memory는 화면에 보이는 항목 위주로 값을 갱신한다. 많은 창이나 변수를 동시에 읽으면 설정한 갱신 주기보다 실제 주기가 길어진다. Plot의 입력 주기를 실제 취득 주기로 간주하지 말고 저장된 CSV 시간 열을 확인한다.

출처: 공식 [Watch](https://www.easydsp.com/Web%20Help/topics/Window%20Watch.htm), [Plot·Plot2](https://www.easydsp.com/Web%20Help/topics/Window%20Plot.htm), [Chart](https://www.easydsp.com/Web%20Help/topics/Window%20Chart.htm), [Record](https://www.easydsp.com/Web%20Help/topics/Window%20Record.htm), [Memory](https://www.easydsp.com/Web%20Help/topics/Window%20Memory.htm), [Array](https://www.easydsp.com/Web%20Help/topics/Window%20Array.htm), [Tree](https://www.easydsp.com/Web%20Help/topics/Window%20Tree.htm).

## Command 문법

| 입력 | 동작 |
| --- | --- |
| `help` | 명령어 도움말 |
| `var =` | 현재 값 조회 |
| `&var =` | 변수 주소 조회 |
| `var = 123` | 값 대입 |
| `var = 0x10` | 16진수 값 대입 |
| `hex var`, `dec var`, `bin var` | 표시 진수 변경 |
| `var = <수식>` | 수식을 계산하여 대입 |
| `r 파일.cmd` | 파일 내용을 창에 복사. 실행하지 않음 |
| `l 파일.cmd` | 명령 파일 실행 |
| `delay(100)` | 약 100 ms 지연. 최대 인수 10000 ms |
| `skip` | 명령 파일에서 이후 실행 중단 |
| `clear` | Command 창 내용 삭제 |
| `// 주석` | 해당 행의 이후 내용을 주석 처리 |

명령어는 소문자로 입력하고 한 줄 길이는 300자를 넘기지 않는다. `bin`은 표시 형식이며 `0b...` 값을 대입하는 문법은 지원하지 않는다. Watch에서 저장한 `.cmd`를 실행하면 저장된 변수 값을 다시 쓸 수 있으므로, 이름 목록을 불러오는 기능과 구분한다.

easyDSP의 명령 파일 `.cmd`와 CCS linker command file `.cmd`는 확장자만 같다. 서로 다른 용도로 관리한다.

출처: [Command](https://www.easydsp.com/Web%20Help/topics/Window%20Command.htm).

## Dimension과 Q 형식

Dimension과 Q 형식은 Watch에서 설정한다. 실수에는 `p`, `n`, `u`, `m`, `K`, `M`, `G` 등 배율을 사용할 수 있다. 지원되는 정수형에서는 `3.14Qn`, 설정된 Q 형식을 사용하는 `3.14Q` 같은 쓰기 문법을 제공한다. 정수 raw 값과 Q 변환된 표시값을 구분한다.

출처: [Watch](https://www.easydsp.com/Web%20Help/topics/Window%20Watch.htm).

## 지원 범위

공식 제한 안내에서는 bitfield 쓰기와 `->` 연산자를 지원하지 않는다. C28x 포인터 역참조도 지원되는 형식이 제한되어 있으므로 C의 모든 표현식을 입력할 수 있다고 가정하지 않는다. 상세 문법과 그림은 통합 HTML의 해당 창 항목에서 확인한다.

출처: [제한 사항](https://www.easydsp.com/Web%20Help/topics/Limitations.html).

## 저장 데이터 활용

Plot과 Chart는 화면 이미지, CSV, `.rec`를 저장할 수 있다. Record는 `.rec`를 다시 열어 관찰하고 CSV로 내보낸다. 빠른 제어 파형은 MCU에서 배열로 취득한 뒤 Chart로 읽거나 DAC와 scope를 함께 사용한다. PC의 Plot2 설정 주기만으로 제어 ISR과 동기화된 샘플링이 되지는 않는다.

공식 DA 팁은 포인터로 출력할 변수를 선택하는 예제다. 이 저장소의 실제 DAC 제어 인터페이스는 [005 설계](../specs/005-fullbridge-adc-dac/design.md)의 `g_analog`를 따른다.

출처: [Chart](https://www.easydsp.com/Web%20Help/topics/Window%20Chart.htm), [DA 사용 팁](https://www.easydsp.com/Web%20Help/topics/Tips%20DA.htm).
