# CubeMX: GPIO and NVIC settings

How to set up a pin and its interrupt in CubeMX, and what each setting means. Example used throughout: the button on **PE4** (interrupt) and the buzzer on **PE5** (output) from lesson 02.

---

## 1. Pinout view: choosing what a pin does

The chip drawing on the right of **Pinout & Configuration**. Every pin starts grey (unused).

| Action | How |
|---|---|
| Choose a pin's function | **Left-click** the pin → pick from the list (`GPIO_Input`, `GPIO_Output`, `GPIO_EXTI4`, `SPI1_MOSI`…) |
| Set a user label | **Right-click** the pin → *Enter User Label* (or type it in the GPIO table, see below) |
| Clear a pin | Left-click it → *Reset_State* |
| See alternate pins for a signal | Hold **Ctrl** + left-click the pin; the possible alternatives blink |
| Find a pin | Search box under the chip |

A pin turns **green** once it has a function.

### Which function to pick

| You want | Pick | Label example |
|---|---|---|
| Read a pin in code (`HAL_GPIO_ReadPin`) | `GPIO_Input` | `BTN` |
| Drive a pin (LED, buzzer) | `GPIO_Output` | `BUZZER` |
| Get an interrupt on an edge | `GPIO_EXTIx` (x = pin number: PE4 → `GPIO_EXTI4`) | `BTN` |

**The function decides which modes the GPIO table offers.** On a `GPIO_Input` pin the *GPIO mode* dropdown has only *Input mode*. The interrupt modes appear only after you change the pin to `GPIO_EXTIx` in the Pinout view. This is the most common "why can't I find it" trap.

---

## 2. System Core → GPIO

Select a pin in the table. Its settings appear in the box below the table.

### GPIO mode

| Mode | Meaning |
|---|---|
| **Input mode** | Pin only reads voltage. Poll it with `HAL_GPIO_ReadPin`. |
| **Output Push Pull** | Pin drives HIGH (3.3 V) and LOW (0 V). Normal for LEDs, buzzers. |
| **Output Open Drain** | Pin can only pull LOW; HIGH needs a pull-up. Used for shared lines (I2C). |
| **External Interrupt Mode with Rising edge** | Interrupt when the pin goes 0 → 1. |
| **External Interrupt Mode with Falling edge** | Interrupt when the pin goes 1 → 0. |
| **External Interrupt Mode with Rising/Falling edge** | Interrupt on both edges. |
| **External Event Mode …** | Wakes the chip or triggers hardware **without** running an interrupt handler. Not what you want for callbacks. Don't pick it. |

### GPIO Pull-up / Pull-down

| Setting | Effect |
|---|---|
| **No pull-up and no pull-down** | Pin is floating unless something external drives it. |
| **Pull-up** | Internal ~40 kΩ resistor to 3.3 V. Idle reads `1`. |
| **Pull-down** | Internal ~40 kΩ resistor to GND. Idle reads `0`. |

A floating input reads random 0/1 (noise). If your module has no resistor of its own, set the pull yourself. For a button that connects the pin to GND when pressed (as in lesson 02): **Pull-up**, and the pin reads `1` released / `0` pressed.

Pull settings only matter for inputs. Leave outputs on *No pull*.

### Which edge?

Match the edge to what you measured in Step 0:

| Pull | Released | Pressed | Press is a… | Pick |
|---|---|---|---|---|
| Pull-up | 1 | 0 | falling edge | **Falling** |
| Pull-down | 0 | 1 | rising edge | **Rising** |

### Output-only settings

Only shown for outputs.

| Setting | Meaning |
|---|---|
| **GPIO output level** | State right after init (`Low` / `High`). Matters if a buzzer or relay must be silent at boot. |
| **Maximum output speed** | Edge rate. `Low` is fine for LEDs and buzzers; it also reduces noise. |

### User Label

The name becomes `main.h` defines:

```c
#define BTN_Pin        GPIO_PIN_4
#define BTN_GPIO_Port  GPIOE
```

**No label → no defines → your code won't compile.** The pin shows the label in the Pinout view; if it still shows `GPIO_EXTI4`, the label is empty.

> **Warning:** changing a pin's function in the Pinout view (e.g. `GPIO_Input` → `GPIO_EXTI4`) **resets its pull and label**. After changing the function, always re-check mode, pull and label in the GPIO table.

