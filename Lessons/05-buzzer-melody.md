# Lesson 05: Buzzer Melody

## Goal
Play a melody on the passive buzzer using a hardware timer to make each note. The CPU only changes notes; the timer makes the sound.

## What you learn
- Changing PWM **frequency** (not just duty) at run time
- APB1 vs APB2 timers and their different clocks
- Preload (shadow) registers and the glitches they prevent
- Data-driven code: a melody is just an array

## Parts and wiring
Passive buzzer module: S → **PE5**, `+` → 3V, `-` → GND.

Passive means it has no oscillator inside: it makes a sound only while you feed it a square wave, and the wave's frequency is the pitch. That's why a timer is needed.

## The maths
PE5 can be **TIM9 channel 1**. TIM9 sits on **APB2**, so its clock is **168 MHz** (not 84 like TIM3/TIM4).

```
Prescaler = 168-1   → counter at 1 MHz (1 tick = 1 µs)
Period    = 1000000 / frequency - 1
Compare   = (Period + 1) / 2   → 50% duty, loudest square wave
```

| Note | Hz | Period |
|---|---|---|
| C4 | 262 | 3816 |
| D4 | 294 | 3400 |
| E4 | 330 | 3029 |
| F4 | 349 | 2864 |
| G4 | 392 | 2550 |
| A4 | 440 | 2271 |
| B4 | 494 | 2023 |
| C5 | 523 | 1911 |

TIM9 is 16-bit, so the Period can go up to 65535. The lowest note is 1 MHz / 65536 ≈ 15 Hz, far below anything you need.

## CubeMX setup

Open `Test_DISC1.ioc` in STM32CubeMX.

### 1. Route the pin
In the Pinout view, click **PE5** and choose `TIM9_CH1`.

*Why:* PE5 is a normal GPIO by default. Choosing `TIM9_CH1` connects the pin to the timer's output, so the timer drives it in hardware and the CPU never toggles it.

### 2. Enable the timer
**Timers → TIM9**:
- **Clock Source**: `Internal Clock`
- **Channel1**: `PWM Generation CH1`

*Why internal clock:* the counter counts ticks of the chip's own clock. TIM9 sits on APB2, which runs the timers at 168 MHz. The other choices are for counting external signals.

### 3. Parameter Settings

| Setting | Value | Why |
|---|---|---|
| Prescaler | `168-1` | 168 MHz / 168 = 1 MHz, so 1 tick = 1 µs and the period in ticks equals the period in µs. The `-1` is there because the hardware divides by (value + 1). |
| Counter Mode | `Up` | The default. The counter goes from 0 up to the period, then wraps. |
| Counter Period | `1000-1` | Any starting value will do (1 kHz). `Buzzer_Tone()` overwrites it for each note. |
| auto-reload preload | **Enable** | A new period waits until the current cycle ends. Without it every note change clicks (see *Why enable preload?* in Step 1). |
| Mode | `PWM mode 1` | The output is high while the counter is below the compare value. |
| Pulse | `0` | A compare value of 0 keeps the output low, so the buzzer is silent until the first note. |
| Fast Mode | `Disable` | Only matters at very high frequencies; these notes are a few hundred Hz. |
| CH Polarity | `High` | The default: the compare-match part of the cycle is the high part. |

### 4. Check the clock (optional)
On the Clock Configuration tab, **APB2 timer clocks** should read **168** MHz. This confirms the maths above.

### 5. Generate code
Click **Generate Code**.

## Steps

### Step 1: One tone

`Core/Inc/buzzer.h`:

```c
#pragma once
#include <stdint.h>

void Buzzer_Init(void);
void Buzzer_Tone(uint32_t frequency);
void Buzzer_Off(void);
```

`Core/Src/buzzer.c`:

```c
#include "buzzer.h"
#include "main.h"

extern TIM_HandleTypeDef htim9;

void Buzzer_Init(void)
{
  __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0);
  HAL_TIM_PWM_Start(&htim9, TIM_CHANNEL_1);
}

void Buzzer_Tone(uint32_t frequency)
{
  uint32_t period = 1000000 / frequency - 1;
  __HAL_TIM_SET_AUTORELOAD(&htim9, period);
  __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, (period + 1) / 2);
}

void Buzzer_Off(void)
{
  __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0);
}
```

