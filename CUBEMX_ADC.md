# CubeMX: ADC

What every field on the CubeMX ADC screen means, how the ADC turns a voltage into a number, and the three ways to read it in code.

Running example: lesson 08. **Potentiometer on PA1 (ADC1_IN1)**, then four channels with DMA. ADC clock **21 MHz**.

---

## 1. Keywords

| Term | Meaning |
|---|---|
| **ADC** | Analog-to-Digital Converter. Measures a voltage on a pin and gives a number. The F407 has three: ADC1, ADC2, ADC3. Use **ADC1** unless you need more. |
| **Resolution** | How many steps the result has. 12 bits = 4096 steps, 0..4095. |
| **LSB** | One step. At 12 bits and 3.3 V: 3.3 V / 4096 ≈ **0.8 mV**. |
| **VREF+ / VDDA** | The voltage the ADC compares against. On this chip VREF+ is tied to the supply. A pin at VREF+ reads 4095. |
| **Channel (INx)** | One analog input of the ADC. Each channel is fixed to one pin: IN1 = PA1, IN11 = PC1, and so on (section 3). |
| **Regular group** | The list of channels the ADC converts in normal operation. Up to 16 entries. |
| **Rank** | A position in the regular group. Rank 1 is converted first, then rank 2… The rank decides the order, not the channel number. |
| **Injected group** | A second, high-priority list (up to 4) that can interrupt the regular group. Not used in these lessons. |
| **Sample-and-hold capacitor** | A tiny capacitor (~4 pF) inside the ADC. It's connected to the pin, charges to the pin's voltage, then gets disconnected and measured. |
| **Sampling time** | How long the capacitor stays connected to the pin. Set in cycles. |
| **Conversion time** | Time to turn the held voltage into bits. Fixed: 12 cycles at 12-bit. |
| **Cycle** | One tick of the ADC clock. 21 MHz → 1 cycle ≈ 48 ns. |
| **ADC clock (ADCCLK)** | PCLK2 (84 MHz) divided by the Clock Prescaler. Max 36 MHz. |
| **Scan mode** | Convert every rank of the regular group in a row, instead of only rank 1. |
| **Continuous mode** | When the sequence ends, start it again by itself, forever. |
| **Discontinuous mode** | Convert the sequence in small chunks, one chunk per trigger. Rarely needed. |
| **EOC** | End Of Conversion flag. Set when a result is ready in DR. |
| **DR** | Data Register. Holds the latest result. One register for the whole ADC, so each new result overwrites the last. |
| **Overrun (OVR)** | A new result arrived before the old one was read from DR. The old one is lost. |
| **DMA** | Direct Memory Access. Hardware that copies data (here: DR → your array) without the CPU. |
| **Circular (DMA)** | After the last array element, the DMA starts again at element 0. |
| **Source impedance** | The resistance "seen" looking back into the sensor's output. High impedance charges the capacitor slowly, so it needs a longer sampling time. |

---

## 2. How the ADC measures

```
 PA1 ──┐
 PC1 ──┤   ┌─────────────┐   switch   ┌───────────┐   ┌──────────────┐   ┌────┐
 PC2 ──┼──►│ multiplexer │────/ ─────►│ capacitor │──►│ SAR: 12 bits │──►│ DR │──► CPU or DMA
 PC4 ──┤   │ (picks the  │            │  (~4 pF)  │   │ one by one   │   └────┘
 ...  ─┘   │   channel)  │            └───────────┘   └──────────────┘
           └─────────────┘
                 ◄── sampling time ──►◄──── 12 cycles ────►
```

One conversion:

1. **Select.** The multiplexer connects the channel of the current rank.
2. **Sample.** The switch closes. The capacitor charges toward the pin's voltage for the *sampling time*.
3. **Hold.** The switch opens. The capacitor keeps the voltage, even if the pin changes now.
4. **Convert.** The SAR (Successive Approximation Register) finds the result one bit at a time. First: "is it above half of VREF?" → bit 11. Then "above ¾ or ¼?" → bit 10. Twelve questions = 12 bits = 12 cycles.
5. **Result.** The number goes into DR and EOC is set.

