# NUCLEOG474RE

STM32G474 NUCLEO 보드 기반 자동화 장비 제어 펌웨어입니다. RTOS task를 이용해 버튼 입력, SN04 limit 센서, DM542 스텝모터, 서보모터, 펌프, 밸브를 분리해서 제어합니다.

현재 소프트웨어 구조의 핵심은 다음과 같습니다.

- 버튼 입력을 RTOS event로 변환
- 장비 동작은 `app_sequence.c`의 상태 기반 시퀀스로 처리
- DM542 스텝모터는 async chunk 단위로 pulse 출력
- SN04 센서는 limit 안전 정지와 Home/End 기준 검출에 사용
- 에러 발생 시 전체 구동부를 안전 상태로 정리하고 `ERROR` 상태로 고정

## 주요 목표

장비 동작 시간 기준은 11.33초/팩입니다. 이를 만족시키기 위해 스텝모터를 긴 blocking 이동으로 처리하지 않고, 짧은 chunk 단위의 비동기 pulse 출력으로 구성했습니다. 이동 중에도 센서 감지, STOP, RESET 요청을 계속 확인할 수 있도록 RTOS event와 ACK 구조를 사용합니다.

## 전체 구조

```text
Button Task
  -> APP_EVT_RESET_REQ / STOP_REQ / START_REQ / FOOT_PRESS

Sensor Task / SN04 ISR
  -> APP_EVT_SN04_1_DETECTED / APP_EVT_SN04_2_DETECTED
  -> DM542 sensor stop

Sequence
  -> RESET / STOP / START / FOOT 시퀀스 실행
  -> StepMotor task에 명령 전송
  -> ACK 대기 및 에러 처리

StepMotor Task
  -> DM542 async chunk 이동
  -> SN04 limit 정책 적용
  -> ACK_DONE / ACK_STOPPED / ACK_ERROR 반환
```

## 주요 파일

| 파일 | 역할 |
| --- | --- |
| `stm32g474/src/ap/app_sequence.c` | 장비 상태와 RESET/STOP/START/FOOT 시퀀스 처리 |
| `stm32g474/src/ap/app_sequence.h` | 장비 시퀀스 상태 enum 및 public API |
| `stm32g474/src/ap/task/task_button.c` | 버튼 디바운싱 및 버튼 이벤트 발생 |
| `stm32g474/src/ap/task/task_sensor.c` | SN04 센서 감지, sensor event 관리, DM542 즉시 정지 |
| `stm32g474/src/ap/task/task_stepmotor.c` | DM542 스텝모터 명령 큐, async chunk 이동, SN04 limit 처리 |
| `stm32g474/src/ap/task/task_stepmotor.h` | StepMotor task public API |
| `stm32g474/src/bsp/rtos.h` | RTOS event bit, step motor command, ACK 구조체 정의 |
| `stm32g474/src/hw/driver/stepmotor/dm542.c` | DM542 low-level pulse, direction, async 이동 구현 |
| `stm32g474/src/hw/driver/sensor/sn04.c` | SN04 센서 GPIO/EXTI 처리 |

## 버튼 동작

버튼은 10ms 주기로 읽고, 같은 상태가 3회 연속 유지되면 눌림으로 인정합니다. 즉 약 30ms 디바운싱 후 눌림 edge에서 한 번만 event가 발생합니다.

| 버튼 | GPIO | Event | 동작 조건 | 동작 |
| --- | --- | --- | --- | --- |
| RESET | PC5 | `APP_EVT_RESET_REQ` | 모든 상태 | 전체 구동부 정지 후 Calibration |
| STOP | PC4 | `APP_EVT_STOP_REQ` | START 이후 동작 상태 | 펌프/밸브 정리, 서보 Home, 스텝모터 Zero 이동 |
| START | PA10 | `APP_EVT_START_REQ` | `IDLE_HOME` 상태 | 작업 시작 시퀀스 실행 |
| FOOT SWITCH | PB3 | `APP_EVT_FOOT_PRESS` | `READY_SEQUENCE` 상태 | STOP 시퀀스 후 START 시퀀스 반복 |

## 장비 상태

