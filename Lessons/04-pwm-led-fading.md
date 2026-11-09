# Lesson 04: LED Fading with PWM

## Goal
Make the four on-board LEDs "breathe" smoothly, then mix any colour on the RGB LED module.

## What you learn
- What a hardware timer is: prescaler, counter, auto-reload (period)
- PWM: brightness by switching fast, duty cycle
- Calculating timer settings from the clock tree
- Why linear brightness looks wrong (gamma)
- Alternate functions: handing a pin from GPIO to a peripheral

## How PWM works

A timer counter counts up from 0 to **Period**, then restarts. A PWM channel compares the counter with a **Compare** value: output is HIGH while `counter < Compare`, LOW after.

```
counter:  0 ... Compare ............ Period | 0 ...
output:   ‾‾‾‾‾‾‾‾‾‾|________________________|‾‾‾‾‾
duty = Compare / (Period + 1)
```

Switch fast enough (> 200 Hz) and your eye sees the average: 25% duty = dim, 100% = full.

**The numbers:**

```
timer clock     = 84 MHz     (APB1 timers: TIM2-7, TIM12-14)
counter clock   = timer clock / (Prescaler + 1)
PWM frequency   = counter clock / (Period + 1)
```

Prescaler = 83 → counter at **1 MHz**. Period = 999 → PWM at **1 kHz**, with Compare from 0 to 1000 as brightness.

The `-1` is because the hardware divides by `register + 1`, so CubeMX fields are often written as `84-1` and `1000-1` (CubeMX accepts expressions).

## Parts and wiring
Part 1 uses only the board. For part 2:

| RGB module pin | Board pin |
|---|---|
| `-` / GND | GND |
| R | PB4 |
| G | PB5 |
| B | PB0 |

Check your module has resistors (small SMD parts next to the LED). If not, put a 220 Ω resistor in series with each colour.

## Part 1: On-board LEDs with TIM4

On this board the LEDs PD12–PD15 are wired to **TIM4 channels 1–4**. That's not luck: ST picked those pins so you can do exactly this.

### CubeMX setup
1. Click **PD12** → `TIM4_CH1`. **PD13** → `TIM4_CH2`, **PD14** → `TIM4_CH3`, **PD15** → `TIM4_CH4`. The pins lose their GPIO labels. That's fine.
2. **Timers → TIM4**:
   - Clock Source: *Internal Clock*
   - Channel 1–4: *PWM Generation CH1..CH4*
   - Parameter Settings → Prescaler: `84-1`, Counter Period: `1000-1`, auto-reload preload: *Enable*
3. Generate Code. You now have `htim4` and `MX_TIM4_Init()`, and `stm32f4xx_hal_tim.h` appears in `Drivers/`.

### Fix app.c
`app.c` writes the LEDs with `HAL_GPIO_WritePin` and uses `LD4_Pin` etc. Those labels are gone now. Remove that code (or the whole old blinker).

### Step 1: Light them at fixed levels

`Core/Inc/led_fade.h`:

```c
#pragma once

void LedFade_Init(void);
void LedFade_Update(void);
```

`Core/Src/led_fade.c`:

```c
#include "led_fade.h"
#include "main.h"

extern TIM_HandleTypeDef htim4;

void LedFade_Init(void)
{
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);

  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 10);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 100);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, 500);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 1000);
}

void LedFade_Update(void)
{
}
```

`htim4` is defined in `main.c`; `extern` tells the compiler it exists somewhere else.

Flash. Four LEDs at four brightness levels. Notice: 500 (50%) looks nearly as bright as 1000. Your eye is not linear.

### Step 2: Breathing

```c
static uint32_t lastStep;
static int32_t level;
static int32_t direction = 1;

void LedFade_Update(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastStep < 2)
    return;
  lastStep = now;

  level += direction;
  if (level >= 1000 || level <= 0)
    direction = -direction;

  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, level);
}
```

It breathes, but it seems to spend most of the time "bright" and drops fast near the bottom.

### Step 3: Gamma correction
Perceived brightness is roughly the square of the duty cycle. Squaring the level spreads the steps evenly for your eye:

```c
static uint32_t Gamma(int32_t level)
{
  return (uint32_t)(level * level) / 1000;
}
```

Use `Gamma(level)` for channel 1, raw `level` for channel 2, and compare. Then make all four LEDs breathe with a phase offset (e.g. each 250 steps behind the previous one) for a rotating effect.

## Part 2: RGB LED with TIM3

### CubeMX setup
1. **PB4** → `TIM3_CH1`, **PB5** → `TIM3_CH2`, **PB0** → `TIM3_CH3`.
2. **TIM3**: Internal Clock, channels 1–3 *PWM Generation*, Prescaler `84-1`, Period `1000-1`.
3. Generate.

### Step 4: Set a colour

```c
extern TIM_HandleTypeDef htim3;

static void Rgb_Set(uint32_t r, uint32_t g, uint32_t b)
{
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, Gamma(r));
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, Gamma(g));
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, Gamma(b));
}
```

Start the three channels in `Init`, then try `Rgb_Set(1000, 0, 0)` (red), `(1000, 500, 0)` (orange), `(1000, 0, 1000)` (purple).

### Step 5: Colour wheel
A **hue** from 0 to 1535 walks through all colours: six segments of 256 steps, in each one channel rises or falls.

```c
static void Rgb_SetHue(uint32_t hue)
{
  uint32_t segment = (hue / 256) % 6;
  uint32_t x = (hue % 256) * 1000 / 255;

  switch (segment)
  {
    case 0: Rgb_Set(1000, x, 0);        break;
    case 1: Rgb_Set(1000 - x, 1000, 0); break;
    case 2: Rgb_Set(0, 1000, x);        break;
    case 3: Rgb_Set(0, 1000 - x, 1000); break;
    case 4: Rgb_Set(x, 0, 1000);        break;
    default: Rgb_Set(1000, 0, 1000 - x); break;
  }
}
```

Step the hue by 1 every 5 ms in `Update()`.

## Check it works
- [ ] On-board LEDs breathe smoothly with no visible flicker
- [ ] Gamma version looks evenly smooth; linear version doesn't
- [ ] RGB cycles through all colours smoothly

## Troubleshooting
| Symptom | Cause |
|---|---|
| LEDs off | `HAL_TIM_PWM_Start` not called for that channel |
| `htim4` undeclared | missing `extern TIM_HandleTypeDef htim4;` |
| One RGB colour missing | wrong channel/pin, or the LED is common-anode (then 0 = full on: use `1000 - value`) |
| Visible flicker | PWM frequency too low: check Prescaler / Period |

## Challenges
1. Change Period to `100-1` and Prescaler to `8400-1`. PWM is now 100 Hz. Wave the board: you'll see the strobing. Find the frequency where flicker disappears for you.
2. Use the **potentiometer** later (lesson 08) to set brightness.
3. Fade without the CPU: look up **DMA burst / timer DMA** (`HAL_TIM_PWM_Start_DMA`) and feed a table of compare values from memory.
