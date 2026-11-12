# Lesson 08: Analog Sensors (ADC)

## Goal
Read the potentiometer, then four analog channels at once in the background with DMA, filter the readings and convert them to useful units.

## What you learn
- How an ADC turns a voltage into a number
- Resolution, reference voltage, sampling time
- Polling vs scan mode + DMA (zero CPU work)
- Circular buffers in DMA
- Noise and simple filters
- Calibrating a sensor to real-world units

## How the ADC works
The ADC compares the input voltage against the reference (3.3 V on this board) and outputs a 12-bit number:

```
raw   = Vin / 3.3 V × 4095        (0..4095)
volts = raw × 3.3 / 4095
```

**Never put more than 3.3 V on an ADC pin.** ADC pins are not 5 V tolerant. Power every analog module from **3V**.

## Parts and wiring

| Module | Pin | Board pin | ADC channel |
|---|---|---|---|
| Potentiometer | VCC / GND / OUT | 3V / GND / PA1 | IN1 |
| Joystick | +5V / GND | **3V** / GND | |
| Joystick | VRx / VRy | PC1 / PC2 | IN11 / IN12 |
| Photoresistor | VCC / GND / AO | 3V / GND / PC4 | IN14 |

The joystick pin is labelled "+5V", but it's just two potentiometers. Power it from 3V so the outputs stay ≤ 3.3 V.

**MQ-2 gas sensor:** it needs 5 V for its heater and its AO can reach 5 V. Use only its **DO** pin with this board, or add a voltage divider (e.g. 10 kΩ + 20 kΩ) on AO.

## Part 1: One channel, polling

### CubeMX setup
Every ADC field is explained in [CUBEMX_ADC.md](../CUBEMX_ADC.md).

1. **PA1** → `ADC1_IN1` (potentiometer). This also ticks **IN1** under *Analog → ADC1*.
2. **Analog → ADC1 → Parameter Settings**:
   - Clock Prescaler: *PCLK2 divided by 4*
   - Resolution: *12 bits*
   - Data Alignment: *Right alignment*
   - Scan Conversion Mode: *Disabled*
   - Continuous Conversion Mode: *Disabled*
   - Number Of Conversion: `1`
   - Rank 1 → Channel: *Channel 1*, Sampling Time: *84 Cycles*
3. Generate.

#### Why each setting

Everything not listed stays at its default.

| Setting | Why |
|---|---|
| PA1 → `ADC1_IN1` | Connects the pin to the ADC's input multiplexer and switches it to analog mode (no digital input buffer, no pull resistors), so the pot's voltage reaches the ADC untouched. |
| Clock Prescaler = PCLK2 / 4 | PCLK2 is 84 MHz. The ADC is only specified up to 36 MHz, so /2 (42 MHz) is too fast. /4 gives 21 MHz. |
| Resolution = 12 bits | The full resolution: 4096 steps of ~0.8 mV. Lower resolutions convert faster, but nothing here needs speed. |
| Data Alignment = Right | The result lands in bits 0..11, so the value is just 0..4095 with no shifting. |
| Scan = Disabled, Number Of Conversion = 1 | Only one channel for now. |
| Continuous = Disabled | Each `HAL_ADC_Start` does exactly one conversion and stops. You decide when to measure. |
| Sampling Time = 84 Cycles | How long the ADC's internal capacitor is connected to the pin before converting. 84 cycles (4 µs) is enough for a 10 kΩ pot. Step 2 shows what happens with less and more. |

### Step 1: Read it

```c
extern ADC_HandleTypeDef hadc1;

static uint32_t ReadPot(void)
{
  HAL_ADC_Start(&hadc1);
  HAL_ADC_PollForConversion(&hadc1, 10);
  uint32_t raw = HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);
  return raw;
}
```

Every 200 ms, log `raw` and `raw * 3300 / 4095` (millivolts, integer maths). Turn the knob: 0..4095.

### Step 2: See the noise
Set the knob to the middle and leave it there. The last 1–2 digits wobble: that's noise (power supply, the long wires, the ADC itself). Write down the lowest and highest value over a few seconds.

Now change Rank 1's Sampling Time to *3 Cycles*, then *480 Cycles*, and do the same each time. Measured on this board:

| Sampling time | Range | Wobble |
|---|---|---|
| 3 cycles | 1991–2001 | 10 |
| 84 cycles | 1994–2006 | 12 |
| 480 cycles | 1995–2002 | 7 |

Almost no difference. Before each conversion, the ADC connects a small internal capacitor to the pin and lets it charge to the pin's voltage; the sampling time is how long that lasts. With **only one channel**, the capacitor still holds the voltage from the last conversion of the same pot, so there's almost nothing to charge, and even 3 cycles is enough. 480 cycles averages over a longer time and smooths a little fast noise, but most of the wobble comes from the supply and wiring, not the ADC. The filter in Step 4 deals with that.

