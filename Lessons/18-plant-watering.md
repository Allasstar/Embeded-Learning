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
| Soil moisture | VCC / GND / AO | 3V / GND / PC5 |
| L9110 (motor B channel) | B-IA / B-IB | PB12 / **GND** |
| L9110 | VCC / GND | power module 5V / GND |
| Pump | wires | L9110 MOTOR-B terminals |
| DS1302 | VCC / GND | 3V / GND |
| DS1302 | CLK / DAT / RST (CE) | PD11 / PB15 / PD10 |
| Rotary encoder | + / GND | 3V / GND |
| Rotary encoder | CLK / DT / SW | PA15 / PB3 / PC11 |
| *Water level (optional)* | `+` / `-` / S | 3V / GND / PB1 (ADC1_IN9) |
| OLED (SSD1306, 128×64) | VCC / GND / SCL / SDA | 3V / GND / PB6 / PB9 |
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
6. **PC5** → `ADC1_IN15`. **Analog → ADC1**: IN15 ticked, Resolution *12 bits*, Scan *Disabled*, Continuous *Disabled*, Rank 1 → Channel 15, Sampling Time *480 Cycles*. *Why:* one slow, polled reading every 10 s is plenty; the long sampling time gives a stable value for a high-impedance sensor.
7. **Connectivity → I2C1** (already enabled on PB6/PB9 for the on-board audio chip): change Speed Mode to **Fast Mode**, 400000 Hz. *Why:* a full OLED frame takes ~93 ms at 100 kHz but ~25 ms at 400 kHz. The audio chip is rated for 100 kHz, but it ignores the bus while it's held in reset, which is how this project leaves it.
8. Generate.

The soil module's AO must stay ≤ 3.3 V (ADC pins are not 5 V tolerant), so it is powered from 3V.

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

The protocol needs microsecond delays, which `HAL_Delay` (1 ms) can't give. The Cortex-M4's cycle counter (`DWT->CYCCNT`, 168 counts per µs) can:

```c
static void DelayUs(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = us * (SystemCoreClock / 1000000);
  while (DWT->CYCCNT - start < cycles)
  {
  }
}
```

Enable the counter once in `App_Start()`, before the first RTC access:

```c
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

The rest of the driver:

```c

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

Call `Rtc_SetTime(14, 5, 0)` once from `App_Start()`, flash, then **remove the call** and flash again (otherwise every reset rewrites the time). Unplug the board for a minute, plug in: the time kept running on the coin cell.

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

### Step 4: Soil moisture and states
Read the soil sensor by polling (once every 10 s is plenty):

```c
extern ADC_HandleTypeDef hadc1;

static uint32_t SoilRaw(void)
{
  HAL_ADC_Start(&hadc1);
  HAL_ADC_PollForConversion(&hadc1, 10);
  uint32_t raw = HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);
  return raw;
}
```

Calibrate it: log the raw value with the probe in **dry air** (e.g. `dry = 3600`) and in a **glass of water** (e.g. `wet = 1400`), then `percent = (dry - raw) * 100 / (dry - wet)`, clamped to 0..100. The values differ per sensor, so measure yours.

Now the states:

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

Implement it without any blocking delay: `EnterState()` sets outputs and remembers `HAL_GetTick()`; `Update()` checks the conditions for leaving the current state and returns.

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
The device has to show its state without a PC, so it gets the 128×64 OLED. This is a compact version of the driver lesson 13 builds step by step: the MCU keeps a 1024-byte copy of the screen (the **framebuffer**), you draw into it, and `Oled_Update()` sends it all in one I2C transfer.

**Font:** get `glcdfont.c` from the *Adafruit-GFX-Library* GitHub repository (BSD license), save it as `Core/Src/font5x7.c`, remove the `#include`s and `PROGMEM`, rename the array to `const uint8_t font5x7[]`, and add it to `CMakeLists.txt`. Each character is 5 bytes, one per column, bit 0 at the top: the same layout as the display.

`Core/Src/oled.c` (`oled.h` declares the public functions and `OLED_WIDTH` / `OLED_HEIGHT`):