```
raw   = Vin / VREF × 4095
Vin   = raw × VREF / 4095
```

**Never put more than VDDA (≈ 3.3 V) on an ADC pin**, not even through a sensor. Analog pins aren't 5 V tolerant. Power analog modules from **3V**, or use a voltage divider.

---

## 3. Pins on this board

ADC channels are fixed to pins. "ADC123_IN1" in the datasheet means ADC1, ADC2 and ADC3 can all read it.

| Channel | Pin | On the Discovery board |
|---|---|---|
| IN0 | PA0 | ❌ blue user button |
| **IN1** | **PA1** | ✅ free (lesson 08: pot) |
| IN2 / IN3 | PA2 / PA3 | ✅ free, unless USART2 is used (lesson 10) |
| IN4–IN7 | PA4–PA7 | ❌ audio DAC (PA4), accelerometer SPI (PA5–PA7) |
| **IN8** | **PB0** | ✅ free (lesson 04 uses it for the RGB LED) |
| **IN9** | **PB1** | ✅ free |
| IN10 | PC0 | ❌ USB power switch |
| **IN11 / IN12** | **PC1 / PC2** | ✅ free (lesson 08: joystick) |
| IN13 | PC3 | ❌ microphone |
| **IN14 / IN15** | **PC4 / PC5** | ✅ free (lesson 08: light; lesson 18: soil) |
| IN16 | internal | Chip temperature sensor (ADC1 only) |
| IN17 | internal | VREFINT, a fixed ~1.21 V reference (ADC1 only) |
| IN18 | internal | VBAT / 2, the backup battery pin (ADC1 only) |

In the **Pinout view**, left-click a pin → `ADC1_INx`. That also ticks INx under *Analog → ADC1*. Ticking INx there sets the pin. Both ways do the same thing.

---

## 4. The CubeMX screen, field by field

`Pinout & Configuration` → `Analog` → `ADC1`.

### 4.1 Mode (top half)

| Field | Meaning |
|---|---|
| **IN0 … IN15** ☑ | Turns on that channel and puts its pin into analog mode. |
| **Temperature Sensor Channel** ☑ | Internal channel 16. |
| **Vrefint Channel** ☑ | Internal channel 17. Used to measure the real VDDA (section 7). |
| **Vbat Channel** ☑ | Internal channel 18. |

Ticking a channel only makes it *available*. It's converted only when a **Rank** points to it (4.3).

### 4.2 Configuration tabs (bottom half)

| Tab | What's there |
|---|---|
| **Parameter Settings** | Everything below. |
| **NVIC Settings** | *ADC1, ADC2 and ADC3 global interrupts*. Only needed for `HAL_ADC_Start_IT`. |
| **DMA Settings** | Add a DMA stream so results go to memory automatically. |
| **GPIO Settings** | Shows the pins in analog mode. Nothing to change. |
| **User Constants** | Your own `#define`s. Not needed. |

### 4.3 Parameter Settings

**ADCs_Common_Settings**

| Field | Value | Meaning |
|---|---|---|
| Mode | `Independent mode` | Each ADC works alone. The *Dual / Triple* modes let ADC1+2(+3) sample at the same moment or take turns for speed. |

**ADC_Settings**

| Field | Lesson value | Meaning |
|---|---|---|
| **Clock Prescaler** | `PCLK2 divided by 4` | ADC clock = 84 MHz / 4 = **21 MHz**. /2 gives 42 MHz, which is above the 36 MHz maximum. |
| **Resolution** | `12 bits` | 12 / 10 / 8 / 6 bits. Fewer bits = fewer conversion cycles (12 / 10 / 8 / 6), but coarser steps. |
| **Data Alignment** | `Right alignment` | Result in bits 0..11, so DR reads 0..4095 directly. *Left* puts it in bits 4..15 (0..65520 in steps of 16). |
| **Scan Conversion Mode** | Polling: `Disabled`. DMA: `Enabled` | Disabled = only rank 1. Enabled = all ranks in order. |
| **Continuous Conversion Mode** | Polling: `Disabled`. DMA: `Enabled` | Disabled = one sequence per start. Enabled = repeat forever. |
| **Discontinuous Conversion Mode** | `Disabled` | Split the sequence into chunks of N ranks, one chunk per trigger. |
| **DMA Continuous Requests** | Polling: `Disabled`. DMA: `Enabled` | Enabled = keep sending results to DMA forever (needed for circular DMA). Disabled = stop after the DMA buffer is full once. |
| **End Of Conversion Selection** | Polling: single. DMA: all | *EOC flag at the end of single channel conversion*: EOC after every rank. *…at the end of all conversions*: EOC once per full sequence. |

