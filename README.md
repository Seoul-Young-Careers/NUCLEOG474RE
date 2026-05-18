# NUCLEOG474RE

STM32G474 NUCLEO 보드 기반 모터 제어 실험 프로젝트입니다.

현재 정리된 주요 흐름은 `PWM` 드라이버를 이용해서 DM542 스텝모터 드라이버의 `PUL` 신호를 만들고, `GPIO` 드라이버로 `DIR` 방향 신호를 제어하는 구조입니다. `ENA`는 소프트웨어 GPIO 제어 대상에서 제외하고 하드웨어 쪽에서 처리하는 방향으로 잡았습니다.

## 현재 구조

```text
Application
  |
  +-- dm542.c
        |
        +-- pwm.c     : DM542 PUL 펄스 출력
        +-- gpio.c    : DM542 DIR 방향 출력
        +-- bsp.c     : us 단위 delay
```

## 파일 구성

| 파일 | 역할 |
| --- | --- |
| `stm32g474/src/hw/driver/pwm.c` | TIM PWM 채널 초기화, 설정, 출력 제어 |
| `stm32g474/src/common/hw/include/pwm.h` | PWM 공개 API |
| `stm32g474/src/bsp/bsp.c` | `delay()`, `delayUs()` 구현 |
| `stm32g474/src/bsp/bsp.h` | BSP delay API 선언 |
| `stm32g474/src/hw/driver/dm542/dm542.c` | DM542 모터 드라이버 API 구현 |
| `stm32g474/src/common/hw/include/dm542/dm542.h` | DM542 공개 API |
| `stm32g474/src/hw/hw_def.h` | 사용 모듈 및 채널 수 설정 |

## PWM 드라이버

PWM은 채널 기반으로 동작합니다. GPIO 관련 설정은 PWM 테이블에서 분리했고, 타이머 핸들러와 TIM 채널 중심으로 구성했습니다.

주요 API:

```c
bool pwmInit(void);
bool pwmOpen(uint8_t ch);
bool pwmIsOpen(uint8_t ch);
bool pwmIsBusy(uint8_t ch);

bool pwmStart(uint8_t ch);
bool pwmStop(uint8_t ch);
bool pwmRunUs(uint8_t ch, uint32_t time_us);

bool pwmSetPrescaler(uint8_t ch, uint32_t prescaler);
bool pwmSetPeriod(uint8_t ch, uint32_t period);
bool pwmSetPulse(uint8_t ch, uint32_t pulse);
```

`pwmRunUs()`는 PWM을 켠 뒤 지정한 us 시간만큼 기다리고 다시 끄는 함수입니다.

```c
pwmRunUs(_DEF_PWM2, 10);
```

위 코드는 `_DEF_PWM2` 채널 PWM을 약 10us 동안 출력한 뒤 정지합니다.

## BSP delayUs

us 단위 지연은 PWM 내부에 두지 않고 BSP 공통 함수로 분리했습니다.

```c
void delayUs(uint32_t us);
```

현재 `delayUs()`는 DWT cycle counter를 사용합니다. PWM의 `pwmRunUs()`와 DM542의 step pulse 출력에서 사용합니다.

## DM542 드라이버

DM542는 현재 다음 신호 기준으로 구성했습니다.

| DM542 신호 | 처리 방식 |
| --- | --- |
| `PUL` | PWM 출력 |
| `DIR` | GPIO 출력 |
| `ENA` | 하드웨어 처리, 소프트웨어 GPIO 제어 없음 |

현재 매크로:

```c
#define DM542_MAX_CH    HW_DM542_MAX
#define DM542_PUL       _DEF_PWM2
#define DM542_DIR       0
```

`DM542_PUL`은 PWM 채널을 의미하고, `DM542_DIR`은 GPIO 채널을 의미합니다.

## DM542 API

초기화 및 상태 확인:

```c
bool dm542Init(void);
bool dm542Open(uint8_t ch);

bool dm542IsOpen(uint8_t ch);
bool dm542IsBusy(uint8_t ch);
bool dm542IsEnabled(uint8_t ch);
```

Enable 상태 제어:

```c
bool dm542Enable(uint8_t ch);
bool dm542Disable(uint8_t ch);
```

현재 `enable/disable`은 외부 ENA 핀을 직접 제어하지 않고, DM542 API 내부에서 동작 허용 여부를 판단하는 소프트웨어 상태값입니다.

PWM 및 step 제어:

```c
bool dm542Start(uint8_t ch);
bool dm542Stop(uint8_t ch);
bool dm542Step(uint8_t ch);

bool dm542SetPrescaler(uint8_t ch, uint32_t prescaler);
bool dm542SetPeriod(uint8_t ch, uint32_t period);
bool dm542SetPulse(uint8_t ch, uint32_t pulse);
bool dm542SetFreq(uint8_t ch, uint32_t freq_hz);
```

이동 제어:

```c
bool dm542MoveStep(uint8_t ch, int32_t step, uint32_t pulse_delay_us);
bool dm542MoveMm(uint8_t ch, float mm, uint32_t pulse_delay_us);
```

## step 이동 원리

DM542는 `PUL` 펄스 1개를 받을 때마다 모터를 1 step 이동시킵니다.

```c
dm542MoveStep(0, 1000, 10);
```

위 코드는 0번 DM542 채널을 정방향으로 1000 step 이동시킵니다. 각 step마다 PWM pulse를 10us 동안 출력합니다.

`step` 값의 부호는 방향을 의미합니다.

| step 값 | 동작 |
| --- | --- |
| 양수 | 정방향 |
| 음수 | 역방향 |
| 0 | 이동 없음 |

## mm 이동 원리

`dm542MoveMm()`는 mm 단위를 step 수로 변환한 뒤 `dm542MoveStep()`을 호출하는 래퍼 함수입니다.

```c
step = mm * DM542_STEP_PER_MM;
dm542MoveStep(ch, step, pulse_delay_us);
```

현재 기본값:

```c
#define DM542_STEP_PER_MM  1.0f
```

실제 장비에서는 모터 step angle, DM542 마이크로스텝 설정, 볼스크류 리드 또는 벨트/풀리 조건에 맞춰 `DM542_STEP_PER_MM` 값을 정해야 합니다.

예시:

```text
1.8도 모터       = 200 step/rev
DM542 16분주     = 200 * 16 = 3200 step/rev
리드스크류 5mm   = 3200 / 5 = 640 step/mm
```

이 경우:

```c
#define DM542_STEP_PER_MM  640.0f
```

## 사용 예시

```c
dm542Init();
dm542Open(0);

dm542SetFreq(0, 1000);
dm542Enable(0);

dm542MoveStep(0, 1000, 10);
dm542MoveStep(0, -1000, 10);
```

`dm542SetFreq()`는 PWM 기준 주파수 설정용이고, `dm542MoveStep()`의 `pulse_delay_us`는 한 step pulse를 켜두는 시간입니다.

## 현재 주의점

- CLI는 DM542 쪽에 추가하지 않았습니다.
- `ENA` 핀 제어는 코드에서 하지 않습니다.
- `dm542Enable()`은 실제 핀 출력이 아니라 내부 허용 상태 플래그입니다.
- `DM542_STEP_PER_MM` 기본값은 임시값입니다. 실제 기구 조건에 맞게 설정해야 합니다.
- `DIR` 출력은 `gpioPinWrite(DM542_DIR, dir)`로 처리합니다.

