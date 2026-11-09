# Lesson 09: Joystick Controls the Servo

## Goal
Steer the servo with the joystick X axis in two modes (position and speed), switch modes with the joystick button, and control the brightness of an on-board LED with the Y axis.

## What you learn
- Combining modules (ADC + timer PWM + GPIO) through small interfaces
- Calibrating a centre point at start-up
- Mapping one range of numbers onto another
- Dead zones
- Integer maths instead of floats where it's easy

## Parts and wiring

| Module | Pin | Board pin |
|---|---|---|
| Joystick | `+5V` / GND | **3V** / GND |
| Joystick | VRx / VRy | PC1 / PC2 |
| Joystick | SW | PE14 |
| Servo | Brown (GND) | breadboard power module GND **and** board GND |
| Servo | Red (+5 V) | breadboard power module **5 V** rail |
| Servo | Orange (signal) | PB14 |

The joystick pin is labelled "+5V", but it's just two potentiometers. Power it from 3V so the outputs stay ≤ 3.3 V (ADC pins are not 5 V tolerant). The servo draws current spikes that can reset the board, so it gets its own 5 V rail.

## CubeMX setup
This lesson starts from a clean `master`, so it sets up everything it needs.

**1. Servo PWM.** **PB14** → `TIM12_CH1`. **Timers → TIM12**: Clock Source *Internal Clock*, Channel1 *PWM Generation CH1*, Prescaler `84-1`, Counter Period `20000-1`, auto-reload preload *Enable*.

*Why:* TIM12 runs at 84 MHz; dividing by 84 gives 1 MHz, so 1 tick = 1 µs and the compare value is the pulse width in µs. A period of 20000 ticks = 20 ms = the 50 Hz frame a servo expects.

**2. Joystick axes.** **PC1** → `ADC1_IN11`, **PC2** → `ADC1_IN12`. **Analog → ADC1** parameters:

| Setting | Value | Why |
|---|---|---|
| Scan Conversion Mode | Enabled | convert both channels one after the other |
| Continuous Conversion Mode | Enabled | start over by itself, no CPU involved |
| DMA Continuous Requests | Enabled | keep the DMA going after the first round |
| End Of Conversion Selection | EOC flag at the end of all conversions | one flag per round, not per channel |
| Number Of Conversion | `2` | X and Y |
| Rank 1 / Rank 2 | Channel 11 / Channel 12 | the order of the `dmaBuffer` array |
| Sampling Time | 480 Cycles | slow sampling = less noise; speed doesn't matter here |

**DMA Settings** tab → Add → `ADC1`: Stream *DMA2 Stream 0*, Mode **Circular**, Data Width *Half Word* / *Half Word*.

*Why:* the DMA controller copies each 12-bit result (a half word) into RAM and wraps around forever, so your code just reads the array. In `main.c` check that `MX_DMA_Init()` is called **before** `MX_ADC1_Init()`.

**3. LED brightness.** **PD12** → `TIM4_CH1` (the green LED loses its GPIO label). **Timers → TIM4**: Internal Clock, Channel1 *PWM Generation CH1*, Prescaler `84-1`, Counter Period `1000-1`, auto-reload preload *Enable*.

*Why:* 1 MHz / 1000 = 1 kHz PWM, far above visible flicker, and the compare value 0..1000 is the brightness.

**4. Joystick button.** **PE14** → `GPIO_Input`, label `JOY_SW`, Pull: **Pull-up**. The joystick button connects SW to GND when pressed and has no resistor of its own, so without a pull-up the pin floats.

Generate Code. Remove the old LED blinker from `app.c`: PD12 is a timer pin now.

## Steps

### Step 0: Two small drivers
Both are minimal versions of what lessons 06 and 08 teach in depth.

`Core/Src/servo.c`:

```c
#include "servo.h"
#include "main.h"

extern TIM_HandleTypeDef htim12;

#define SERVO_MIN_US 1000
#define SERVO_MAX_US 2000
#define SERVO_STEP_MS 2

static uint32_t current = 90;
static uint32_t target = 90;
static uint32_t lastStep;

static void SetAngle(uint32_t degrees)
{
  __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1,
      SERVO_MIN_US + degrees * (SERVO_MAX_US - SERVO_MIN_US) / 180);
}

void Servo_Init(void)
{
  SetAngle(current);
  HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1);
}

void Servo_MoveTo(uint32_t degrees)
{
  target = degrees > 180 ? 180 : degrees;
}

void Servo_Update(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastStep < SERVO_STEP_MS)
    return;
  lastStep = now;

  if (current < target) current++;
  else if (current > target) current--;
  else return;

  SetAngle(current);
}
```

It moves one degree per `SERVO_STEP_MS` instead of jumping, so there is no big current spike. 1000–2000 µs usually covers less than the full 180°; if your servo hits its stop (buzzing), shrink the range.

`Core/Src/analog_in.c`:

```c
#include "analog_in.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;

static volatile uint16_t dmaBuffer[ANALOG_COUNT];
static int32_t filteredX16[ANALOG_COUNT];
static uint32_t lastFilterTick;

void AnalogIn_Init(void)
{
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)dmaBuffer, ANALOG_COUNT);
}

void AnalogIn_Update(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastFilterTick < 1)
    return;
  lastFilterTick = now;

  for (uint32_t i = 0; i < ANALOG_COUNT; i++)
    filteredX16[i] += dmaBuffer[i] - filteredX16[i] / 16;
}

uint16_t AnalogIn_Raw(AnalogInput input)
{
  return dmaBuffer[input];
}

uint16_t AnalogIn_Filtered(AnalogInput input)
{
  return filteredX16[input] / 16;
}
```

