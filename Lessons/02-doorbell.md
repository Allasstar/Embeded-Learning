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

1. CubeMX, **Pinout view**: left-click **PE4** → `GPIO_Input`. Then **System Core → GPIO**, select the PE4 row, set **User Label** to `BTN`, pull = *No pull*. Generate.
2. In `App_Update()` temporarily add:
   ```c
   LOG("BTN = %d\n", HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin));
   HAL_Delay(200);
   ```
3. Run in Debug and press the button. Write down: released = ?, pressed = ?

Most of these modules have a resistor that holds the output in one state, and the button pulls it to the other. If yours reads random values when released, it has no resistor: set the pin's **Pull** to *Pull-up* in CubeMX and test again.

**Result with this module:** released = random (floating), pressed = steady `0`. After setting *Pull-up*: released = `1`, pressed = `0`. So a press is a **falling edge** (1 → 0).

**Why random?** A pin with nothing driving it is *floating*: it picks up noise and reads 0 or 1 at random. The pull-up is an internal resistor (~40 kΩ) from the pin to 3.3 V. Released, it pulls the pin high. Pressed, the button connects the pin to GND, which wins over the weak resistor:

```
3.3V
 |
[40k]  internal pull-up
 |
 +---- PE4
 |
[button]
 |
GND
```

Remove the test code.

## CubeMX setup

The settings are explained in [CUBEMX_GPIO_NVIC.md](../CUBEMX_GPIO_NVIC.md). Do them in this order:

1. **Pinout view**: left-click **PE4** → `GPIO_EXTI4`. Left-click **PE5** → `GPIO_Output`.
   - The GPIO mode dropdown offers the interrupt modes **only** for `GPIO_EXTIx` pins. On a `GPIO_Input` pin it shows just *Input mode*.
2. **System Core → GPIO**, select the **PE4** row, and set in the box below the table:
   - **GPIO mode**: *External Interrupt Mode with Falling edge trigger detection* (use *Rising* if your button reads 1 when pressed). Not *Event Mode*: event mode never calls your code.
   - **GPIO Pull-up/Pull-down**: *Pull-up* (as found in step 0).
   - **User Label**: `BTN`.
3. Select the **PE5** row: mode *Output Push Pull*, *No pull*, **User Label** `BUZZER`.
4. **System Core → NVIC**: tick **EXTI line4 interrupt**. The row only appears after PE4 is in an interrupt mode.
5. **Generate Code**, then check:
   - `main.h` has `BTN_Pin`, `BTN_GPIO_Port`, `BUZZER_Pin`, `BUZZER_GPIO_Port`
   - `main.c` calls `HAL_NVIC_EnableIRQ(EXTI4_IRQn)`
   - `stm32f4xx_it.c` has `EXTI4_IRQHandler()`, which calls `HAL_GPIO_EXTI_IRQHandler(BTN_Pin)`, which calls your `HAL_GPIO_EXTI_Callback()`

> **Trap:** changing a pin's function in the Pinout view (`GPIO_Input` → `GPIO_EXTI4`) **resets its pull and label**. Re-check both after changing it. An empty label means no `BTN_Pin` and a compile error.

The chain is: pin edge → EXTI hardware → NVIC → `EXTI4_IRQHandler` → HAL → `HAL_GPIO_EXTI_Callback`.

### What you set, and why

| Setting | Why |
|---|---|
| **PE4 → `GPIO_EXTI4`** | A press is rare and unpredictable. Polling in the main loop wastes CPU and can miss short presses. EXTI hardware watches the pin and interrupts the CPU the moment it changes. |
| **Pull-up** | Without it the pin floats and would fire the interrupt randomly. The pull-up gives a defined idle state: released = 1, pressed = 0. |
| **Falling edge** | A press is the 1 → 0 change. One edge = one event per press. *Rising* would fire on release, *Rising/Falling* twice per press. |
| **Interrupt, not Event mode** | Event mode triggers hardware without running a handler, so the callback would never be called. |
| **NVIC: EXTI line4 enabled** | Two gates: EXTI detects the edge, NVIC decides whether the CPU is interrupted. If the NVIC line is off the edge is ignored. Line 4 because **EXTI line number = pin number**. |
| **Label `BTN` / `BUZZER`** | CubeMX generates `BTN_Pin` / `BTN_GPIO_Port` in `main.h`, so your code never contains raw `GPIO_PIN_4` / `GPIOE`. |
| **PE5 → push-pull output, no pull** | A passive buzzer only sounds when its input changes, so the code toggles PE5 hundreds of times per second. That needs an output that actively drives both HIGH and LOW. A pull resistor does nothing on an output. Initial level Low = silent at boot. |
| **Priority 0** | Nothing else competes for the CPU yet. Priorities matter in lesson 20. |

The module's `+` and `-` pins only supply power. Only the S wires carry the signal into the chip.

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