Test with `Buzzer_Tone(440)` in `App_Start()`. That's concert A. Compare with a tuner app on your phone.

**Why enable preload?** Without it, writing the Period takes effect instantly. If the counter is already at 2500 and you write a new Period of 2000, the counter misses the match and keeps counting to 65535 before wrapping: a 65 ms glitch, heard as a click. With preload on, the new value waits in a shadow register until the current cycle ends. Try both and listen.

### Step 2: A melody as data

`Core/Inc/melody.h`:

```c
#pragma once

void Melody_Play(void);
void Melody_Update(void);
```

`Core/Src/melody.c`:

```c
#include "melody.h"
#include "buzzer.h"
#include "main.h"

#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define REST    0

typedef struct
{
  uint16_t frequency;
  uint16_t durationMs;
} Note;

static const Note twinkle[] =
{
  { NOTE_C4, 400 }, { NOTE_C4, 400 }, { NOTE_G4, 400 }, { NOTE_G4, 400 },
  { NOTE_A4, 400 }, { NOTE_A4, 400 }, { NOTE_G4, 800 },
  { NOTE_F4, 400 }, { NOTE_F4, 400 }, { NOTE_E4, 400 }, { NOTE_E4, 400 },
  { NOTE_D4, 400 }, { NOTE_D4, 400 }, { NOTE_C4, 800 },
};

#define NOTE_COUNT (sizeof(twinkle) / sizeof(twinkle[0]))
#define GAP_MS 40

static uint8_t playing;
static uint32_t noteIndex;
static uint32_t noteStart;

static void StartNote(void)
{
  noteStart = HAL_GetTick();
  if (twinkle[noteIndex].frequency == REST)
    Buzzer_Off();
  else
    Buzzer_Tone(twinkle[noteIndex].frequency);
}

void Melody_Play(void)
{
  playing = 1;
  noteIndex = 0;
  StartNote();
}

void Melody_Update(void)
{
  if (!playing)
    return;

  uint32_t elapsed = HAL_GetTick() - noteStart;

  if (elapsed >= twinkle[noteIndex].durationMs - GAP_MS)
    Buzzer_Off();

  if (elapsed >= twinkle[noteIndex].durationMs)
  {
    noteIndex++;
    if (noteIndex >= NOTE_COUNT)
    {
      playing = 0;
      return;
    }
    StartNote();
  }
}
```

The short silent gap at the end of each note makes repeated notes (C C, G G) sound separate instead of one long note.

Call `Buzzer_Init()` in `App_Start()`, `Melody_Update()` in `App_Update()`, and `Melody_Play()` on the blue button's rising edge. `App_Update()` already detects that edge to change the blinking LED, so add the call there.

Polling `Melody_Update()` instead of using `HAL_Delay()` is deliberate: `HAL_Delay()` would freeze the main loop for the whole melody, and the LED blink and button reading would stop.

## Check it works
- [ ] 440 Hz matches a tuner app
- [ ] Melody is recognisable, repeated notes are separate
- [ ] The LED keeps blinking and the button still works during playback
- [ ] No clicks between notes

## Troubleshooting
| Symptom | Cause |
|---|---|
| Pitch is half what you expect | used 84 MHz in your maths: TIM9 runs at 168 MHz |
| Clicks between notes | auto-reload preload disabled |
| Quiet | that's a small buzzer; it's loudest near its resonance (~2–4 kHz) |
| Silence | compare is 0, or `HAL_TIM_PWM_Start` missing |

## Challenges
1. Add a `tempo` variable that scales all durations. Use the blue button to cycle through speeds.
2. Store melodies in a compact format, e.g. a string `"C4 C4 G4 G4 A4 A4 G4-"`, and write a parser.
3. Write a two-tone doorbell (ding-dong) on top of `Buzzer_Tone()`.
4. Volume: try duties of 50%, 10%, 2%. Explain why lower duty sounds quieter but also changes the tone.
