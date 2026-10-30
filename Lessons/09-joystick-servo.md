# Lesson 09: Joystick Controls the Servo

## Goal
Steer the servo with the joystick X axis in two modes (position and speed), switch modes with the joystick button, and control LED brightness with the Y axis.

## What you learn
- Combining modules (ADC + timer PWM + GPIO) through small interfaces
- Calibrating a centre point at start-up
- Mapping one range of numbers onto another
- Dead zones
- Integer maths instead of floats where it's easy

## Parts and wiring
Joystick and servo exactly as in lessons 06 and 08, plus:

| Joystick pin | Board pin |
|---|---|
| SW | PE14 |

## CubeMX setup
**PE14** → `GPIO_Input`, label `JOY_SW`, Pull: **Pull-up**. The joystick button connects SW to GND when pressed and has no resistor of its own, so without a pull-up the pin floats.

Generate.

## Steps

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

Create `joystick_servo.h` with `JoystickServo_Init()` and `JoystickServo_Update()`, as in earlier lessons.

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

Lower the servo step delay from lesson 06 to 2 ms so it keeps up.

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
Read `JOY_SW` every 20 ms (that sampling rate is itself a debounce), detect the falling edge (pressed = 0 because of the pull-up) and toggle `mode`. Blink the blue LED on mode change, or give it a different brightness per mode.

### Step 6: Y axis → LED brightness
Map Y to 0..1000 and write it to a TIM4 channel from lesson 04 with `__HAL_TIM_SET_COMPARE`.

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
3. Replace the joystick with the **rotary encoder** for exact positioning (you'll learn encoder mode properly in lesson 18).
