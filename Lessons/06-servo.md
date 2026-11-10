# Lesson 06: Servo

## Goal
Move the 9G servo to any angle, sweep it smoothly, and step through positions with the button.

## What you learn
- Servo control signal: pulse **width**, not duty, carries the information
- Choosing timer settings for µs-accurate pulses
- Calibrating a real part instead of trusting nominal values
- Powering inductive / high-current loads separately

## How a hobby servo works
The servo expects a pulse every **20 ms** (50 Hz). The pulse width sets the angle:

```
 1.0 ms  →   0°   (nominal; SG90 often goes 0.5 ms)
 1.5 ms  →  90°
 2.0 ms  → 180°   (nominal; SG90 often goes 2.4 ms)
```

Inside, a motor, gears, a potentiometer on the output shaft and a small controller move the shaft until the pot matches the pulse.

## Parts and wiring

| Servo wire | Connect to |
|---|---|
| Brown (GND) | breadboard power module GND **and** board GND |
| Red (+5 V) | breadboard power module **5 V** rail |
| Orange (signal) | PB14 |

**Why not power it from the board?** A moving servo draws 100–250 mA, and up to ~700 mA when stalled. Those current spikes through the USB cable can drop the board's voltage and reset it. 3.3 V signal into a 5 V servo works fine.

Set the breadboard power module's jumper for the rail to **5V**, and power it from the 9 V battery or USB.

## The maths
**TIM12**, on APB1 → 84 MHz timer clock.

```
Prescaler = 84-1     → 1 MHz, 1 tick = 1 µs
Period    = 20000-1  → 20 ms = 50 Hz
Compare   = pulse width in µs (1000..2000)
```

With 1 µs resolution over 1000 µs of travel you get 1000 steps for 180°, about 0.18° per step. That's finer than the servo can actually move.

## CubeMX setup
1. **PB14** → `TIM12_CH1`.
   *Why PB14 / TIM12:* PB14 is free on the Discovery board (nothing on-board is wired to it), and TIM12 is a simple timer that no earlier lesson used, so it can't clash with LED or buzzer PWM.
2. **TIM12** → tick **Internal Clock**.
   *Why:* the timer counts the 84 MHz APB1 timer clock, which is the clock the maths above assumes.
3. Channel1 → *PWM Generation CH1*.
   *Why:* hardware PWM makes the pulse on the pin by itself, so it stays µs-accurate however busy the CPU is. If you toggled the pin from code, any delay in the main loop would make the pulse longer and the servo would jitter.
4. Prescaler `84-1`.
   *Why:* 84 MHz / 84 = 1 MHz, so one tick is 1 µs and the compare value equals the pulse width in µs. No conversion is needed in the code.
5. Counter Period `20000-1`.
   *Why:* 20000 ticks × 1 µs = 20 ms, which is the 50 Hz frame the servo expects. TIM12 is 16-bit (max 65535), so 20000 fits.
6. auto-reload preload → *Enable*.
   *Why:* a new period (ARR) value is applied only when the counter starts the next period, so changing it can never give one broken frame. The pulse width (CCR) has its own preload bit (`OC1PE`), which `HAL_TIM_PWM_ConfigChannel` always turns on. Because of that, a new pulse width also waits for the next period instead of cutting the current pulse short or making it twice as long.
7. Leave PWM Mode 1, Pulse `0`, CH Polarity *High*.
   *Why:* the pin is high from count 0 until the count reaches CCR, so CCR = pulse width. Pulse 0 means the pin stays low until `Servo_Init` sets the angle, so the servo doesn't jump to some random position at boot.
8. Generate.

## Steps

### Step 1: Driver

`Core/Inc/servo.h`:

```c
#pragma once
#include <stdint.h>

void Servo_Init(void);
void Servo_SetPulse(uint32_t us);
void Servo_SetAngle(uint32_t degrees);
```

`Core/Src/servo.c`:

```c
#include "servo.h"
#include "main.h"

extern TIM_HandleTypeDef htim12;

#define SERVO_MIN_US 1000
#define SERVO_MAX_US 2000

void Servo_Init(void)
{
  Servo_SetAngle(90);
  HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1);
}

void Servo_SetPulse(uint32_t us)
{
  __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, us);
}

void Servo_SetAngle(uint32_t degrees)
{
  if (degrees > 180)
    degrees = 180;
  Servo_SetPulse(SERVO_MIN_US + degrees * (SERVO_MAX_US - SERVO_MIN_US) / 180);
}
```

Flash: the servo jumps to the centre. Put a horn (the plastic arm) on it so you can see the angle.

### Step 2: Calibrate your servo
Nominal 1000–2000 µs usually gives less than 180°. Find your servo's real limits:

1. In the debugger, set a breakpoint after `Servo_Init()`.
2. Call `Servo_SetPulse(1000)`, then try 900, 800, 700... Watch the horn.
3. Stop when it no longer moves further, or you **hear buzzing / grinding** (it's pushing against its mechanical stop; back off 50 µs).
4. Same on the high side: 2000, 2100, 2200...

Write down your values and update `SERVO_MIN_US` / `SERVO_MAX_US`. Typical SG90: ~500 and ~2400.

Tip: in the debugger's **Watch** / **Debug Console** you can type `htim12.Instance->CCR1 = 700` to change the pulse live without rebuilding.

### Step 3: Smooth sweep
Jumping straight to an angle is violent (big current spike, jerky). Move one step at a time:

```c
static uint32_t current = 90;
static uint32_t target = 90;
static uint32_t lastStep;

void Servo_MoveTo(uint32_t degrees)
{
  target = degrees > 180 ? 180 : degrees;
}

void Servo_Update(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastStep < 10)
    return;
  lastStep = now;

  if (current < target) current++;
  else if (current > target) current--;
  else return;

  Servo_SetAngle(current);
}
```

Add `Servo_MoveTo()` and `Servo_Update()` to `servo.h` and call `Servo_Update()` from `App_Update()`.

10 ms per degree → 1.8 s for a full sweep. Change the 10 to change speed.

### Step 4: Button positions
On each blue-button press, cycle the target through `0 → 90 → 180 → 90 → 0...` with `Servo_MoveTo()`.

## Check it works
- [ ] Servo holds position quietly at rest (no buzzing)
- [ ] Full calibrated range reached, no grinding at the ends
- [ ] Sweep is smooth; other modules keep running
- [ ] Board never resets when the servo moves

## Troubleshooting
| Symptom | Cause |
|---|---|
| Servo twitches randomly | grounds not connected between power module and board |
| Board resets when servo moves | servo powered from the board, not the power module |
| Servo buzzes at an end | pulse beyond its mechanical range, so recalibrate |
| Doesn't move | wrong period (must be ~20 ms), PWM not started, no 5 V |
| Moves the wrong way | it's normal for some servos: swap min/max |

## Challenges
1. Make the sweep **ease in/out** (slow near the ends, fast in the middle) instead of constant speed.
2. Measure the pulse with a logic analyzer (if you have one). Is it exactly 1500 µs at 90°?
3. Combine with lesson 07 later: a **radar** that sweeps the ultrasonic sensor and logs `angle, distance`.
