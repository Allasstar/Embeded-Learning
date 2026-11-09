# Lesson 13: OLED Display (SSD1306)

## Goal
Write a driver for the 128×64 OLED: pixels, text, bar graphs and a bouncing-ball animation.

## What you learn
- **Framebuffers**: draw in RAM, then send the whole picture
- How a monochrome display maps bits to pixels (pages)
- Bulk I2C transfers and I2C Fast Mode (400 kHz)
- Using a font table
- Where the time goes (measuring transfer cost)

## How it works
The SSD1306 controller has its own 1 KB of display RAM: 128 columns × 8 **pages**, each page is 8 pixel rows. One byte = one column of 8 vertical pixels, **bit 0 at the top**.

```
byte index = x + (y / 8) * 128
bit        = y % 8
```

The MCU keeps a copy of those 1024 bytes (the **framebuffer**). You draw into the copy, which is fast and costs no I2C traffic, then send all 1024 bytes in one transfer.

Every I2C write to the SSD1306 starts with a **control byte**: `0x00` = the following bytes are commands, `0x40` = the following bytes are pixel data. That maps neatly onto `HAL_I2C_Mem_Write` with the control byte as the "memory address".

## Parts and wiring

| OLED pin | Board pin |
|---|---|
| GND | GND |
| VCC | 3V |
| SCL | PB6 |
| SDA | PB9 |

Scan I2C1 by calling this from `App_Start()` (`#include "log.h"`): expect **0x3C**.

```c
for (uint16_t address = 1; address < 128; address++)
{
  if (HAL_I2C_IsDeviceReady(&hi2c1, address << 1, 2, 5) == HAL_OK)
    LOG("found 0x%02X\n", address);
}
```

## CubeMX setup
**I2C1**: change Speed Mode to **Fast Mode**, 400000 Hz. Generate.

At 100 kHz a full frame takes ~93 ms; at 400 kHz ~25 ms. (The on-board audio chip is rated for 100 kHz, but it ignores the bus while held in reset, which is how this project leaves it.)

## Steps

### Step 1: Init and clear

`Core/Inc/oled.h`:

```c
#pragma once
#include <stdint.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

void Oled_Init(void);
void Oled_Clear(void);
void Oled_Update(void);
void Oled_DrawPixel(int32_t x, int32_t y, uint8_t on);
void Oled_FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t on);
void Oled_DrawChar(int32_t x, int32_t y, char c);
void Oled_DrawString(int32_t x, int32_t y, const char *text);
```

`Core/Src/oled.c`:

```c
#include "oled.h"
#include "main.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c1;

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
```

The comments on the init table are the exception that proves the rule: raw command bytes are meaningless without them. They all come from the SSD1306 datasheet command table. Open it and find each one.

`0xA1` / `0xC8` flip the picture. If your text appears upside down, use `0xA0` / `0xC0`.

### Step 2: Pixels and rectangles

```c
void Oled_DrawPixel(int32_t x, int32_t y, uint8_t on)
{
  if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT)
    return;

  uint8_t *cell = &frame[x + (y / 8) * OLED_WIDTH];
  uint8_t mask = 1 << (y % 8);
  if (on) *cell |= mask;
  else    *cell &= ~mask;
}

void Oled_FillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t on)
{
  for (int32_t i = x; i < x + w; i++)
    for (int32_t j = y; j < y + h; j++)
      Oled_DrawPixel(i, j, on);
}
```

The bounds check makes off-screen drawing safe: shapes that move partly off the edge just get clipped.

Test: a frame around the screen edge, and a filled square in the middle.

### Step 3: Font
A font is a table of bitmaps. The common **5×7** font stores each character as 5 bytes, one per column, bit 0 at the top: the same layout as the display, which is why it's so popular.

1. Get `glcdfont.c` from the *Adafruit-GFX-Library* GitHub repository (BSD license). It holds 256 characters × 5 bytes.
2. Save it as `Core/Src/font5x7.c`. Remove the `#include`s and `PROGMEM`, rename the array to `font5x7`, make it `const uint8_t font5x7[]`.
3. Add it to `CMakeLists.txt` and declare `extern const uint8_t font5x7[];` in `oled.c`.

```c
void Oled_DrawChar(int32_t x, int32_t y, char c)
{
  const uint8_t *glyph = &font5x7[(uint8_t)c * 5];
  for (int32_t column = 0; column < 5; column++)
  {
    uint8_t bits = glyph[column];
    for (int32_t row = 0; row < 8; row++)
      Oled_DrawPixel(x + column, y + row, bits & (1 << row));
  }
}

void Oled_DrawString(int32_t x, int32_t y, const char *text)
{
  while (*text)
  {
    Oled_DrawChar(x, y, *text++);
    x += 6;
  }
}
```

6 px per character (5 + 1 gap) → 21 characters per line, 8 lines.

`font5x7` is `const`, so the linker puts it in **flash**, not RAM. Check in the build output's memory usage: RAM didn't grow by 1280 bytes.

### Step 4: Dashboard

```c
char text[22];
Oled_Clear();
snprintf(text, sizeof(text), "Up %lus", HAL_GetTick() / 1000);
Oled_DrawString(0, 0, text);
Oled_FillRect(0, 12, (HAL_GetTick() % 5000) * OLED_WIDTH / 5000, 8, 1);
Oled_Update();
```

Run it every 50 ms (20 fps).

### Step 5: Bouncing ball
A ball (4×4 square) with position and velocity. Every frame: move, bounce off the edges by flipping the velocity sign, clear, draw, update. Target 30 fps (33 ms per frame).

### Step 6: Measure
Time `Oled_Update()` with `HAL_GetTick()` at 100 kHz and at 400 kHz. The CPU waits for every byte. That's the motivation for challenge 1.

## Check it works
- [ ] Text readable, right way up
- [ ] Bar grows from empty to full every 5 s
- [ ] Ball animation is smooth
- [ ] `Oled_Update()` takes ~25 ms at 400 kHz

## Troubleshooting
| Symptom | Cause |
|---|---|
| Black screen | wrong address (0x3D?), charge pump command missing, no power |
| Snow / random pixels | display RAM never written: `Oled_Update()` not called |
| Image shifted / wrapped | addressing mode or window commands wrong |
| Upside down / mirrored | `0xA1`/`0xC8` vs `0xA0`/`0xC0` |
| Every other line missing | `0xDA` value wrong for your panel (try `0x02` for 128×32) |
| Garbage characters | font table offset: does your font start at char 0 or char 32? |

## Challenges
1. **DMA transfer**: add DMA for I2C1_TX in CubeMX, use `HAL_I2C_Mem_Write_DMA`, and don't touch `frame` until `HAL_I2C_MemTxCpltCallback` fires. Now `Oled_Update()` returns in microseconds. (Bonus: two framebuffers, draw into one while the other is being sent.)
2. Draw lines with **Bresenham's algorithm**, and circles with the midpoint algorithm.
3. Plot a sensor value as a scrolling graph (for example the potentiometer or photoresistor from lesson 08): keep the last 128 samples in a ring buffer and draw one column per sample.
4. A double-size font: draw each font pixel as a 2×2 block.
