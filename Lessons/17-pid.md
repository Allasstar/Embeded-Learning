# Lesson 17: PID Speed Control

## Goal
Hold the motor at a target RPM no matter what: when you brake it with your finger, when the battery sags, when the target changes. Tune the controller live in the debugger and plot the results.

## What you learn
- Open-loop vs closed-loop control
- P, I and D terms, and what each one does to the response
- Fixed-rate control loops
- Integral windup and how to prevent it
- Practical tuning by experiment
- Live parameter tuning with the debugger's watch window

## Open loop vs closed loop
With a plain motor driver you set a **duty** and hope for a speed (lesson 16 does exactly this). That's **open loop**: load, friction or battery voltage change the result, and nothing corrects it.

**Closed loop**: measure the speed, compare with the target, and adjust the duty based on the **error**. That's a feedback controller. PID is the most common one.

```
         error            output (duty)
target ──(+)──► [ PID ] ──────────► motor ──┬──► rpm
          ▲(−)                              │
          └──────────── speed sensor ◄──────┘
```

## The three terms
`error = target − measured`

| Term | Formula | Effect |
|---|---|---|
| **P**roportional | `Kp × error` | push harder the further off you are. Too high = oscillation. Alone, it leaves a steady error |
| **I**ntegral | `Ki × Σ(error × dt)` | keeps pushing while *any* error remains → removes steady error. Too high = overshoot, slow wobble |
| **D**erivative | `Kd × d(measured)/dt` | brakes fast changes → less overshoot. Amplifies noise. Often 0 for speed control |

`output = P + I + D`, clamped to the actuator's range.

## Parts and wiring
This lesson is self-contained: it repeats the motor wiring and setup it needs.

| From | To |
|---|---|
| L9110 VCC | breadboard power module **5V** |
| L9110 GND | power module GND **and** board GND |
| L9110 A-IA | PB8 |
| L9110 A-IB | PB7 |
| L9110 MOTOR-A terminals | TT motor's two wires |
| Speed module VCC / GND | 3V / GND |
| Speed module DO | PD3 |

Push the encoder disk onto the motor's second shaft so the slots pass through the speed module's gap (count the slots, usually 20). Never power the motor from the board: its current spikes can reset the MCU.

## CubeMX setup
1. **PB8** → `TIM10_CH1`. **TIM10**: *Activated*, Channel1 *PWM Generation CH1*, Prescaler `0`, Period `8400-1`. *Why:* TIM10 runs at 168 MHz; 168 MHz / 8400 = 20 kHz, above hearing, so the motor doesn't whine.
2. **PB7** → `GPIO_Output`, label `MOTOR_DIR`. *Why:* the L9110 needs a direction input next to the PWM input.
3. **PD3** → `GPIO_EXTI3`, label `SPEED`, *Rising edge*, no pull. NVIC: **EXTI line3 interrupt**. *Why:* every slot passing the sensor is one rising edge; the interrupt timestamps it.
4. Generate. In `app.c` keep the blue button edge detection (Step 3 uses it) and delete the LED blinking.

## Steps

### Step 0: Minimal motor driver and RPM sensor
Forward only, since the controller below only needs 0..1000. `Core/Src/motor.c`:

```c
#include "motor.h"
#include "main.h"

extern TIM_HandleTypeDef htim10;

#define PWM_MAX 8400
#define SLOTS 20

static volatile uint32_t lastPulseCycles;
static volatile uint32_t periodUs;

static uint32_t UsSince(uint32_t startCycles)
{
  return (DWT->CYCCNT - startCycles) / (SystemCoreClock / 1000000);
}

void Motor_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  HAL_GPIO_WritePin(MOTOR_DIR_GPIO_Port, MOTOR_DIR_Pin, GPIO_PIN_RESET);
  Motor_Set(0);
  HAL_TIM_PWM_Start(&htim10, TIM_CHANNEL_1);
}

void Motor_Set(int32_t permille)
{
  if (permille < 0) permille = 0;
  if (permille > 1000) permille = 1000;
  __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, permille * PWM_MAX / 1000);
}

void Motor_OnPulse(void)
{
  uint32_t elapsed = UsSince(lastPulseCycles);
  if (elapsed < 300)
    return;
  periodUs = elapsed;
  lastPulseCycles = DWT->CYCCNT;
}

uint32_t Motor_GetRpm(void)
{
  if (UsSince(lastPulseCycles) > 200000)
    periodUs = 0;
  if (periodUs == 0)
    return 0;
  return 60000000UL / (periodUs * SLOTS);
}
```