`analog_in.h` has an enum `ANALOG_JOY_X, ANALOG_JOY_Y, ANALOG_COUNT` (the order **must match the CubeMX rank order**) and the four function prototypes; `servo.h` declares `Servo_Init`, `Servo_MoveTo`, `Servo_Update`. `dmaBuffer` is `volatile` because the DMA writes it behind the compiler's back. The filter `filteredX16 += raw - filteredX16 / 16` is an exponential moving average: each sample moves the result 1/16 of the way toward it, stored ×16 to keep the fraction. It runs once per millisecond so the smoothing has a fixed time constant (~16 ms); run on every loop pass it would depend on how fast the loop happens to be.

Add both `.c` files to `CMakeLists.txt`. Call `AnalogIn_Init()` then `Servo_Init()` in `App_Start()`, and `AnalogIn_Update()` + `Servo_Update()` from `App_Update()`.

### Step 1: Map function
Almost every sensor-to-actuator project needs this:

```c
static int32_t Map(int32_t x, int32_t inMin, int32_t inMax, int32_t outMin, int32_t outMax)
{
  if (x < inMin) x = inMin;
  if (x > inMax) x = inMax;
  return outMin + (x - inMin) * (outMax - outMin) / (inMax - inMin);
}
```

Multiply **before** dividing. `(x - inMin) / (inMax - inMin) * ...` in integer maths is almost always 0.

### Step 2: Calibrate the centre
A joystick at rest rarely reads exactly 2047. Measure it at start-up (don't touch the stick while the board resets):

`Core/Src/joystick_servo.c`:

```c
#include "joystick_servo.h"
#include "analog_in.h"
#include "servo.h"
#include "main.h"
#include "log.h"

#define DEAD_ZONE 80
#define CALIBRATION_SAMPLES 32

static int32_t centerX;
static int32_t centerY;

void JoystickServo_Init(void)
{
  int32_t sumX = 0;
  int32_t sumY = 0;

  for (int i = 0; i < CALIBRATION_SAMPLES; i++)
  {
    HAL_Delay(1);
    sumX += AnalogIn_Raw(ANALOG_JOY_X);
    sumY += AnalogIn_Raw(ANALOG_JOY_Y);
  }

  centerX = sumX / CALIBRATION_SAMPLES;
  centerY = sumY / CALIBRATION_SAMPLES;
  LOG("Joystick centre %ld, %ld\n", centerX, centerY);
}
```

It averages raw samples rather than reading the filtered value, because the filter only runs in `AnalogIn_Update()`, which hasn't been called yet during init. The DMA keeps `AnalogIn_Raw()` fresh on its own. Call `AnalogIn_Init()` before `JoystickServo_Init()`.

Create `joystick_servo.h` with `JoystickServo_Init()` and `JoystickServo_Update()`, and call them from `App_Start()` / `App_Update()` like the other modules.

### Step 3: Dead zone + position mode
Near the centre the reading jitters. Treat everything within ±`DEAD_ZONE` as exactly centre. Then map each half separately, because the centre isn't exactly in the middle:

```c
static int32_t AxisToAngle(int32_t raw, int32_t center)
{
  if (raw > center - DEAD_ZONE && raw < center + DEAD_ZONE)
    return 90;
  if (raw < center)
    return Map(raw, 0, center - DEAD_ZONE, 0, 90);
  return Map(raw, center + DEAD_ZONE, 4095, 90, 180);
}
```

In `Update()`, every 20 ms (the servo only takes a new pulse every 20 ms anyway):

```c
Servo_MoveTo(AxisToAngle(AnalogIn_Filtered(ANALOG_JOY_X), centerX));
```

`SERVO_STEP_MS` is already 2 ms, so the servo keeps up with the stick.

### Step 4: Speed (rate) mode
In position mode, releasing the stick sends the servo back to 90°. In **rate mode** the stick sets the *speed*: hold it right and the servo keeps turning right; let go and it stays where it is. That's how cranes and camera gimbals are controlled.

```c
static int32_t angle = 90;

static void RateMode(int32_t raw, int32_t center)
{
  int32_t offset = raw - center;
  if (offset > -DEAD_ZONE && offset < DEAD_ZONE)
    return;

  angle += offset / 400;
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  Servo_MoveTo(angle);
}
```

`offset / 400` gives up to ±5° per 20 ms at full deflection. Tune it.

### Step 5: Mode button
Read `JOY_SW` every 20 ms (that sampling rate is itself a debounce), detect the falling edge (pressed = 0 because of the pull-up) and toggle `mode`. Blink an on-board LED on mode change (LD3, PD13, is still a plain GPIO).

### Step 6: Y axis → LED brightness
Map Y to 0..1000 and write it to TIM4 channel 1 (the green LED) with `__HAL_TIM_SET_COMPARE`; call `HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1)` in `JoystickServo_Init()` first.

## Check it works
- [ ] Stick at rest: servo sits at 90° without twitching
- [ ] Position mode: servo follows the stick, returns to 90° on release
- [ ] Rate mode: servo moves while the stick is held, stays on release
- [ ] Button switches modes exactly once per press
- [ ] Y axis smoothly dims an LED

## Troubleshooting
| Symptom | Cause |
|---|---|
| Servo twitches at rest | dead zone too small, or centre measured while touching the stick |
| Servo always at one end | centre measured as 0: filter wasn't settled during calibration |
| Button reads 0 all the time | it's pressed, or pull-up not set |
| One direction moves faster | centre isn't 2047 and you used a single `Map(0..4095)` |

## Challenges
1. Make speed proportional to the **square** of the offset: precise near centre, fast at the ends ("expo" in RC controllers).
2. Save the current angle as a "home" position on a long press, and return to it on a double press.
3. Replace the joystick with the **rotary encoder** for exact positioning (encoder mode is covered in lesson 18).
