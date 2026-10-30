# Lesson 18: Plant Watering System

## Goal
A complete device: it measures soil moisture, waters the plant with the pump when it's dry (but only during the day and never too much), keeps time with a battery-backed clock, shows its status on the OLED, and lets you change settings with the rotary encoder.

## What you learn
- Designing a whole system: requirements, states, safety limits
- A real-time clock and BCD numbers
- Another bit-banged protocol (DS1302 3-wire)
- **Timer encoder mode**: hardware counts a rotary encoder for you
- A simple on-device menu
- Fail-safe design: what if a sensor fails or the water runs out?

## Requirements
Write these down before coding. This is how real projects start.

1. Measure soil moisture every 10 s.
2. If moisture < **threshold** (default 35 %), run the pump for **N seconds** (default 5 s).
3. Then wait **10 minutes** before checking again ("soak"). Water spreads slowly, and the sensor reacts late. Without this wait, it would flood the pot.
4. Only water between **07:00 and 21:00**.
5. At most **6 waterings per day**: a protection against a broken sensor reading "dry" forever.
6. Never run the pump longer than **15 s** in one go, whatever the settings say.
7. *(Optional, with the water level module)* If the reservoir is empty, never run the pump (pumps get damaged running dry) and show a warning.
8. OLED shows time, moisture, state, last watering.
9. Encoder: turn to change a setting, press to move to the next setting, long press to exit.

## Parts and wiring

| Module | Pin | Board pin |
|---|---|---|
| Soil moisture | VCC / GND / AO | 3V / GND / PC5 (from lesson 08) |
| L9110 (motor B channel) | B-IA / B-IB | PB12 / **GND** |
| L9110 | VCC / GND | power module 5V / GND |
| Pump | wires | L9110 MOTOR-B terminals |
| DS1302 | VCC / GND | 3V / GND |
| DS1302 | CLK / DAT / RST (CE) | PD11 / PB15 / PD10 |
| Rotary encoder | + / GND | 3V / GND |
| Rotary encoder | CLK / DT / SW | PA15 / PB3 / PC11 |
| OLED | as lesson 13 | I2C1 |
| *Water level (optional)* | `+` / `-` / S | 3V / GND / PB1 (ADC1_IN9) |
| *Relay (optional)* | VCC / GND / IN | 5V / GND / PC8 |

Put the pump fully **under water** in a container, and the tube into the plant pot. Test over a sink first.

B-IB is tied to GND: the pump only needs one direction, so it's on/off from one GPIO pin.

**Relay**: you can switch the pump through the relay instead of the L9110, to practise. Relays are for loads that need isolation. **Never mains voltage.** Relay modules are often *active-low* (IN = LOW turns them on): check.