```c
APP_SEQUENCE_STATE_BOOT
APP_SEQUENCE_STATE_HOMING
APP_SEQUENCE_STATE_IDLE_HOME
APP_SEQUENCE_STATE_START_ACTION
APP_SEQUENCE_STATE_MOVING_TO_END
APP_SEQUENCE_STATE_END_ACTION
APP_SEQUENCE_STATE_READY_SEQUENCE
APP_SEQUENCE_STATE_RUNNING_SEQUENCE
APP_SEQUENCE_STATE_MOVING_TO_HOME
APP_SEQUENCE_STATE_ERROR
```

기본 흐름은 다음과 같습니다.

```text
BOOT
  -> RESET/Calibration
  -> IDLE_HOME
  -> START
  -> START_ACTION
  -> MOVING_TO_END
  -> END_ACTION
  -> READY_SEQUENCE
  -> FOOT 입력 시 RUNNING_SEQUENCE
```

## 시퀀스 동작

### RESET

RESET은 장비 위치 기준을 다시 잡는 복구 동작입니다.

```text
1. 상태를 HOMING으로 변경
2. 모든 구동부 안전 상태로 정리
3. DM542 Calibration 실행
   - End 방향으로 이동해 SN04_2 검출
   - Home 방향으로 이동해 SN04_1 검출
   - Home 위치를 0 step 기준으로 설정
4. 성공 시 IDLE_HOME
5. 실패 시 ERROR
```

### START

START는 `IDLE_HOME` 상태에서만 실행됩니다.

```text
1. Pump ON
2. Valve 1 OPEN
3. Valve 2 OPEN
4. Servo 20도
5. Servo 180도
6. DM542 MoveToFull 실행
   - End 방향으로 고정 pulse 이동
7. Servo 60도
8. 300ms 대기
9. Servo 170도
10. READY_SEQUENCE 진입
```

### STOP

STOP은 START 이후 동작 상태에서만 실행됩니다.

```text
1. Valve 1 CLOSE
2. Valve 2 CLOSE
3. Pump OFF
4. Servo 180도
5. DM542 MoveToZero 실행
   - Home/Start 방향으로 고정 pulse 이동
6. 성공 시 IDLE_HOME
7. 실패 시 ERROR
```

### FOOT SWITCH

FOOT SWITCH는 START 완료 후 `READY_SEQUENCE` 상태에서만 동작합니다.

```text
1. RUNNING_SEQUENCE 진입
2. STOP 시퀀스 실행
3. START 시퀀스 실행
4. 성공 시 READY_SEQUENCE 복귀
```

## DM542 스텝모터 제어

StepMotor task는 명령 큐와 ACK 큐를 사용합니다.

### 명령

```c
RTOS_STEP_MOTOR_NONE
RTOS_STEP_MOTOR_MOVE_TO_ZERO
RTOS_STEP_MOTOR_MOVE_TO_FULL
RTOS_STEP_MOTOR_MOVE_TO_HOME
RTOS_STEP_MOTOR_MOVE_TO_END
RTOS_STEP_MOTOR_CALIBRATION
RTOS_STEP_MOTOR_STOP
```

### ACK

```c
RTOS_STEP_MOTOR_ACK_DONE
RTOS_STEP_MOTOR_ACK_STOPPED
RTOS_STEP_MOTOR_ACK_ERROR
```

`threadStepMotor()`는 한 번에 긴 이동을 blocking으로 처리하지 않습니다. 남은 step을 작은 chunk로 나누고, 각 chunk를 `dm542MoveStepAsync()`로 실행합니다.

```text
새 명령 확인
  -> 기존 명령이 있으면 정지 후 ACK_STOPPED
  -> 새 명령 상태 초기화
  -> 명령별 이동 조건 설정

센서 확인
  -> 목표 센서 감지 시 DONE
  -> 목표가 아닌 센서 감지 시 ERROR
  -> limit 방향으로 이미 눌린 센서가 있으면 ERROR

이동 처리
  -> async chunk 실행 중이면 완료 event 대기
  -> 완료되면 remain step 갱신
  -> 다음 chunk 계산 후 async 실행
```

## 스텝모터 속도/Chunk 기준

현재 유지하는 주파수와 chunk 값은 다음과 같습니다.

