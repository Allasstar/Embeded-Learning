# Lesson 17: PID Speed Control

## Goal
Hold the motor at a target RPM no matter what: when you brake it with your finger, when the battery sags, when the target changes. Tune the controller from the console and plot the results.

## What you learn
- Open-loop vs closed-loop control
- P, I and D terms, and what each one does to the response
- Fixed-rate control loops
- Integral windup and how to prevent it
- Practical tuning by experiment
- Live parameter tuning over UART

## Open loop vs closed loop
In lesson 16 you set a **duty** and hoped for a speed. That's **open loop**: load, friction or battery voltage change the result, and nothing corrects it.

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
Same as lesson 16. Bluetooth console (lesson 10) connected for tuning and logging.

## Steps

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

Use the **period method** RPM from lesson 16 (better at low speed): rename `RpmFromPeriod()` to `Motor_GetRpm()` and declare it in `motor.h`. The controller smooths it with a light exponential filter (`rpm += (raw - rpm) * 0.3f`).

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

### Step 3: Console commands for live tuning
Add to the lesson 10 command handler:

```c
else if (strcmp(command, "kp") == 0 && argument) kp = strtof(argument, NULL);
else if (strcmp(command, "ki") == 0 && argument) ki = strtof(argument, NULL);
else if (strcmp(command, "kd") == 0 && argument) kd = strtof(argument, NULL);
else if (strcmp(command, "rpm") == 0 && argument) targetRpm = strtof(argument, NULL);
else if (strcmp(command, "log") == 0) logging = !logging;
```

(`extern` the gains, `targetRpm` and a `uint8_t logging` flag in `speed_control.h`.)

### Step 4: Log for plotting
When `logging` is on, every control step prints CSV:

```c
Console_Printf("%lu,%ld,%ld,%ld\r\n", HAL_GetTick(), (int32_t)targetRpm, (int32_t)rpm, (int32_t)output);
```

(integers: `nano.specs` printf has no `%f`). 4 lines × 20/s at 9600 baud fits, but only just. Raise the Bluetooth baud rate if you can.

Capture the output to a file (PuTTY → Logging), open in Excel / LibreOffice / Python, and plot target and rpm against time. Or use *SerialPlot* for a live graph.

### Step 5: Tune
Use a target in the middle of the motor's range (e.g. `rpm 120`), and after each change send a step: `rpm 60`, wait 3 s, `rpm 140`.

1. **P only.** `ki 0`, `kd 0`. Start `kp 1`. Increase until the speed **oscillates** around the target. Note that value, then use **half** of it.
   You'll see the speed settle **below** the target. That's P's steady-state error: a P controller needs an error to produce any output.
2. **Add I.** Start `ki 1`, increase until the steady error disappears within ~1 s. Too much → overshoot and slow wobble. Back off.
3. **D**: try a small `kd`. With a 20-slot encoder the RPM signal is noisy, and D amplifies noise: you'll likely hear the motor get rough. For this system **PI is usually best**. That's a real-world lesson too.

### Step 6: Prove it works
1. **Disturbance**: at `rpm 100`, squeeze the wheel lightly. In open loop (lesson 16) the speed drops and stays low. With PI, it dips and **recovers**: watch the output climb in the log.
2. **Supply**: power the motor from the 9 V battery through the power module's 5 V, then from USB 5 V. Open loop: different speeds. Closed loop: same speed.
3. **Low speed**: try `rpm 30`. Below the dead band, open loop can't hold it at all. The integral pushes through the static friction.

## Check it works
- [ ] Speed reaches the target within ~1 s, overshoot < 15 %
- [ ] Holds speed under a light brake
- [ ] No runaway after releasing a hard stall (anti-windup)
- [ ] Gains change live from the console

## Troubleshooting
| Symptom | Cause |
|---|---|
| Wild oscillation | `kp` too high, or RPM measurement too noisy / too slow |
| Never reaches target | `ki = 0`, or output clamped too low |
| Huge overshoot after a stall | integral windup: anti-windup missing |
| Speeds up instead of slowing | sign error: error must be `target - measured` |
| Controller "sees" speed late | too much filtering on RPM: lower the 0.3 smoothing or use the raw period |

## Challenges
1. **Feed-forward**: from lesson 16's measurements, build a rough `duty = f(rpm)` (a straight line is fine) and add it to the PID output. The PID then only corrects the remainder: faster and smoother.
2. Make it **bidirectional**: signed targets, output −1000..1000, and direction from the motor driver. What does the encoder need to know direction? (Hint: a single-channel sensor can't. Quadrature encoders can.)
3. **Position control**: count pulses as position and hold the wheel at an angle with a PID on position.
4. Plot the step response for three different `kp` values on one chart.
