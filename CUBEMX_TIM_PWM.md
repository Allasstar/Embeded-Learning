# CubeMX: Timers and PWM

What every field on the CubeMX timer screen means, how the timer turns them into a PWM signal, and how to pick the values.

Running example: **84 MHz timer clock, 1 µs tick, 20 ms period (50 Hz)**.

---

## 1. Keywords

| Term | Meaning |
|---|---|
| **PWM** | Pulse-Width Modulation. A square wave on a pin: HIGH (3.3 V) or LOW (0 V), nothing in between. You set how often it repeats and how long it stays HIGH. |
| **Period** | Time of one full cycle, HIGH + LOW. Example: 20 ms. |
| **Frequency** | Cycles per second = 1 / period. 20 ms → 50 Hz. |
| **Pulse width** | Time the pin is HIGH in one cycle. Example: 1.5 ms. |
| **Duty cycle** | Pulse width / period, in %. 1.5 ms / 20 ms = 7.5 %. |
| **Timer (TIM)** | Hardware counter. Counts clock ticks by itself, the CPU is not involved. |
| **Timer clock** | Clock feeding the timer, before the prescaler. 84 MHz or 168 MHz on the F407 (section 8). |
| **Tick** | One step of the counter = (PSC + 1) / timer clock. |
| **PSC** | Prescaler register. Divides the timer clock → sets the tick. |
| **CNT** | Counter register. The live count: 0, 1, 2 … ARR, 0, 1 … |
| **ARR** | Auto-Reload Register. The top value of CNT → sets the period. |
| **CCR** | Capture/Compare Register, one per channel. The pin switches when CNT reaches it → sets the pulse width. |
| **Update event (UEV)** | The moment CNT wraps from ARR to 0. Buffered values are loaded here. |
| **Preload (shadow register)** | Buffer: you write a new value, hardware copies it into the active register at the next update event. A running cycle is never cut. |
| **Channel (CHx)** | One output of a timer. All channels of a timer share CNT / PSC / ARR (**same frequency**), each has its own CCR (**own pulse width**). |
| **Resolution** | Number of possible pulse widths = ARR + 1. |
| **AF (Alternate Function)** | Pin handed to a peripheral (here a timer channel) instead of plain GPIO. |
| **ETR** | External Trigger input. A timer pin that can clock the counter from outside (count external pulses) or reset/clear it. Only TIM1–5 and TIM8 have one. |

---

## 2. How a timer makes PWM

```
              ┌─────────────┐   tick    ┌──────────────────┐
timer clock ─►│  Prescaler  │──────────►│   Counter CNT    │  0 → ARR → 0 → ...
  84 MHz      │  ÷ (PSC+1)  │   1 µs    │                  │
              └─────────────┘           └────────┬─────────┘
                                                 │ compared on every tick
                                        ┌────────▼─────────┐
                                        │   CNT < CCR ?    │──► pin HIGH / LOW
                                        └──────────────────┘
```

PWM mode 1, polarity High:

```
CNT:      0        CCR                    0        CCR
          │         │                     │         │
3.3V      ┌─────────┐                     ┌─────────┐
          │         │                     │         │
0V  ──────┘         └─────────────────────┘         └─────
          ←── CCR ──→
          ←──────── ARR + 1 ticks ────────→
```

- CNT = 0 → pin goes HIGH
- CNT reaches CCR → pin goes LOW
- CNT passes ARR → wraps to 0, pin goes HIGH again

| CCR | Pin |
|---|---|
| `0` | always LOW (0 %) |
| between 0 and ARR | HIGH for CCR ticks |
| `> ARR` | always HIGH (100 %) |

---

## 3. The CubeMX screen, field by field

`Pinout & Configuration` → `Timers` → `TIMx`. First give the pin its function in the **Pinout view**: left-click the pin → `TIMx_CHy`. **Ctrl + click** a pin to see where else that channel can go.

The screen has two halves: **Mode** (top) and **Configuration** (bottom).

### 3.1 Mode (top half)

| Field | Typical PWM value | What it means |
|---|---|---|
| **Slave Mode** | `Disable` | Lets *another* signal control this timer's counter. `Disable` = the timer runs freely on its own clock. |
| **Trigger Source** | `Disable` | Which signal Slave Mode listens to. Only matters when Slave Mode is on. |
| **Internal Clock** ☑ / **Clock Source** ▾ | **checked** / `Internal Clock` | Feed the counter from the internal timer clock. **Not set = no clock = the timer never counts.** Checkbox or dropdown depends on the timer, see below. |
| **Channel1** | `PWM Generation CH1` | What channel 1 does (options below). |
| **Channel2** | `Disable` | Same for channel 2. Each channel is set separately. |
| **Combined Channels** | `Disable` (greyed) | Modes that use two channels together. Greyed when the channels it needs are already in use. |
| **One Pulse Mode** ☐ | unchecked | Checked = the counter **stops after one period**, so you get a single pulse. Unchecked = PWM repeats forever. |