Sampling time starts to matter when the ADC switches between channels. You'll see it in Step 5.

## Part 2: Four channels with DMA

Polling stops the CPU for each conversion. With **scan mode + DMA**, the ADC converts all channels in sequence, the DMA controller copies each result into an array, and then it starts over, forever, without the CPU. Your code just reads the array.

### CubeMX setup
Keep the Part 1 setup and add to it.

1. Pins (PA1 is already set):
   - **PC1** → `ADC1_IN11` (joystick VRx)
   - **PC2** → `ADC1_IN12` (joystick VRy)
   - **PC4** → `ADC1_IN14` (photoresistor AO)

   *Analog → ADC1* now shows **IN1, IN11, IN12, IN14** ticked.
2. **ADC1 → Parameter Settings** (change these):
   - Scan Conversion Mode: *Enabled*
   - Continuous Conversion Mode: *Enabled*
   - DMA Continuous Requests: *Enabled* (greyed out until you add the DMA in step 3; do step 3 first, then come back)
   - End Of Conversion Selection: *EOC flag at the end of all conversions*
   - Number Of Conversion: `4`
   - Ranks, all with Sampling Time *480 Cycles*:

     | Rank | Channel | Part |
     |---|---|---|
     | 1 | Channel 1 | Potentiometer |
     | 2 | Channel 11 | Joystick X |
     | 3 | Channel 12 | Joystick Y |
     | 4 | Channel 14 | Photoresistor |
3. **ADC1 → DMA Settings** tab → *Add* → `ADC1`:
   - Stream: *DMA2 Stream 0*
   - Direction: *Peripheral To Memory*
   - Mode: **Circular**
   - Increment Address: Peripheral ☐, Memory ☑
   - Data Width: *Half Word* / *Half Word*
4. **ADC1 → NVIC Settings**: *DMA2 stream0 global interrupt* is ticked automatically. Leave it on.
5. Generate. In `main.c` check that `MX_DMA_Init()` is called **before** `MX_ADC1_Init()`. (Some CubeMX versions get this wrong; if so, fix the order in *Project Manager → Advanced Settings*.)

#### Why each setting

**Pins**

| Setting | Why |
|---|---|
| PC1 / PC2 / PC4 → `ADC1_IN11` / `IN12` / `IN14` | Each sensor needs its own analog input. These are free pins on the header that ADC1 can reach. |

**ADC1 Parameters**

| Setting | Why |
|---|---|
| Scan = Enabled | One start converts every rank in order (1, 2, 3, 4) instead of only rank 1. |
| Continuous = Enabled | After rank 4 the ADC goes straight back to rank 1, forever. The array is always fresh without you starting anything. |
| DMA Continuous Requests = Enabled | The ADC keeps asking the DMA to collect results after the first full buffer. Disabled, it stops after one round, which is why the buffer would update only once. |
| EOC = end of all conversions | The "conversion done" flag means "whole scan done", not "one channel done". The DMA collects each result by itself, so the CPU doesn't need a flag per channel. |
| Number Of Conversion = 4 | Four channels in the sequence. Must equal `ANALOG_COUNT` in the code. |
| Rank order | Decides which array slot each channel lands in. The enum in `analog_in.h` must follow the same order. |
| Sampling Time = 480 Cycles | All channels share one sample capacitor inside the ADC. The photoresistor module has a high output resistance, so the capacitor charges slowly through it. If the sampling time is too short, part of the previous channel's voltage is still on the capacitor and leaks into this one. 480 cycles (23 µs) gives every sensor time to settle. |

**DMA**

| Setting | Why |
|---|---|
| DMA2 Stream 0 | ADC1 is hard-wired to DMA2 Stream 0 (or Stream 4) in the chip (RM0090, DMA2 request mapping). |
| Peripheral To Memory | Data goes from the ADC's result register into your array. |
| Mode = Circular | After the last array element the DMA wraps back to element 0 and keeps going. *Normal* would stop after one pass. |
| Increment: Peripheral off, Memory on | The ADC always has one result register, so the source address stays fixed. Each result goes into the next array slot, so the destination moves. |
| Data Width = Half Word | A 12-bit result fits in 16 bits, and the array is `uint16_t`. *Word* would write 32 bits per slot and overrun the array. |

**NVIC**

| Setting | Why |
|---|---|
| DMA2 stream0 global interrupt | HAL uses it to track the transfer (half-complete and complete). CubeMX forces it on with DMA. It's also what challenge 3 is about: it fires twice per scan, all the time. |