---

## 3. System Core → NVIC

The **NVIC** (*Nested Vectored Interrupt Controller*) is the part of the CPU that decides which interrupt runs and when. A peripheral can raise an interrupt, but it only reaches the CPU if its line is **enabled** here.

### Enabling an interrupt

Open **NVIC** in the left panel and tick the checkbox in the **Add** column (labelled *Enabled* in some versions) of the right row. Rows for peripherals appear only after that peripheral is configured. With **Show: available interrupts**, a missing row means the pin or peripheral isn't set up yet.

### EXTI lines (GPIO interrupts)

**EXTI line number = pin number.** PE4 → line 4, PA0 → line 0.

| Pins | NVIC row | Handler generated |
|---|---|---|
| Pin 0 | EXTI line0 interrupt | `EXTI0_IRQHandler` |
| Pin 1 | EXTI line1 interrupt | `EXTI1_IRQHandler` |
| Pin 2 | EXTI line2 interrupt | `EXTI2_IRQHandler` |
| Pin 3 | EXTI line3 interrupt | `EXTI3_IRQHandler` |
| Pin 4 | EXTI line4 interrupt | `EXTI4_IRQHandler` |
| Pins 5–9 | EXTI line[9:5] interrupts | `EXTI9_5_IRQHandler` (shared) |
| Pins 10–15 | EXTI line[15:10] interrupts | `EXTI15_10_IRQHandler` (shared) |

Only **one port per line**: PE4 and PA4 can't both be interrupts.

### Priority

| Column | Meaning |
|---|---|
| **Preemption priority** | Lower number = more urgent. A more urgent interrupt can interrupt a less urgent handler. |
| **Sub priority** | Tie-breaker between interrupts waiting at the same time. |

The default group *4 bits for pre-emption priority* gives 16 levels (0–15) and no sub priority. Leave everything at 0 for now. Priorities start to matter when several interrupts compete (lesson 20, FreeRTOS).

### What CubeMX generates

```c
// main.c, in MX_GPIO_Init():
HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
HAL_NVIC_EnableIRQ(EXTI4_IRQn);
```

```
pin edge → EXTI hardware → NVIC → EXTI4_IRQHandler → HAL_GPIO_EXTI_IRQHandler → HAL_GPIO_EXTI_Callback
```

`EXTI4_IRQHandler` (in `stm32f4xx_it.c`) and `HAL_GPIO_EXTI_IRQHandler` are written for you. You write only `HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)`.

---

## 4. Checklist: button interrupt + buzzer output

1. **Pinout view:** left-click **PE4** → `GPIO_EXTI4`. Left-click **PE5** → `GPIO_Output`.
2. **System Core → GPIO → PE4:**
   - GPIO mode: *External Interrupt Mode with Falling edge trigger detection*
   - Pull: *Pull-up*
   - User Label: `BTN`
3. **System Core → GPIO → PE5:**
   - GPIO mode: *Output Push Pull*
   - Pull: *No pull*
   - User Label: `BUZZER`
4. **System Core → NVIC:** tick **EXTI line4 interrupt**.
5. **GENERATE CODE.**
6. **Verify:**
   - `main.h` has `BTN_Pin`, `BTN_GPIO_Port`, `BUZZER_Pin`, `BUZZER_GPIO_Port`
   - `main.c` calls `HAL_NVIC_EnableIRQ(EXTI4_IRQn)`
   - `stm32f4xx_it.c` has `EXTI4_IRQHandler`

## 5. Troubleshooting

| Symptom | Cause |
|---|---|
| GPIO mode dropdown only has *Input mode* | Pin function is `GPIO_Input`. Change it to `GPIO_EXTIx` in the Pinout view. |
| No *EXTI line4* row in NVIC | PE4 isn't in an interrupt mode yet. |
| `BTN_Pin` undeclared | User Label is empty, or was reset when you changed the pin function. |
| Input reads random 0/1 | Floating. Set a pull-up or pull-down. |
| Callback never runs | EXTI line not ticked in NVIC, or *Event* mode chosen instead of *Interrupt* mode. |
| Fires on release, not press | Wrong edge for your pull. |
| Settings gone after Generate | You edited the generated code outside `USER CODE` blocks, or edited `.ioc` without saving. |