**ADC_Regular_ConversionMode**

| Field | Lesson value | Meaning |
|---|---|---|
| **Number Of Conversion** | `1`, later `4` | How many ranks are in the sequence. More ranks appear below when you raise it. |
| **External Trigger Conversion Source** | `Regular Conversion launched by software` | What starts a conversion. Software = `HAL_ADC_Start`. Other choices: a timer event (e.g. *Timer 8 Trigger Out event*) to sample at an exact rate. |
| **External Trigger Conversion Edge** | `None` | Rising / falling / both edges of the trigger. Greyed when the trigger is software. |
| **Rank N → Channel** | `Channel 1` … | Which channel is converted at this position. The same channel may appear in several ranks. |
| **Rank N → Sampling Time** | `84 Cycles`, later `480` | How long the capacitor charges for this rank (section 5). |

**ADC_Injected_ConversionMode**: Number Of Conversions `0`. Not used.

**WatchDog**: *Enable Analog WatchDog Mode* ☐. When on, the ADC raises an interrupt by itself if a channel goes outside a low/high window. Handy for "alarm when the value goes above X" without checking it in code.

---

## 5. Sampling time

Choices: **3, 15, 28, 56, 84, 112, 144, 480** cycles.

| Sampling | + conversion | Total | Time at 21 MHz | Max rate, one channel |
|---|---|---|---|---|
| 3 | 12 | 15 | 0.71 µs | 1.4 MS/s |
| 15 | 12 | 27 | 1.3 µs | 780 kS/s |
| 84 | 12 | 96 | 4.6 µs | 220 kS/s |
| 144 | 12 | 156 | 7.4 µs | 135 kS/s |
| 480 | 12 | 492 | 23 µs | 43 kS/s |

**What it does.** The capacitor charges through the sensor's output resistance. Higher resistance = slower charging. If the switch opens too early, the capacitor hasn't reached the pin's voltage and the result is wrong.

**When it matters:**

- **One channel only.** Barely matters. The capacitor already holds almost the same voltage from the last conversion of the same pin. Measured in lesson 08 with the pot in the middle (wobble over a few seconds): 3 cycles → 10, 84 → 12, 480 → 7.
- **Several channels (scan).** Matters a lot. The capacitor jumps from one channel's voltage to the next. Too short, and part of the previous channel leaks into this one (**crosstalk**).
- **High-impedance sources** (photoresistor modules, voltage dividers with large resistors, the internal temperature sensor) always need long sampling times. The temperature sensor needs **≥ 10 µs**, so use 480 cycles.

**Rule of thumb:** use **480 cycles** unless you need speed. These lessons read sensors at 1 kHz at most, and 480 cycles still gives 40 000+ samples per second.

**What it doesn't fix:** noise from the supply, long wires and the ADC itself. A longer sampling time averages a little of it. For the rest, use a filter (lesson 08, Step 4).

---

## 6. Formulas

```
ADCCLK          = PCLK2 / prescaler                     = 84 MHz / 4 = 21 MHz
1 cycle         = 1 / ADCCLK                            ≈ 48 ns
t_conversion    = (sampling + 12) / ADCCLK              (12-bit)
t_sequence      = sum of t_conversion of every rank     (scan mode)
sequences / s   = 1 / t_sequence                        (continuous mode)

millivolts      = raw × VDDA_mV / 4095
```

Example, lesson 08 Part 2: 4 ranks × 492 cycles = 1968 cycles = **93.7 µs** per sequence → **10 670 sequences per second**. Circular DMA raises a *half-complete* and a *complete* interrupt per pass → **~21 000 interrupts per second**.

