# Lesson 01: Traffic Light

## Goal
Run a realistic traffic light sequence on the LED Traffic Light module, first the naive way, then the right way. Add a "pedestrian button" that shortens the green phase.

## What you learn
- Configuring GPIO outputs in CubeMX and naming pins
- Why `HAL_Delay()` is a trap
- Non-blocking timing with `HAL_GetTick()`
- State machines: the most-used pattern in embedded code

## Parts and wiring

| Module pin | Board pin |
|---|---|
| GND | GND |
| R | PD0 |
| Y | PD1 |
| G | PD2 |

The module has resistors on board, so you can connect LEDs straight to GPIO pins. A GPIO pin set HIGH outputs 3.3 V and turns the LED on.

## CubeMX setup
1. Click **PD0** → `GPIO_Output`. Right-click → **Enter User Label** → `TL_RED`.
2. Same for **PD1** → `TL_YELLOW`, **PD2** → `TL_GREEN`.
3. Generate Code. Open `Core/Inc/main.h` and find `TL_RED_Pin` and `TL_RED_GPIO_Port`.

## Steps

### Step 1: The naive version
Create `Core/Inc/traffic_light.h`:

```c
#pragma once

void TrafficLight_Init(void);
void TrafficLight_Update(void);
```

Create `Core/Src/traffic_light.c`:

```c
#include "traffic_light.h"
#include "main.h"

static void SetLights(GPIO_PinState red, GPIO_PinState yellow, GPIO_PinState green)
{
  HAL_GPIO_WritePin(TL_RED_GPIO_Port, TL_RED_Pin, red);
  HAL_GPIO_WritePin(TL_YELLOW_GPIO_Port, TL_YELLOW_Pin, yellow);
  HAL_GPIO_WritePin(TL_GREEN_GPIO_Port, TL_GREEN_Pin, green);
}

void TrafficLight_Init(void)
{
  SetLights(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
}

void TrafficLight_Update(void)
{
  SetLights(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
  HAL_Delay(5000);
  SetLights(GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_RESET);
  HAL_Delay(1000);
  SetLights(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
  HAL_Delay(5000);
  SetLights(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);
  HAL_Delay(2000);
}
```

Add `Core/Src/traffic_light.c` to `CMakeLists.txt`. In `app.c` call `TrafficLight_Init()` in `App_Start()` and `TrafficLight_Update()` in `App_Update()`. Keep the existing button/LED code in `app.c`.

Build, flash, watch it work.

### Step 2: See the problem
Press the blue user button. The on-board LED code in `app.c` now reacts only once every 13 seconds, because `TrafficLight_Update()` doesn't return for 13 seconds. **One blocking function freezes the entire program.** In real devices you have to read sensors, answer a UART and refresh a display all at once, so nothing is allowed to block.

### Step 3: The state machine
Think of the light as being in one **state** at a time. Each state has a duration. When the time runs out, move to the next state. Every call to `Update()` only asks "is it time to switch yet?" and returns right away.

Replace `traffic_light.c` with:

```c
#include "traffic_light.h"
#include "main.h"
#include "log.h"

typedef enum
{
  STATE_RED,
  STATE_RED_YELLOW,
  STATE_GREEN,
  STATE_YELLOW,
  STATE_COUNT
} LightState;

static const uint32_t durationMs[STATE_COUNT] = { 5000, 1000, 5000, 2000 };

static LightState state;
static uint32_t stateStart;

static void SetLights(GPIO_PinState red, GPIO_PinState yellow, GPIO_PinState green)
{
  HAL_GPIO_WritePin(TL_RED_GPIO_Port, TL_RED_Pin, red);
  HAL_GPIO_WritePin(TL_YELLOW_GPIO_Port, TL_YELLOW_Pin, yellow);
  HAL_GPIO_WritePin(TL_GREEN_GPIO_Port, TL_GREEN_Pin, green);
}

static void EnterState(LightState next)
{
  state = next;
  stateStart = HAL_GetTick();
  LOG("Traffic light state %d\n", state);

  switch (state)
  {
    case STATE_RED:        SetLights(GPIO_PIN_SET,   GPIO_PIN_RESET, GPIO_PIN_RESET); break;
    case STATE_RED_YELLOW: SetLights(GPIO_PIN_SET,   GPIO_PIN_SET,   GPIO_PIN_RESET); break;
    case STATE_GREEN:      SetLights(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);   break;
    case STATE_YELLOW:     SetLights(GPIO_PIN_RESET, GPIO_PIN_SET,   GPIO_PIN_RESET); break;
    default: break;
  }
}

void TrafficLight_Init(void)
{
  EnterState(STATE_RED);
}

void TrafficLight_Update(void)
{
  if (HAL_GetTick() - stateStart >= durationMs[state])
  {
    EnterState((state + 1) % STATE_COUNT);
  }
}
```

Build and press the blue button again: the on-board LEDs now react instantly while the traffic light keeps running.

**Why `HAL_GetTick() - stateStart >= duration` and not `HAL_GetTick() >= stateStart + duration`?** `HAL_GetTick()` is a `uint32_t` that wraps to 0 after 49.7 days. Unsigned subtraction still gives the right elapsed time across the wrap; the addition form breaks. Always write timing checks as `now - start >= duration`.

### Step 4: Pedestrian button
Add a function that a button can call:

```c
void TrafficLight_RequestCrossing(void)
{
  if (state == STATE_GREEN && HAL_GetTick() - stateStart < durationMs[STATE_GREEN] - 1000)
  {
    stateStart = HAL_GetTick() - (durationMs[STATE_GREEN] - 1000);
  }
}
```

It moves the green phase forward so only 1 s of green is left. Declare it in the header and call it from `app.c` when the blue button is pressed (the existing edge detection in `App_Update()` already finds the press).

## Check it works
- [ ] Sequence is red → red+yellow → green → yellow → red, with the right durations
- [ ] Blue button changes on-board LEDs instantly at any time
- [ ] Pressing the button during green switches to yellow after ~1 s
- [ ] Debug build prints each state change in the debug console

## Troubleshooting
| Symptom | Cause |
|---|---|
| `TL_RED_Pin` undeclared | label not set in CubeMX, or code not regenerated |
| Undefined reference to `TrafficLight_Init` | `traffic_light.c` not added to `CMakeLists.txt` |
| One LED never lights | wrong pin, or the module's GND not connected |

## Challenges
1. Add a **night mode**: a long button press (>2 s) switches to blinking yellow (500 ms on/off). Another long press goes back to normal. Hint: a new state, plus measuring how long the button is held.
2. Store each state's lights in a table (`{red, yellow, green}` per state) instead of the `switch`. Which is easier to change?
3. Put the on-board LED blinking from `app.c` into its own non-blocking module too.
