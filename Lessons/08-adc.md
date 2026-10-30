# Lesson 08: Analog Sensors (ADC)

## Goal
Read the potentiometer, then five analog sensors at once in the background with DMA, filter the readings and convert them to useful units.

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
| Soil Moisture (comparator board) | VCC / GND / AO | 3V / GND / PC5 | IN15 |

The joystick pin is labelled "+5V", but it's just two potentiometers. Power it from 3V so the outputs stay ≤ 3.3 V.

**MQ-2 gas sensor:** it needs 5 V for its heater and its AO can reach 5 V. Use only its **DO** pin with this board, or add a voltage divider (e.g. 10 kΩ + 20 kΩ) on AO.

## Part 1: One channel, polling

### CubeMX setup
1. **PA1** → `ADC1_IN1`.
2. **Analog → ADC1**: IN1 ticked. Parameters: Resolution *12 bits*, Scan *Disabled*, Continuous *Disabled*, Rank 1 → Channel 1, Sampling Time *84 Cycles*.
3. Generate.

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
Keep the knob still. The last 1–2 digits wobble: that's noise (power supply, the long wires, the ADC itself). Try sampling times of *3 cycles* and *480 cycles*. Longer sampling lets the ADC's internal capacitor charge fully through the sensor's resistance: less noise, slower conversion.

## Part 2: Five channels with DMA

Polling stops the CPU for each conversion. With **scan mode + DMA**, the ADC converts all channels in sequence, the DMA controller copies each result into an array, and then it starts over, forever, without the CPU. Your code just reads the array.

### CubeMX setup
1. Also tick **IN11 (PC1), IN12 (PC2), IN14 (PC4), IN15 (PC5)**.
2. ADC1 Parameters:
   - Scan Conversion Mode: *Enabled*
   - Continuous Conversion Mode: *Enabled*
   - DMA Continuous Requests: *Enabled*
   - End Of Conversion Selection: *EOC flag at the end of all conversions*
   - Number Of Conversion: `5`
   - Rank 1 → Channel 1, Rank 2 → Channel 11, Rank 3 → Channel 12, Rank 4 → Channel 14, Rank 5 → Channel 15. Sampling Time *480 Cycles* for all.
3. **DMA Settings** tab → Add → `ADC1`. Stream *DMA2 Stream 0*, Mode **Circular**, Data Width *Half Word* / *Half Word*.
4. Generate. In `main.c` check that `MX_DMA_Init()` is called **before** `MX_ADC1_Init()`. (Some CubeMX versions get this wrong; if so, fix the order in *Project Manager → Advanced Settings*.)

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
  ANALOG_SOIL,
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

### Step 4: The filter
`filteredX16 += raw - filteredX16 / 16` is an **exponential moving average** with integer maths. Each new sample moves the result 1/16 of the way toward it. It's stored multiplied by 16 so you don't lose the fractional part. It needs only one variable per channel (an average of the last N samples needs N). Bigger divisor = smoother but slower to react.

Log raw vs filtered for the pot and compare.

Note: for the first ~50 ms after start-up the filtered value is still climbing from 0. If that matters, add `HAL_Delay(5)` after starting the DMA in `Init` and pre-fill `filteredX16[i] = dmaBuffer[i] * 16`.

### Step 5: Calibrate to real units

**Soil moisture %:**
1. Log the filtered soil value with the probe in **dry air**: e.g. `dry = 3600`.
2. Probe in a **glass of water**: e.g. `wet = 1400`.
3. `percent = (dry - raw) * 100 / (dry - wet)`, clamped to 0..100.

**Light:** the photoresistor module's AO usually **rises in the dark**. Record "covered with your hand" and "under a lamp" values the same way and map them to 0..100%.

Write these as small functions, e.g. `uint32_t Soil_Percent(void)`.

## Check it works
- [ ] Pot gives 0 at one end and ~4095 at the other
- [ ] Joystick centre is ~2000 on both axes and full travel reaches the ends
- [ ] Filtered values are stable when nothing moves
- [ ] Soil reads ~0% in air, ~100% in water
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
3. This setup converts nonstop: ~17 000 DMA interrupts per second for data you read at 1 kHz. Make **TIM8** trigger the conversions instead (External Trigger Conversion Source, Continuous *Disabled*) at 1 kHz.
4. Add the **water level** sensor as a 6th channel on PB1 (IN9). What changes in the code?