Use integer maths for millivolts: `raw * 3300 / 4095` fits easily in `uint32_t` (max 13.5 million).

---

## 7. Converting to real units

**Volts.** `mV = raw × VDDA / 4095`. The catch: VDDA on the Discovery board can be closer to **3.0 V** than 3.3 V. Measure the **3V** pin with a multimeter, or let the chip measure it:

**VREFINT trick.** Channel 17 is an internal reference of about 1.21 V. ST measured it on every chip at the factory with VDDA = exactly 3.3 V and stored the raw value in flash (`VREFINT_CAL`, address `0x1FFF7A2A`). Read channel 17 now and compare:

```c
uint32_t vddaMv = 3300 * (*VREFINT_CAL_ADDR) / vrefintRaw;
```

`VREFINT_CAL_ADDR` is defined in `stm32f4xx_ll_adc.h`. The LL macro `__LL_ADC_CALC_VREFANALOG_VOLTAGE(vrefintRaw, LL_ADC_RESOLUTION_12B)` does the same.

**Percent / physical units.** Measure two known points and draw a straight line between them (lesson 08, Step 6):

```
percent = (raw - low) × 100 / (high - low)      clamped to 0..100
```

Swap `low` and `high` if the sensor's output falls as the quantity rises (photoresistor modules, soil probes).

---

## 8. Reading in code

`hadc1` is created by CubeMX in `main.c`. In your own files: `extern ADC_HandleTypeDef hadc1;`.

### 8.1 Polling: one value, the CPU waits

CubeMX: Scan *Disabled*, Continuous *Disabled*, 1 rank.

```c
HAL_ADC_Start(&hadc1);                    // start one conversion, returns at once
HAL_ADC_PollForConversion(&hadc1, 10);    // wait for EOC, at most 10 ms
uint32_t raw = HAL_ADC_GetValue(&hadc1);  // read DR
HAL_ADC_Stop(&hadc1);                     // switch the ADC off again
```

- **Timeout** (`10`) is in **milliseconds**, based on `HAL_GetTick()`. A conversion takes ~5 µs, so 10 ms is only a safety net in case the ADC is broken. Don't use `1`: the tick may change right after the call and expire it almost at once.
- `PollForConversion` returns `HAL_OK` or `HAL_TIMEOUT`. On a timeout, `GetValue` returns the old value.
- **Several channels by polling:** call `HAL_ADC_ConfigChannel` with a new channel before each `Start`. That works, but it's slow and clumsy. Use DMA instead.

Good for: one sensor, read now and then.

### 8.2 Interrupt: the CPU does other work meanwhile

CubeMX: same as polling, plus **NVIC → ADC1, ADC2 and ADC3 global interrupts** ☑.

```c
HAL_ADC_Start_IT(&hadc1);

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  lastRaw = HAL_ADC_GetValue(hadc);       // lastRaw: static volatile uint32_t
}
```

The callback runs in the interrupt when EOC is set. With Continuous *Enabled* it fires after every conversion, which is far too often at short sampling times. Good for: one channel, timer-triggered.

### 8.3 Scan + circular DMA: everything in the background

CubeMX: Scan, Continuous and DMA Continuous Requests *Enabled*, N ranks, DMA stream **circular**, **Half Word**. The full setup is in lesson 08 Part 2.

```c
static volatile uint16_t buffer[4];       // one slot per rank, in rank order

HAL_ADC_Start_DMA(&hadc1, (uint32_t *)buffer, 4);   // once, at start-up

uint16_t pot = buffer[0];                 // any time later: always the latest value
```

- `buffer` is `volatile` because the DMA writes it behind the compiler's back.
- The `(uint32_t *)` cast is only there because HAL's function signature says so. The DMA writes 16-bit values because Data Width is *Half Word*.
- Length = number of ranks (or a multiple, to keep several sequences).
- Optional callbacks: `HAL_ADC_ConvHalfCpltCallback` and `HAL_ADC_ConvCpltCallback` fire at the half and end of the buffer. Use them to process a big buffer half by half while the DMA fills the other half.
- `MX_DMA_Init()` must run **before** `MX_ADC1_Init()` in `main.c`.