## CubeMX setup
1. **PB12** → `GPIO_Output`, label `PUMP`.
2. **PD10** → `GPIO_Output` label `RTC_CE`; **PD11** → `GPIO_Output` label `RTC_CLK`; **PB15** → `GPIO_Output` label `RTC_IO`, **Open Drain**, **Pull-up**, output level High.
3. **System Core → SYS → Debug**: *Serial Wire*. That frees PB3 (it was assigned to SWO trace, which you don't use).
4. **TIM2**: Combined Channels → **Encoder Mode**. Pins become PA15 (CH1) and PB3 (CH2). Parameters: Encoder Mode *TI1 and TI2*, Period `65535`, Input Filter `10` for both channels.
5. **PC11** → `GPIO_Input`, label `ENC_SW`, **Pull-up**.
6. Generate.

---

## Part 1: DS1302 real-time clock

### Protocol (DS1302 datasheet)
- Raise **CE**, send a command byte, then send or receive one data byte, lower CE.
- Bits are **LSB first**. The DS1302 reads IO on the **rising** CLK edge and outputs on the **falling** edge.
- Command byte: bit 7 = 1, bits 1–5 = register, bit 0 = 1 for read, 0 for write.

| Register | Write cmd | Read cmd | Contents (BCD) |
|---|---|---|---|
| Seconds | 0x80 | 0x81 | bit 7 = CH (clock halt), bits 0–6 seconds |
| Minutes | 0x82 | 0x83 | |
| Hours | 0x84 | 0x85 | bit 7 = 0 → 24-hour mode |
| Date | 0x86 | 0x87 | |
| Month | 0x88 | 0x89 | |
| Year | 0x8C | 0x8D | 00–99 |
| Write protect | 0x8E | 0x8F | bit 7 = WP: write 0x00 before setting the time |

**BCD** (binary-coded decimal): each 4-bit nibble is one decimal digit. 45 is stored as `0x45`.

### Step 1: Driver

```c
#include "delay_us.h"

static void Pin(GPIO_TypeDef *port, uint16_t pin, int high)
{
  HAL_GPIO_WritePin(port, pin, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void WriteByte(uint8_t value)
{
  for (int i = 0; i < 8; i++)
  {
    Pin(RTC_IO_GPIO_Port, RTC_IO_Pin, (value >> i) & 1);
    DelayUs(1);
    Pin(RTC_CLK_GPIO_Port, RTC_CLK_Pin, 1);
    DelayUs(1);
    Pin(RTC_CLK_GPIO_Port, RTC_CLK_Pin, 0);
  }
}

static uint8_t ReadByte(void)
{
  uint8_t value = 0;
  Pin(RTC_IO_GPIO_Port, RTC_IO_Pin, 1);
  for (int i = 0; i < 8; i++)
  {
    if (HAL_GPIO_ReadPin(RTC_IO_GPIO_Port, RTC_IO_Pin))
      value |= 1 << i;
    Pin(RTC_CLK_GPIO_Port, RTC_CLK_Pin, 1);
    DelayUs(1);
    Pin(RTC_CLK_GPIO_Port, RTC_CLK_Pin, 0);
    DelayUs(1);
  }
  return value;
}

static uint8_t Rtc_Read(uint8_t command)
{
  Pin(RTC_CE_GPIO_Port, RTC_CE_Pin, 1);
  DelayUs(4);
  WriteByte(command | 0x01);
  uint8_t value = ReadByte();
  Pin(RTC_CE_GPIO_Port, RTC_CE_Pin, 0);
  DelayUs(4);
  return value;
}

static void Rtc_Write(uint8_t command, uint8_t value)
{
  Pin(RTC_CE_GPIO_Port, RTC_CE_Pin, 1);
  DelayUs(4);
  WriteByte(command & 0xFE);
  WriteByte(value);
  Pin(RTC_CE_GPIO_Port, RTC_CE_Pin, 0);
  DelayUs(4);
}

static uint8_t FromBcd(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static uint8_t ToBcd(uint8_t v)   { return ((v / 10) << 4) | (v % 10); }
```

The IO pin is open-drain: writing 1 releases the line so the DS1302 can drive it during reads. The last command bit (bit 7) is always 1, so the line is already released when the chip starts answering.

### Step 2: Set and read the time

```c
void Rtc_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
  Rtc_Write(0x8E, 0x00);
  Rtc_Write(0x80, ToBcd(seconds));
  Rtc_Write(0x82, ToBcd(minutes));
  Rtc_Write(0x84, ToBcd(hours));
  Rtc_Write(0x8E, 0x80);
}

void Rtc_GetTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
{
  *seconds = FromBcd(Rtc_Read(0x81) & 0x7F);
  *minutes = FromBcd(Rtc_Read(0x83) & 0x7F);
  *hours   = FromBcd(Rtc_Read(0x85) & 0x3F);
}
```

Writing seconds with bit 7 = 0 also clears **CH**: a new chip starts with the clock **halted**.

Add a console command `time 14 05 00` (lesson 10). Set it, unplug the board for a minute, plug in: time kept running on the coin cell.

---

## Part 2: Rotary encoder in hardware

A rotary encoder outputs two square waves 90° apart (**quadrature**). Which one leads tells the direction. In **encoder mode**, the timer counts up or down on every edge of both signals by itself. No interrupts and no missed steps, even when you spin it fast.

### Step 3: Read the encoder

```c
extern TIM_HandleTypeDef htim2;

static uint16_t lastCount;

void Encoder_Init(void)
{
  HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
  lastCount = __HAL_TIM_GET_COUNTER(&htim2);
}

int32_t Encoder_GetSteps(void)
{
  uint16_t count = __HAL_TIM_GET_COUNTER(&htim2);
  int16_t delta = (int16_t)(count - lastCount);
  int32_t steps = delta / 4;
  lastCount += steps * 4;
  return steps;
}
```

- `(int16_t)(count - lastCount)` gives a signed difference that's correct even when the counter wraps from 65535 to 0.
- *TI1 and TI2* mode counts **4 edges per detent** ("click"), so divide by 4. If one click moves by 2 on your encoder, use 2. Only consume whole steps (`lastCount += steps * 4`) so partial turns aren't lost.
- The **input filter** debounces the mechanical contacts in hardware.

---

## Part 3: The application

### Step 4: States

```c
typedef enum
{
  PLANT_MONITOR,
  PLANT_WATERING,
  PLANT_SOAKING,
  PLANT_NO_WATER,
} PlantState;
```

```
            dry && daytime && waterings < max && reservoir ok
  MONITOR ──────────────────────────────────────────► WATERING
     ▲                                                   │ N seconds (≤ 15 s)
     │ 10 min                                            ▼
     └────────────────────────────────────────────── SOAKING

  any state ── reservoir empty ──► NO_WATER ── refilled ──► MONITOR
```

Implement it as in lesson 01: `EnterState()` sets outputs and remembers the time; `Update()` checks the conditions for leaving the current state.

The pump must be **off** in every state except WATERING, and `EnterState()` is the only place that switches it. One place = one place to check for bugs.

Reset the daily watering counter when the RTC hour goes from 23 to 0.

### Step 5: Settings and menu

```c
typedef struct
{
  int32_t thresholdPercent;
  int32_t waterSeconds;
  int32_t startHour;
  int32_t endHour;
} Settings;

static Settings settings = { 35, 5, 7, 21 };
```

Menu behaviour:
- Short press: enter menu / move to next field.
- Turn: change the selected field (clamp to sensible limits; `waterSeconds` max 15).
- Long press (> 1 s): leave menu.
- While in the menu, the plant logic keeps running (it's non-blocking!).

### Step 6: Screen

```
14:05:32     MONITOR
Soil  42%  (min 35)
Water  2/6 today
Last  09:12  5s
```

In the menu, invert or underline the selected field.

### Step 7: Test the logic without waiting all day
Testing "water at 07:00, soak 10 minutes" in real time is painful. Add console commands:
- `fake soil 20`: override the moisture reading
- `fake hour 6`: override the hour
- `soak 10`: soak time in seconds instead of minutes

Remove (or `#ifdef DEBUG`) them when done. Being able to **inject test conditions** is a basic embedded test technique.

## Check it works
- [ ] Time survives power loss
- [ ] Dry soil (probe in air) → pump runs N seconds → soaks → checks again
- [ ] No watering outside the allowed hours
- [ ] Stops after the daily maximum
- [ ] Pump never runs longer than 15 s, even if you set more
- [ ] Encoder changes settings smoothly, both directions, no skipped steps
- [ ] Everything keeps running while you're in the menu

## Troubleshooting
| Symptom | Cause |
|---|---|
| Time reads 0 or 85:85:85 | wrong pins, IO not open-drain, or write protect still on |
| Clock doesn't advance | CH bit set: write the seconds register |
| Encoder counts only one way / jumps | PB3 still assigned to SWO, or CLK/DT swapped (swaps direction only) |
| One click = several steps | change the divisor (4 → 2) |
| Pump runs but no water | air in the pump: it must be submerged |
| Board resets when pump starts | pump on board power: use the power module |

## Challenges
1. Store the settings in the **DS1302's 31 bytes of battery-backed RAM** (commands 0xC0–0xFC) so they survive power loss. Add a checksum byte to detect garbage.
2. Soil sensors corrode when powered constantly. Power the soil module from a **GPIO pin** (it draws a few mA) and switch it on only for 100 ms per measurement.
3. Log each watering with a time stamp over Bluetooth.
4. Put the whole system in **sleep** between measurements (`HAL_PWR_EnterSLEEPMode`) and measure the current difference with a multimeter.
