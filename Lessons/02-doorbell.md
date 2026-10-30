# Lesson 02: Doorbell

## Goal
Pressing the Button module plays a "ding-dong" on the passive buzzer. The button is read with an **interrupt**, not by polling.

## What you learn
- GPIO inputs, pull-up / pull-down resistors
- External interrupts (EXTI) and the NVIC
- Contact bounce and software debouncing
- `volatile`, and why interrupt handlers must be short
- Making a tone by toggling a pin

## Parts and wiring

| Module | Module pin | Board pin |
|---|---|---|
| Button | `-` / GND | GND |
| Button | `+` / VCC | 3V |
| Button | S / OUT | PE4 |
| Passive Buzzer | `-` / GND | GND |
| Passive Buzzer | `+` / VCC | 3V |
| Passive Buzzer | S / I/O | PE5 |

Module pin labels vary. Read the silkscreen on your board.

A **passive** buzzer has no oscillator inside: it only clicks when its input changes. To get a tone you must toggle the pin hundreds of times per second. (An *active* buzzer beeps by itself when powered.)

## Step 0: Find out what the button outputs
Before writing interrupt code, learn what the module does.

1. CubeMX: **PE4** → `GPIO_Input`, label `BTN`, no pull. Generate.
2. In `App_Update()` temporarily add:
   ```c
   LOG("BTN = %d\n", HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin));
   HAL_Delay(200);
   ```
3. Run in Debug and press the button. Write down: released = ?, pressed = ?

Most of these modules have a resistor that holds the output in one state, and the button pulls it to the other. If yours reads random values when released, it has no resistor: set the pin's **Pull** to *Pull-up* in CubeMX.

Remove the test code.

## CubeMX setup
1. **PE4** → `GPIO_EXTI4`, label `BTN`. In **System Core → GPIO → PE4**:
   - GPIO mode: *External Interrupt Mode with Falling edge* if the button reads 0 when pressed, *Rising edge* if it reads 1.
   - Pull: as found in step 0.
2. **System Core → NVIC**: tick **EXTI line4 interrupt**.
3. **PE5** → `GPIO_Output`, label `BUZZER`.
4. Generate Code. Look in `stm32f4xx_it.c`: CubeMX added `EXTI4_IRQHandler()`, which calls `HAL_GPIO_EXTI_IRQHandler(BTN_Pin)`, which calls your `HAL_GPIO_EXTI_Callback()`.

The chain is: pin edge → EXTI hardware → NVIC → `EXTI4_IRQHandler` → HAL → `HAL_GPIO_EXTI_Callback`.

**EXTI line number = pin number.** PE4 uses EXTI line 4, PA0 uses line 0. Only **one port per line**: PE4 and PA4 can't both be interrupts. Lines 5–9 share one handler (`EXTI9_5_IRQHandler`) and lines 10–15 share another (`EXTI15_10_IRQHandler`). That's why the callback gets `GPIO_Pin`: to tell you which one fired.

## Steps

### Step 1: Count presses and see bouncing
In `app.c`:

```c
static volatile uint32_t pressCount;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BTN_Pin)
  {
    pressCount++;
  }
}
```

In `App_Update()` print `pressCount` when it changes. Press the button 10 times. You'll often see more than 10. A mechanical contact **bounces**: it opens and closes several times within a few milliseconds, and every bounce is an edge.

**Why `volatile`?** `pressCount` is changed by the interrupt, behind the main loop's back. Without `volatile`, the compiler may keep the value in a CPU register and never re-read memory, so the main loop never sees the change. Any variable shared between an interrupt and the main code must be `volatile`.

### Step 2: Debounce in the interrupt
Accept a press only if the previous accepted one was at least 50 ms ago:

```c
static volatile uint32_t pressCount;
static volatile uint32_t lastPressTick;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BTN_Pin)
  {
    uint32_t now = HAL_GetTick();
    if (now - lastPressTick >= 50)
    {
      lastPressTick = now;
      pressCount++;
    }
  }
}
```

Press 10 times → exactly 10.

### Step 3: The doorbell module
The interrupt only **sets a flag**. All the work happens in the main loop. That keeps the interrupt short (microseconds), so other interrupts aren't delayed.

`Core/Inc/doorbell.h`:

```c
#pragma once

void Doorbell_Init(void);
void Doorbell_Update(void);
void Doorbell_OnPress(void);
```

`Core/Src/doorbell.c`:

```c
#include "doorbell.h"
#include "main.h"

typedef enum { DB_IDLE, DB_DING, DB_DONG } DoorbellState;

static volatile uint8_t pressPending;
static volatile uint32_t lastPressTick;

static DoorbellState state = DB_IDLE;
static uint32_t stateStart;
static uint32_t lastToggle;

void Doorbell_Init(void)
{
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

void Doorbell_OnPress(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastPressTick >= 50)
  {
    lastPressTick = now;
    pressPending = 1;
  }
}

static void Tone(uint32_t halfPeriodMs)
{
  uint32_t now = HAL_GetTick();
  if (now - lastToggle >= halfPeriodMs)
  {
    lastToggle = now;
    HAL_GPIO_TogglePin(BUZZER_GPIO_Port, BUZZER_Pin);
  }
}

void Doorbell_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (pressPending && state == DB_IDLE)
  {
    pressPending = 0;
    state = DB_DING;
    stateStart = now;
  }

  switch (state)
  {
    case DB_DING:
      Tone(1);
      if (now - stateStart >= 300) { state = DB_DONG; stateStart = now; }
      break;

    case DB_DONG:
      Tone(2);
      if (now - stateStart >= 500) { state = DB_IDLE; }
      break;

    default:
      HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
      break;
  }
}
```

Toggling every 1 ms gives a period of 2 ms → **500 Hz** ("ding"). Every 2 ms → **250 Hz** ("dong").

In `app.c`, the callback becomes:

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BTN_Pin) Doorbell_OnPress();
}
```

and `App_Start()` / `App_Update()` call `Doorbell_Init()` / `Doorbell_Update()`.

## Check it works
- [ ] One press = one ding-dong, every time
- [ ] Holding the button doesn't repeat
- [ ] Traffic light from lesson 01 keeps running during the sound

## Troubleshooting
| Symptom | Cause |
|---|---|
| Callback never runs | EXTI line4 not ticked in NVIC, or wrong edge |
| Fires on release, not press | wrong edge selected |
| Buzzer silent | it's not toggling (check with LOG), or it's an *active* buzzer variant |
| Tone sounds rough | `HAL_GetTick()` only has 1 ms resolution; lesson 05 fixes this with a hardware timer |

## Challenges
1. Make the buzzer play only while the button is **held**. Hint: trigger on both edges and read the pin in the callback.
2. Enable **EXTI line0** in CubeMX and use the blue user button (PA0, `B1`) through an interrupt too. Remove the polling code from `app.c`.
3. Measure bounce: in the callback, store the tick of every edge in an array. Look at it in the debugger after one press. (Ticks are 1 ms; most bounce is shorter. That's why you see several edges at the same tick.)