### Step 3: The ADC module

`Core/Inc/analog_in.h`:

```c
#pragma once
#include <stdint.h>

typedef enum
{
  ANALOG_POT,
  ANALOG_JOY_X,
  ANALOG_JOY_Y,
  ANALOG_LIGHT,
  ANALOG_COUNT
} AnalogInput;

void AnalogIn_Init(void);
void AnalogIn_Update(void);
uint16_t AnalogIn_Raw(AnalogInput input);
uint16_t AnalogIn_Filtered(AnalogInput input);
```

The enum order **must match the rank order** in CubeMX.

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
  {
    filteredX16[i] += dmaBuffer[i] - filteredX16[i] / 16;
  }
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

`dmaBuffer` is `volatile` because the DMA writes it behind the compiler's back, the same reason as variables shared with interrupts.

Hook it up:
1. Add `Core/Src/analog_in.c` to `target_sources` in `CMakeLists.txt`.
2. `App_Start`: call `AnalogIn_Init()` once.
3. `App_Update`: call `AnalogIn_Update()` every loop, outside any timer.
4. Every 200 ms, log the raw value of all four channels on one line.
5. Delete `ReadPot()`. The ADC now belongs to the DMA. Polling it returns `HAL_ERROR`, and the `HAL_ADC_Stop()` inside it would switch the ADC off.

Turn each knob, move the joystick and cover the light sensor. Only its own number should change.

### Step 4: The filter
`filteredX16 += raw - filteredX16 / 16` is an **exponential moving average** with integer maths. Each new sample moves the result 1/16 of the way toward it. It's stored multiplied by 16 so you don't lose the fractional part. It needs only one variable per channel (an average of the last N samples needs N). Bigger divisor = smoother but slower to react.

Log raw vs filtered for the pot and compare.

Note: for the first ~50 ms after start-up the filtered value is still climbing from 0. If that matters, add `HAL_Delay(5)` after starting the DMA in `Init` and pre-fill `filteredX16[i] = dmaBuffer[i] * 16`.

### Step 5: Channels leaking into each other
Now the capacitor goes pot → joystick X → joystick Y → light → pot, one after another. Each time it starts from the previous channel's voltage.

1. Keep the light on the photoresistor steady (don't move your hand near it). Log all four raw values.
2. Push the joystick fully up and down (the Y axis). The light channel (rank 4) is converted right after joystick Y (rank 3). With *480 Cycles*, the light value stays put.
3. Set every rank to *3 Cycles*, regenerate, and repeat. The light value now follows the joystick a little, because the capacitor is still partly at joystick Y's voltage when the light channel is converted.

The photoresistor module has a high output resistance, so it charges the capacitor slowly and suffers most. That's why Part 2 uses 480 cycles for every channel. Set them back to 480 afterwards.

### Step 6: Calibrate to real units

**Light %:** the photoresistor module's AO usually **rises in the dark**.
1. Log the filtered light value **covered with your hand**: e.g. `dark = 3600`.
2. **Under a lamp**: e.g. `bright = 400`.
3. `percent = (dark - raw) * 100 / (dark - bright)`, clamped to 0..100.

The values differ per module, so measure yours. Write it as a small function, e.g. `uint32_t Light_Percent(void)`.

## Check it works
- [ ] Pot gives 0 at one end and ~4095 at the other
- [ ] Joystick centre is ~2000 on both axes and full travel reaches the ends
- [ ] Filtered values are stable when nothing moves
- [ ] Light reads ~0% covered, ~100% under a lamp
- [ ] The CPU does nothing for the ADC except reading the array

## Troubleshooting
| Symptom | Cause |
|---|---|
| All values 0 | `HAL_ADC_Start_DMA` not called, or DMA initialised after ADC |
| Values in the wrong slots | enum order ≠ rank order |
| Only the first value updates | Scan mode disabled, or Number Of Conversion = 1 |
| Updates once then stops | DMA mode *Normal* instead of *Circular*, or DMA Continuous Requests disabled |
| Max is ~2800 instead of 4095 | module powered from 3V but pot isn't rail-to-rail: normal for some modules |
| Neighbouring channels affect each other | sampling time too short for high-resistance sensors |

## Challenges
1. Show the pot position as LED brightness (lesson 04).
2. Measure the chip's **internal temperature sensor** (ADC1 channel *Temperature Sensor*, see RM0090 for the formula).
3. This setup converts nonstop: ~21 000 DMA interrupts per second for data you read at 1 kHz. Make **TIM8** trigger the conversions instead (External Trigger Conversion Source, Continuous *Disabled*) at 1 kHz.
4. Add the **water level** sensor as a 5th channel on PB1 (IN9). What changes in the code?