**Internal Clock: checkbox or dropdown?**
It depends on whether the timer has an **ETR pin** (External Trigger input).

| Timers | Has ETR | CubeMX shows |
|---|---|---|
| TIM1, TIM2, TIM3, TIM4, TIM5, TIM8 | yes | **Clock Source** dropdown: `Disable` / `Internal Clock` / `ETR2` |
| TIM9 … TIM14 | no | **Internal Clock** checkbox (there's no other clock source to choose) |

**`ETR2`** = *External Clock Mode 2*: the counter counts edges on the **ETR pin** (e.g. `TIM1_ETR` = PE7) instead of the internal clock. Used to count external pulses (a flow sensor, an encoder wheel) or to run the timer from an outside clock. For PWM, pick `Internal Clock`.

Extra checkboxes on advanced timers (TIM1/TIM8):

| Checkbox | Meaning |
|---|---|
| Activate-Break-Input | A pin that **instantly forces all outputs off** (motor emergency stop, overcurrent). |
| Use ETR as Clearing Source | A signal on ETR forces the PWM output LOW for the rest of the cycle (current limiting). |
| XOR activation | XORs the CH1–CH3 inputs into one (hall sensors). Greyed until those channels are inputs. |

**Slave Mode** options:

| Option | Meaning |
|---|---|
| `Disable` | Free-running. **Normal for PWM.** |
| `External Clock Mode 1` | Count edges of the trigger signal instead of the internal clock (count pulses from a sensor). |
| `Reset Mode` | Trigger resets CNT to 0 (sync the cycle to an external event). |
| `Gated Mode` | Counter runs only while the trigger is HIGH. |
| `Trigger Mode` | Counter starts on the trigger edge. |

**Trigger Source** options:

| Option | Meaning |
|---|---|
| `ITR0`…`ITR3` | Internal trigger: the output of another timer. Chains timers. |
| `TI1_ED` | Any edge on the channel 1 pin. |
| `TI1FP1` / `TI2FP2` | Filtered input from the channel 1 / channel 2 pin. |

**Channel** options:

| Option | Meaning |
|---|---|
| `Disable` | Channel unused. |
| **`PWM Generation CHx`** | PWM on the pin. **The one for PWM.** |
| `PWM Generation No Output` | PWM inside the timer only (for interrupts / triggers). Pin is free. |
| `Output Compare CHx` | Pin toggles / sets / clears once when CNT reaches CCR. |
| `Output Compare No Output` | Same, pin unused (just a timed interrupt). |
| `Input Capture direct mode` | Measure an incoming signal: CNT is saved into CCR on an edge. |
| `Forced Output CHx` | Pin held HIGH or LOW by software. |

**Combined Channels** options (depend on the timer):

| Option | Meaning |
|---|---|
| `PWM Input on CH1` / `CH2` | Uses both channels to **measure** an incoming PWM: period and pulse width. |
| `Encoder Mode` | Read a rotary encoder (TIM1–5, TIM8 only). |
| `XOR ON / Hall Sensor Mode` | Read BLDC motor hall sensors (TIM1–5, TIM8 only). |

### 3.2 Configuration tabs (bottom half)

| Tab | What it is | For PWM |
|---|---|---|
| **Parameter Settings** | All counter and channel values. | This is where you work. |
| **User Constants** | Your own named constants. CubeMX writes them as `#define` into `main.h`, and you can type them into Parameter Settings fields instead of numbers. | Optional, e.g. `PWM_PERIOD = 20000`. |
| **NVIC Settings** | Timer interrupts (`TIMx global interrupt`). | Leave off. PWM needs no interrupt. Turn on only for a callback every period. |
| **GPIO Settings** | The pin's mode, pull, speed. Filled in automatically (AF push-pull, no pull, low speed). | Leave as is. Raise speed only for signals of several MHz. |

### 3.3 Parameter Settings → Counter Settings

| Field (exact CubeMX name) | Register | Example | Meaning |
|---|---|---|---|
| **Prescaler (PSC - 16 bits value)** | PSC | `84-1` | Divides the timer clock by PSC + 1. 84 MHz / 84 = 1 MHz → **1 tick = 1 µs**. "16 bits" = max 65535. |
| **Counter Mode** | CR1.DIR, CR1.CMS | `Up` | Direction of counting (options below). |
| **Counter Period (AutoReload Register - 16 bits value )** | ARR | `20000-1` | CNT counts 0 … ARR, then wraps. 20000 ticks × 1 µs = **20 ms period**. Max 65535 (32-bit on TIM2/TIM5). |
| **Internal Clock Division (CKD)** | CR1.CKD | `No Division` | Clock for the input filters and dead-time only. **Does not change counting speed.** Leave it. |
| **auto-reload preload** | CR1.ARPE | `Enable` | `Enable` = a new ARR waits for the next update event. Changing the period mid-cycle then can't produce one broken cycle. `Disable` = new ARR applies immediately. |
| Repetition Counter *(TIM1/TIM8 only)* | RCR | `0` | Update event only every RCR + 1 periods. |

**Counter Mode** options:

| Option | Counting | Frequency |
|---|---|---|
| **`Up`** | 0 → ARR, wrap to 0. **Normal.** | timer clock / ((PSC+1)(ARR+1)) |
| `Down` | ARR → 0, wrap to ARR. | same |
| `Center Aligned mode1/2/3` | 0 → ARR → 0. Pulses centred in the period (motor control). 1/2/3 only differ in when the compare flag is set. | **half** of Up |

### 3.4 Parameter Settings → PWM Generation Channel 1

One such block appears for every channel set to PWM.

| Field | Register | Example | Meaning |
|---|---|---|---|
| **Mode** | CCMR.OCxM | `PWM mode 1` | Rule for the pin level (table below). |
| **Pulse (16 bits value)** | CCRx | `0` | **Starting** compare value = starting pulse width in ticks. Change it later in code. `0` = pin stays LOW until your code sets a value. |
| **Output compare preload** | CCMR.OCxPE | `Enable` | `Enable` = a new Pulse value waits for the next update event, so a running pulse is never cut short. HAL forces it on for PWM anyway. |
| **Fast Mode** | CCMR.OCxFE | `Disable` | Shortens the delay from a trigger input to the output in One Pulse Mode. No effect on normal PWM. |
| **CH Polarity** | CCER.CCxP | `High` | Which level is "active". `High` = active is 3.3 V. `Low` = output inverted. |

**Mode** options:

| Option | CNT < CCR | CNT ≥ CCR |
|---|---|---|
| **`PWM mode 1`** | HIGH | LOW |
| `PWM mode 2` | LOW | HIGH |

Mode 2 and polarity `Low` both invert the signal. Use **PWM mode 1 + High**: then Pulse = HIGH time, easy to think about.

### 3.5 Trigger Output (TRGO) Parameters *(not on every timer)*

| Field | Example | Meaning |
|---|---|---|
| Master/Slave Mode (MSM bit) | `Disable` | Delay this timer so it starts exactly together with its slaves. |
| Trigger Event Selection | `Reset` | What this timer sends out to other timers or the ADC (`Update Event` = once per period, e.g. start an ADC reading). |

Not needed for plain PWM.

---

## 4. Formulas

```
tick         = (PSC + 1) / f_timer
period       = (ARR + 1) × tick
f_pwm        = f_timer / ((PSC + 1) × (ARR + 1))
pulse width  = CCR × tick
duty         = CCR / (ARR + 1)
resolution   = ARR + 1 steps
```

Example:
```
tick   = 84 / 84 MHz      = 1 µs
period = 20000 × 1 µs     = 20 ms  → 50 Hz
CCR    = 1500             → 1.5 ms pulse, 7.5 % duty
```

### Why "-1"
The counter starts **at 0**. ARR = 19999 → 0 … 19999 = **20000** ticks. PSC = 83 divides by **84**. CubeMX accepts expressions, so type `84-1` and `20000-1`: the real number stays readable.

---

## 5. Choosing PSC and ARR

1. Look up the **timer clock** (section 8).
2. Pick a **tick** that makes CCR easy (1 µs → CCR in µs).
3. `PSC = f_timer × tick − 1`
4. `ARR = period / tick − 1`
5. Both must be **≤ 65535** on 16-bit timers.

For a fixed frequency: **smaller PSC → bigger ARR → finer resolution**.

| Use | Need | f_timer | PSC | ARR | Result |
|---|---|---|---|---|---|
| Servo | 50 Hz, µs steps | 84 MHz | `84-1` | `20000-1` | 1 µs tick, 20 ms |
| LED dimming | ≥ 1 kHz, no flicker | 84 MHz | `84-1` | `1000-1` | 1 kHz, 1000 steps |
| DC motor | 20 kHz, silent | 84 MHz | `0` | `4200-1` | 20 kHz, 4200 steps |
| Buzzer | variable tone | 84 MHz | `84-1` | `1000000/f - 1` at runtime | CCR = (ARR+1)/2 → 50 % |

---

## 6. Code

### Start / stop
```c
HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1);   // channel output on + counter on
HAL_TIM_PWM_Stop(&htim12, TIM_CHANNEL_1);    // pin back to inactive level
```
CubeMX **configures** the timer but does **not start** it. Forgetting `HAL_TIM_PWM_Start` is the #1 "nothing happens" bug.

### Change values at runtime
| Macro | Register | Use |
|---|---|---|
| `__HAL_TIM_SET_COMPARE(&htim, TIM_CHANNEL_1, v)` | CCR1 = v | pulse width / duty |
| `__HAL_TIM_GET_COMPARE(&htim, TIM_CHANNEL_1)` | CCR1 | |
| `__HAL_TIM_SET_AUTORELOAD(&htim, v)` | ARR = v | period / frequency |
| `__HAL_TIM_GET_AUTORELOAD(&htim)` | ARR | |
| `__HAL_TIM_SET_PRESCALER(&htim, v)` | PSC = v | tick length (applies at next update) |
| `__HAL_TIM_GET_COUNTER(&htim)` | CNT | live count |
| `__HAL_TIM_SET_COUNTER(&htim, v)` | CNT = v | restart the cycle with `0` |

Each one is a macro that compiles to a single register access.

### Duty in percent
```c
uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim4);
__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, (arr + 1) * percent / 100);
```

### Debugger
Watch `htimX.Instance->CNT`, `->CCR1`, `->ARR`, `->PSC`. Type `htimX.Instance->CCR1 = 1500` in the Debug Console to change the pulse live.

---

## 7. Field → register → code

| CubeMX field | Register | `htimX.Init` / `sConfigOC` in `main.c` |
|---|---|---|
| Prescaler | PSC | `Init.Prescaler` |
| Counter Mode | CR1 | `Init.CounterMode` |
| Counter Period | ARR | `Init.Period` |
| Internal Clock Division | CR1.CKD | `Init.ClockDivision` |
| auto-reload preload | CR1.ARPE | `Init.AutoReloadPreload` |
| Internal Clock | SMCR | `sClockSourceConfig.ClockSource` |
| Mode (PWM mode 1) | CCMR.OCxM | `sConfigOC.OCMode` |
| Pulse | CCRx | `sConfigOC.Pulse` |
| Output compare preload | CCMR.OCxPE | set inside `HAL_TIM_PWM_ConfigChannel` |
| Fast Mode | CCMR.OCxFE | `sConfigOC.OCFastMode` |
| CH Polarity | CCER.CCxP | `sConfigOC.OCPolarity` |

You can find all of these in the generated `MX_TIMx_Init()`.

---

## 8. Timers on the STM32F407 (this project's clocks)

Clock Configuration tab → **APB1 Timer clocks** / **APB2 Timer clocks**. When an APB prescaler is not 1, its timers get **2 × the APB clock**.

| Timer | Bus | Timer clock | Bits | Channels | Notes |
|---|---|---|---|---|---|
| TIM1, TIM8 | APB2 | **168 MHz** | 16 | 4 + 3 inverted (CHxN) | Advanced: dead-time, break. Motors. |
| TIM2, TIM5 | APB1 | 84 MHz | **32** | 4 | Very long periods. |
| TIM3, TIM4 | APB1 | 84 MHz | 16 | 4 | TIM4 drives the on-board LEDs PD12–PD15. |
| TIM9 | APB2 | **168 MHz** | 16 | 2 | |
| TIM10, TIM11 | APB2 | **168 MHz** | 16 | 1 | |
| TIM12 | APB1 | 84 MHz | 16 | 2 | |
| TIM13, TIM14 | APB1 | 84 MHz | 16 | 1 | |
| TIM6, TIM7 | APB1 | 84 MHz | 16 | **0** | Basic: no pins, no PWM. |

Same `PSC = 84-1` on an APB2 timer gives a **0.5 µs** tick, so every pulse comes out half as long.

---

## 9. Troubleshooting

| Symptom | Cause |
|---|---|
| Pin always LOW | `HAL_TIM_PWM_Start` not called; Pulse/CCR = 0; *Internal Clock* unchecked |
| Pin always HIGH | CCR > ARR |
| Pulses half as long / frequency 2× | Timer on APB2 (168 MHz), maths assumed 84 MHz |
| Frequency half | Counter Mode is Center Aligned |
| Only one pulse, then nothing | *One Pulse Mode* checked |
| Wrong frequency, value seems to wrap | PSC or ARR > 65535 on a 16-bit timer |
| Slightly off | Forgot `-1` in PSC or ARR |
| One glitchy cycle when changing period | *auto-reload preload* = Disable |
| Timer counts, pin dead | Pin not set to `TIMx_CHy` in Pinout view |
| Channel option greyed | Pin used by something else, or the timer has fewer channels |
| LED flickers | PWM below ~200 Hz |
| Motor whines | PWM in hearing range, go ≥ 20 kHz |
