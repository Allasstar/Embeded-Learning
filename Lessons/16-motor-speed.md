# Lesson 16: Motor Speed

## Goal
Drive the TT motor forwards and backwards at any speed with the L9110 H-bridge, and measure its real speed in RPM with the slotted encoder disk and speed sensor.

## What you learn
- Why a GPIO pin can't drive a motor, and what an H-bridge does
- Direction + PWM control (and a trick to do it with one PWM pin)
- PWM frequency for motors (20 kHz, inaudible)
- Motor dead band (static friction)
- Two ways to measure speed: **counting pulses** vs **measuring the period**, and when each one wins
- Electrical noise from motors

## How an H-bridge works
A motor needs ~100–800 mA; a GPIO pin gives ~20 mA. An **H-bridge** is four switches around the motor. Closing opposite pairs makes current flow one way or the other:

```
  VCC ──┬────────┬──
       S1       S3
        ├──(M)───┤
       S2       S4
  GND ──┴────────┴──
  S1+S4 = forward, S3+S2 = reverse
```

The L9110 has two H-bridges (A and B). Each has two inputs:

| IA | IB | Motor |
|---|---|---|
| L | L | off (coast) |
| H | L | forward |
| L | H | reverse |
| H | H | brake |

## Parts and wiring

| From | To |
|---|---|
| L9110 VCC | breadboard power module **5V** |
| L9110 GND | power module GND **and** board GND |
| L9110 A-IA | PB8 |
| L9110 A-IB | PB7 |
| L9110 MOTOR-A terminals | TT motor's two wires |
| Speed module VCC / GND | 3V / GND |
| Speed module DO | PD3 |

Push the **encoder disk** onto the motor's second shaft so the slots pass through the speed module's gap. Count the slots on your disk (usually 20).

**Never power the motor from the board.** Motors create current spikes and voltage noise that can reset the MCU.

## The PWM plan
PWM at 1 kHz makes motors whine audibly. Use **20 kHz**, just above hearing.

**TIM10** (APB2, 168 MHz), channel 1 on PB8:

```
Prescaler = 0          → 168 MHz
Period    = 8400-1     → 168 MHz / 8400 = 20 kHz
Compare   = 0..8400
```

### The one-PWM-pin trick
- **Forward**: IB = LOW, IA = PWM with duty *d*. Motor is driven *d* of the time, coasts the rest.
- **Reverse**: IB = HIGH, IA = PWM with duty *(1 − d)*. When IA is LOW, the pair (L, H) drives in reverse; when IA is HIGH, (H, H) brakes. So the reverse drive time is the LOW time = *d*.

One timer channel + one plain GPIO controls both direction and speed.

## CubeMX setup
1. **PB8** → `TIM10_CH1`. **TIM10**: *Activated*, Channel1 *PWM Generation CH1*, Prescaler `0`, Period `8400-1`.
2. **PB7** → `GPIO_Output`, label `MOTOR_DIR`.
3. **PD3** → `GPIO_EXTI3`, label `SPEED`, *Rising edge*, no pull. NVIC: **EXTI line3 interrupt**.
4. Generate.

## Steps

### Step 1: Motor driver

`Core/Src/motor.c`:

```c
#include "motor.h"
#include "main.h"

extern TIM_HandleTypeDef htim10;

#define PWM_MAX 8400

void Motor_Init(void)
{
  Motor_Set(0);
  HAL_TIM_PWM_Start(&htim10, TIM_CHANNEL_1);
}

void Motor_Set(int32_t speed)
{
  if (speed > 1000) speed = 1000;
  if (speed < -1000) speed = -1000;

  uint32_t duty = (uint32_t)(speed < 0 ? -speed : speed) * PWM_MAX / 1000;

  if (speed >= 0)
  {
    HAL_GPIO_WritePin(MOTOR_DIR_GPIO_Port, MOTOR_DIR_Pin, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, duty);
  }
  else
  {
    HAL_GPIO_WritePin(MOTOR_DIR_GPIO_Port, MOTOR_DIR_Pin, GPIO_PIN_SET);
    __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, PWM_MAX - duty);
  }
}
```