`motor.h` declares the four public functions. Why it measures the **period** between pulses and not pulses per second: at low speed a 20-slot disk gives only a few pulses per 100 ms, so counting is coarse, while the period gives a new value on every slot. The `DWT->CYCCNT` register counts CPU cycles (168 per µs), the only clock fine enough for this. Time stamps are kept in raw cycles and only the *difference* is divided, because the 32-bit counter wraps cleanly at 2^32 (every 25.6 s) and unsigned subtraction handles that; a converted µs value would jump from ~25 565 000 back to 0 instead and give one bogus reading every 25.6 s. Clearing `periodUs` on timeout keeps a stopped motor at 0 even after the counter wraps. The 300 µs filter drops double triggers from slow comparator edges, and the 200 ms timeout reports 0 for a stopped motor.

Forward the interrupt in `app.c`:

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == SPEED_Pin) Motor_OnPulse();
}
```

Call `Motor_Init()` in `App_Start()`. Quick test: `Motor_Set(600)` and `LOG` `Motor_GetRpm()` every 200 ms; the motor should run and report a plausible RPM.

### Step 1: Fixed-rate loop
A PID controller assumes a constant time step. Run it exactly every **50 ms** (`dt = 0.05 s`), driven by `HAL_GetTick()`:

```c
#define CONTROL_PERIOD_MS 50

static uint32_t lastControl;

void SpeedControl_Update(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastControl < CONTROL_PERIOD_MS)
    return;
  lastControl += CONTROL_PERIOD_MS;

  Control(CONTROL_PERIOD_MS / 1000.0f);
}
```

`lastControl += PERIOD` (not `= now`) keeps the average rate exact even if one call comes late.

The controller reads `Motor_GetRpm()` and smooths it with a light exponential filter (`rpm += (raw - rpm) * 0.3f`).

### Step 2: The controller

`Core/Src/speed_control.c`:

```c
#include "speed_control.h"
#include "motor.h"
#include "main.h"

float kp = 2.0f;
float ki = 0.0f;
float kd = 0.0f;
float targetRpm = 0.0f;

static float integral;
static float previousRpm;
static float rpm;
static float output;