```c
#include "oled.h"
#include "main.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;
extern const uint8_t font5x7[];

#define OLED_ADDR (0x3C << 1)

static uint8_t frame[OLED_WIDTH * OLED_HEIGHT / 8];

static void Commands(const uint8_t *commands, uint16_t count)
{
  HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, (uint8_t *)commands, count, 100);
}

void Oled_Init(void)
{
  static const uint8_t init[] =
  {
    0xAE,       // display off
    0xD5, 0x80, // clock divide
    0xA8, 0x3F, // multiplex: 64 rows
    0xD3, 0x00, // display offset 0
    0x40,       // start line 0
    0x8D, 0x14, // charge pump on (makes the high panel voltage from 3.3 V)
    0x20, 0x00, // horizontal addressing mode
    0xA1,       // mirror columns
    0xC8,       // mirror rows
    0xDA, 0x12, // COM pins config for 128x64
    0x81, 0xCF, // contrast
    0xD9, 0xF1, // pre-charge
    0xDB, 0x40, // VCOMH level
    0xA4,       // show RAM contents
    0xA6,       // normal (not inverted)
    0xAF,       // display on
  };

  HAL_Delay(100);
  Commands(init, sizeof(init));
  Oled_Clear();
  Oled_Update();
}

void Oled_Clear(void)
{
  memset(frame, 0, sizeof(frame));
}

void Oled_Update(void)
{
  static const uint8_t window[] = { 0x21, 0, OLED_WIDTH - 1, 0x22, 0, 7 };
  Commands(window, sizeof(window));
  HAL_I2C_Mem_Write(&hi2c1, OLED_ADDR, 0x40, I2C_MEMADD_SIZE_8BIT, frame, sizeof(frame), 100);
}

static void DrawPixel(int32_t x, int32_t y, uint8_t on)
{
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
    return;

  uint8_t *cell = &frame[x + (y / 8) * OLED_WIDTH];
  uint8_t mask = 1 << (y % 8);
  if (on) *cell |= mask;
  else    *cell &= ~mask;
}

void Oled_DrawString(int32_t x, int32_t y, const char *text, uint8_t inverted)
{
  for (; *text; text++, x += 6)
  {
    const uint8_t *glyph = &font5x7[(uint8_t)*text * 5];
    for (int32_t column = 0; column < 6; column++)
    {
      uint8_t bits = column < 5 ? glyph[column] : 0;
      for (int32_t row = 0; row < 8; row++)
        DrawPixel(x + column, y + row, ((bits >> row) & 1) != inverted);
    }
  }
}
```

The init table is the one place where comments describe *what*: raw command bytes are meaningless without them. They all come from the SSD1306 datasheet command table. If the text is upside down, use `0xA0` / `0xC0` instead of `0xA1` / `0xC8`.

`inverted` draws white-on-black text as black-on-white, including the gap column, so a selected menu field becomes a solid highlighted block.

Call `Oled_Init()` in `App_Start()`. Every 200 ms, clear, draw four lines at `y = 0, 16, 32, 48` (21 characters fit per line), and update:

```
14:05:32     MONITOR
Soil  42%  (min 35)
Water  2/6 today
Last  09:12  5s
```

In the menu, draw the selected field with `inverted = 1`.

**Why only 5 times a second:** a full frame is 1024 bytes, about 25 ms of blocking I2C at 400 kHz. Redrawing every loop would stall the encoder and the plant logic. The pump decisions don't depend on the screen at all, so a slow screen is harmless.

### Step 7: Test the logic without waiting all day
Testing "water at 07:00, soak 10 minutes" in real time is painful. Add three global variables and edit them in the debugger's Watch window:
- `fakeSoil` (0 = off, 1..100 overrides the moisture reading)
- `fakeHour` (-1 = off, 0..23 overrides the hour)
- `soakSeconds` (soak time in seconds instead of 10 minutes)

Remove (or `#ifdef DEBUG`) them when done. Being able to **inject test conditions** is a basic embedded test technique.

## Check it works
- [ ] Time survives power loss
- [ ] Dry soil (probe in air) → pump runs N seconds → soaks → checks again
- [ ] No watering outside the allowed hours
- [ ] Stops after the daily maximum
- [ ] Pump never runs longer than 15 s, even if you set more
- [ ] Encoder changes settings smoothly, both directions, no skipped steps
- [ ] Everything keeps running while you're in the menu
- [ ] The screen shows all four lines and the selected menu field is highlighted

## Troubleshooting
| Symptom | Cause |
|---|---|
| Time reads 0 or 85:85:85 | wrong pins, IO not open-drain, or write protect still on |
| Clock doesn't advance | CH bit set: write the seconds register |
| Encoder counts only one way / jumps | PB3 still assigned to SWO, or CLK/DT swapped (swaps direction only) |
| One click = several steps | change the divisor (4 → 2) |
| Pump runs but no water | air in the pump: it must be submerged |
| Board resets when pump starts | pump on board power: use the power module |
| OLED stays black | wrong address (scan for 0x3C / 0x3D), charge pump command missing, or no power |
| Text upside down or mirrored | swap `0xA1`/`0xC8` for `0xA0`/`0xC0` in the init table |

## Challenges
1. Store the settings in the **DS1302's 31 bytes of battery-backed RAM** (commands 0xC0–0xFC) so they survive power loss. Add a checksum byte to detect garbage.
2. Soil sensors corrode when powered constantly. Power the soil module from a **GPIO pin** (it draws a few mA) and switch it on only for 100 ms per measurement.
3. Log each watering with a time stamp over Bluetooth (lesson 10).
4. Put the whole system in **sleep** between measurements (`HAL_PWR_EnterSLEEPMode`) and measure the current difference with a multimeter.