`speed` is −1000..+1000 (per mille), independent of the timer settings.

### Step 2: Pot controls the motor
Map the potentiometer (lesson 08) to −1000..+1000 with a dead zone in the middle (like lesson 09). Turn slowly from the centre.

**Find the dead band**: log the speed value where the motor *starts* turning, and where it *stops* when you go back down. Typically ~300–400 to start, lower to keep turning. That gap is static vs dynamic friction. Write your numbers down; lesson 17 needs them.

### Step 3: Count pulses (method A)
Each slot passing the sensor = one rising edge = one interrupt.

```c
#define SLOTS 20

static volatile uint32_t pulseCount;

void Motor_OnPulse(void)
{
  pulseCount++;
}
```

Every 100 ms in `Update()`:

```c
uint32_t count = pulseCount;
pulseCount = 0;
rpmCounting = count * 600 / SLOTS;
```

(`count` per 0.1 s × 10 × 60 s / slots per revolution.)

There's a tiny race: a pulse arriving between reading and zeroing is lost. Fix: disable interrupts for those two lines, or never reset and subtract the previous value instead (better, and lock-free).

### Step 4: Measure the period (method B)
Store the time between pulses in the interrupt, using `Micros()` from lesson 15:

```c
static volatile uint32_t lastPulseUs;
static volatile uint32_t periodUs;

void Motor_OnPulse(void)
{
  uint32_t now = Micros();
  uint32_t elapsed = now - lastPulseUs;
  if (elapsed < 300)
    return;
  periodUs = elapsed;
  lastPulseUs = now;
  pulseCount++;
}
```

```c
uint32_t RpmFromPeriod(void)
{
  if (Micros() - lastPulseUs > 200000 || periodUs == 0)
    return 0;
  return 60000000UL / (periodUs * SLOTS);
}
```

- The **300 µs filter** throws away glitches: slow edges through the comparator can trigger twice. At max speed, real pulses are > 10 ms apart, so nothing real is lost.
- The **200 ms timeout** detects a stopped motor (otherwise the last period stays forever).

### Step 5: Compare both methods
Log `speed, rpmCounting, rpmPeriod` every 100 ms over the console, and sweep the pot slowly.

- **Counting**: at 60 RPM with 20 slots you get only 2 pulses per 100 ms, so the resolution is **30 RPM**. Coarse at low speed, accurate at high speed.
- **Period**: smooth at low speed, an update per pulse, but noisier (one uneven slot changes the result).

Real systems pick by speed range, or use a hardware timer that does both. That's the encoder mode in lesson 18.

## Check it works
- [ ] Pot centre = stopped, one side forward, other side reverse
- [ ] No audible whine from the PWM
- [ ] Both RPM methods agree within a few % at medium speed
- [ ] RPM drops when you lightly brake the wheel with your finger
- [ ] Stopped motor shows 0 within 200 ms

## Troubleshooting
| Symptom | Cause |
|---|---|
| Motor doesn't turn | no 5 V on L9110, grounds not connected, below dead band |
| Board resets when motor starts | motor powered from the board, or no common ground + noisy wires |
| Turns only one way | `MOTOR_DIR` not toggling, or forgot the `PWM_MAX - duty` inversion |
| RPM doubled | comparator double-triggers: raise the glitch filter, or turn the module's pot |
| RPM always 0 | disk doesn't pass through the gap, or EXTI3 not enabled |

## Challenges
1. Use the L9110's **B channel** for a second motor, or the pump (lesson 18).
2. Count pulses in hardware: route PD3's signal into a timer's **external clock** input (ETR) so the timer counts edges with no interrupts at all.
3. **Ramp**: limit how fast speed can change (e.g. 500 per second), so the motor never jumps from full forward to full reverse. Why is that kind to the hardware?