static float Clamp(float value, float low, float high)
{
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

static void Control(float dt)
{
  rpm += ((float)Motor_GetRpm() - rpm) * 0.3f;

  float error = targetRpm - rpm;
  float derivative = -(rpm - previousRpm) / dt;
  previousRpm = rpm;

  float unclamped = kp * error + ki * (integral + error * dt) + kd * derivative;
  output = Clamp(unclamped, 0.0f, 1000.0f);

  if (unclamped == output)
    integral += error * dt;

  if (targetRpm <= 0.0f)
  {
    output = 0.0f;
    integral = 0.0f;
  }

  Motor_Set((int32_t)output);
}
```

Two details worth understanding:

- **Derivative on measurement**: `d(rpm)/dt` instead of `d(error)/dt`. When the target jumps, `d(error)` spikes and kicks the motor ("derivative kick"); `d(rpm)` doesn't.
- **Anti-windup**: if the output is already at its limit (e.g. 1000 = full power but the motor is stalled), the integral would keep growing. When the stall ends, that huge integral makes the motor overshoot for seconds. So we only integrate while the output is **not** saturated.

This controller only runs forward (output 0..1000). Making it bidirectional is a challenge.

### Step 3: Live tuning without a console
`kp`, `ki`, `kd` and `targetRpm` are global, so change them while the program runs: in VS Code's debug view add them to **Watch** (or **Live Watch** with Cortex-Debug) and edit the value. The new gain is used at the next control step, no rebuild needed.

For the step input of Step 5, make the blue button toggle between two targets. In `App_Update()`, on its rising edge (already detected in `app.c`):

```c
targetRpm = (targetRpm == 60.0f) ? 140.0f : 60.0f;
```

(Declare `extern float kp, ki, kd, targetRpm;` in `speed_control.h` if you edit them from other files.)

### Step 4: Log for plotting
Every control step, print one CSV line:

```c
LOG("%lu,%ld,%ld,%ld\n", HAL_GetTick(), (int32_t)targetRpm, (int32_t)rpm, (int32_t)output);
```

(integers: `nano.specs` printf has no `%f`). Capture the debug console output to a file, open it in Excel / LibreOffice / Python, and plot target and rpm against time. Logging blocks briefly (semihosting), so use the Debug build only and log every second step if the timing suffers.

### Step 5: Tune
Use targets in the middle of the motor's range, and after each gain change press the blue button to send a step between 60 and 140 RPM; wait 3 s between presses.

1. **P only.** `ki = 0`, `kd = 0`. Start `kp = 1`. Increase until the speed **oscillates** around the target. Note that value, then use **half** of it.
   You'll see the speed settle **below** the target. That's P's steady-state error: a P controller needs an error to produce any output.
2. **Add I.** Start `ki = 1`, increase until the steady error disappears within ~1 s. Too much → overshoot and slow wobble. Back off.
3. **D**: try a small `kd`. With a 20-slot encoder the RPM signal is noisy, and D amplifies noise: you'll likely hear the motor get rough. For this system **PI is usually best**. That's a real-world lesson too.

### Step 6: Prove it works
1. **Disturbance**: at a target of 100 RPM, squeeze the wheel lightly. In open loop (a fixed duty) the speed drops and stays low. With PI, it dips and **recovers**: watch the output climb in the log.
2. **Supply**: power the motor from the 9 V battery through the power module's 5 V, then from USB 5 V. Open loop: different speeds. Closed loop: same speed.
3. **Low speed**: try a target of 30 RPM. Below the dead band, open loop can't hold it at all. The integral pushes through the static friction.

## Check it works
- [ ] Speed reaches the target within ~1 s, overshoot < 15 %
- [ ] Holds speed under a light brake
- [ ] No runaway after releasing a hard stall (anti-windup)
- [ ] Gains change live from the watch window

## Troubleshooting
| Symptom | Cause |
|---|---|
| Wild oscillation | `kp` too high, or RPM measurement too noisy / too slow |
| Never reaches target | `ki = 0`, or output clamped too low |
| Huge overshoot after a stall | integral windup: anti-windup missing |
| Speeds up instead of slowing | sign error: error must be `target - measured` |
| Controller "sees" speed late | too much filtering on RPM: lower the 0.3 smoothing or use the raw period |

## Challenges
1. **Feed-forward**: measure the duty needed for a few RPM values (log `output` at steady state), build a rough `duty = f(rpm)` (a straight line is fine) and add it to the PID output. The PID then only corrects the remainder: faster and smoother.
2. Make it **bidirectional**: signed targets, output −1000..1000, and direction from the motor driver. What does the encoder need to know direction? (Hint: a single-channel sensor can't. Quadrature encoders can.)
3. **Position control**: count pulses as position and hold the wheel at an angle with a PID on position.
4. Plot the step response for three different `kp` values on one chart.
5. Tune over Bluetooth instead of the debugger: add `kp`, `ki`, `kd`, `rpm` commands to a UART console (lesson 10).