Good for: several sensors, read whenever you like, zero CPU cost.

### 8.4 Timer-triggered: exact sample rate

Set **External Trigger Conversion Source** to a timer's TRGO (e.g. *Timer 8 Trigger Out event*), Edge *Rising*, Continuous *Disabled*. Set the timer's *Trigger Event Selection* to *Update Event*. Start the timer, then `HAL_ADC_Start_DMA`. Each timer period converts one sequence. Good for: audio, signal analysis, or just not converting 10 000 times a second when you need 100.

### Which one?

| Need | Use |
|---|---|
| One sensor, now and then | Polling |
| One sensor, CPU must not wait | Interrupt |
| Several sensors, always fresh | **Scan + circular DMA** |
| Exact sample rate | Timer trigger + DMA |

---

## 9. Field → register → code

What `MX_ADC1_Init()` writes, so you can find a setting in the generated code or in RM0090 chapter 13.

| CubeMX field | `hadc1.Init.` / `sConfig.` | Register |
|---|---|---|
| Clock Prescaler | `ClockPrescaler` | ADC_CCR.ADCPRE |
| Resolution | `Resolution` | ADC_CR1.RES |
| Scan Conversion Mode | `ScanConvMode` | ADC_CR1.SCAN |
| Discontinuous Mode | `DiscontinuousConvMode` | ADC_CR1.DISCEN |
| Continuous Mode | `ContinuousConvMode` | ADC_CR2.CONT |
| Data Alignment | `DataAlign` | ADC_CR2.ALIGN |
| DMA Continuous Requests | `DMAContinuousRequests` | ADC_CR2.DDS |
| End Of Conversion Selection | `EOCSelection` | ADC_CR2.EOCS |
| External Trigger Source / Edge | `ExternalTrigConv` / `ExternalTrigConvEdge` | ADC_CR2.EXTSEL / EXTEN |
| Number Of Conversion | `NbrOfConversion` | ADC_SQR1.L |
| Rank → Channel | `sConfig.Channel`, `sConfig.Rank` | ADC_SQR1..3 |
| Sampling Time | `sConfig.SamplingTime` | ADC_SMPR1 / SMPR2 |
| (result) | `HAL_ADC_GetValue` | ADC_DR |

**Debugger:** in the peripheral view, `ADC1 → DR` shows the last result, and `ADC1 → SR` shows the flags (EOC, OVR).

---

## 10. Troubleshooting

| Symptom | Cause |
|---|---|
| Always 0 | Wire to GND or broken; channel not in any rank; `HAL_ADC_Start` / `Start_DMA` not called |
| Always 4095 | Pin at or above VDDA, e.g. sensor powered from 5V (**unplug it, it can damage the pin**) |
| Random values that drift around | Nothing connected: a floating analog pin reads noise |
| Last digits wobble by 5–15 counts | Normal noise. Filter it (lesson 08, Step 4) |
| Max is ~3700–3800, not 4095 | Pot or module isn't rail-to-rail, or the sensor's supply is lower than VDDA |
| Millivolts ~10 % too high | VDDA is ~3.0 V, not 3.3 V (section 7) |
| One channel follows another | Sampling time too short for scan mode (section 5) |
| Values in the wrong array slots | Array order ≠ rank order |
| Only `buffer[0]` updates | Scan mode *Disabled*, or Number Of Conversion = 1 |
| DMA buffer updates once, then freezes | DMA mode *Normal*, or DMA Continuous Requests *Disabled* |
| DMA buffer all zeros | `MX_DMA_Init()` runs after `MX_ADC1_Init()` |
| `PollForConversion` returns `HAL_TIMEOUT` | `HAL_ADC_Start` not called first |
| `PollForConversion` returns `HAL_ERROR` | Polling an ADC that `HAL_ADC_Start_DMA` already started. Read the DMA buffer instead |
| Overrun (OVR flag set) | Continuous mode without DMA: DR isn't read before the next result arrives |
| Pin can't be set to `ADC1_INx` | Pin used by something else in CubeMX (section 3) |