| 구분 | 값 |
| --- | --- |
| Fast frequency | 25600 Hz |
| Mid frequency | 16000 Hz |
| Slow frequency | 6400 Hz |
| Calibration frequency | 12800 Hz |
| Fast chunk | 50 step |
| Slow/Mid chunk | 10 step |
| Calibration chunk | 1 step |

가속/감속은 연속 ramp가 아니라 단계식 profile입니다.

```text
시작부: Slow -> Mid
중간부: Fast
도착부: Mid -> Slow
```

Calibration은 센서를 정확히 잡기 위해 profile을 쓰지 않고 12800Hz, 1 step chunk로 이동합니다.

## SN04 센서 정책

센서 매핑은 다음과 같습니다.

| 센서 | 위치 | Event | 이동 제한 |
| --- | --- | --- | --- |
| SN04_1 | Home/Start | `APP_EVT_SN04_1_DETECTED` | Home 방향 추가 이동 차단 |
| SN04_2 | End | `APP_EVT_SN04_2_DETECTED` | End 방향 추가 이동 차단 |

방향 기준은 다음과 같습니다.

```text
step > 0 : End 방향
step < 0 : Home/Start 방향
```

센서 위에서 반대 방향으로 빠져나가는 동작은 허용합니다.

```text
SN04_1 감지 중
  -> Home 방향 이동 차단
  -> End 방향 이동 허용

SN04_2 감지 중
  -> End 방향 이동 차단
  -> Home 방향 이동 허용
```

이를 위해 `taskStepMotorPrepareLimitMove()`가 이동 시작 전에 현재 센서 상태를 확인합니다. 이동 방향 쪽 limit 센서가 이미 눌려 있으면 `ACK_ERROR`를 반환하고, 반대편 센서에서 빠져나가는 경우에는 해당 센서를 release될 때까지 DM542 sensor stop 조건에서 제외합니다.

## 에러 처리

StepMotor task에서 `RTOS_STEP_MOTOR_ACK_ERROR`가 발생하면 상위 시퀀스는 `enterErrorState()`로 진입합니다.

```text
1. DM542 STOP
2. Pump OFF
3. DC Motor STOP
4. Servo HOME(180도)
5. Valve OFF
6. Control event clear
7. APP_SEQUENCE_STATE_ERROR 고정
```

`ERROR` 상태에서는 START, STOP, FOOT 동작이 실행되지 않습니다. RESET을 눌러 Calibration을 다시 수행해야 복구됩니다.

## 설계상 특징

- 긴 이동을 blocking하지 않고 async chunk로 나눠 RTOS 반응성을 확보했습니다.
- 센서 감지는 task polling과 EXTI callback을 함께 사용합니다.
- SN04 감지 시 DM542는 ISR 경로에서 먼저 정지하고, StepMotor task는 ACK와 상태를 정리합니다.
- limit 센서 위에서 빠져나가는 방향은 허용하고, limit 쪽으로 더 들어가는 방향은 차단합니다.
- Zero/Full 이동은 좌표 보정이나 백래시 보정 없이 정해진 pulse만 출력합니다.
- 에러 시 모든 구동부를 안전 상태로 정리합니다.

## 빌드 참고

프로젝트는 STM32CubeIDE 생성 구조를 따릅니다.

```text
stm32g474/
  Debug/
  src/
```

현재 작업 환경에서는 `arm-none-eabi-gcc`가 PATH에 없을 수 있습니다. 이 경우 전체 펌웨어 빌드는 STM32CubeIDE에서 수행하고, 로컬에서는 필요한 C 파일에 대해 `gcc -fsyntax-only` 수준의 문법 확인만 가능합니다.

## 주의 사항

- STOP은 비상정지 전용이라기보다 작업 중 Home 복귀 시퀀스에 가깝습니다.
- RESET은 장비 기준 위치를 다시 잡는 복구 동작입니다.
- SN04 센서 배선 또는 active level이 바뀌면 `sn04.c`와 sensor event 정책을 함께 확인해야 합니다.
- 실제 기구 조건에 따라 `STEP_MOTOR_FULL_MOVE_STEPS`, `STEP_MOTOR_MOVE_MAX_STEPS`, 서보 각도, delay 값은 조정이 필요합니다.
