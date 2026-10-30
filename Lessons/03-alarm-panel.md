# Lesson 03: Alarm Panel

## Goal
Connect six digital sensors. Each one lights its own LED when triggered. Then turn it into an alarm: when armed, any trigger latches the alarm and sounds the buzzer until you disarm with the blue button.

## What you learn
- How digital sensor modules work (comparator + threshold pot)
- Active-high vs active-low signals
- Describing many similar devices with a table instead of copy-paste code
- Edge detection in software
- Reading datasheets and measuring before trusting

## Parts and wiring

| Sensor | Power | Signal pin | Notes |
|---|---|---|---|
| PIR Motion Sensor | **5V** | PE7 | output is 3.3 V even at 5 V supply |
| Hall Switch | 3V | PE8 | reacts to one pole of a magnet |
| IR Obstacle Avoidance | 3V | PE10 | adjust range with its pot |
| Vibration | 3V | PE12 | adjust sensitivity with its pot |
| Touch | 3V | PE13 | |
| Flame | 3V | PE15 | test with a lighter **far** from the board, or a TV remote (IR) |

All GNDs to board GND. Keep the traffic light (lesson 01) and buzzer (lesson 02) connected.

### How these modules work
Most blue boards with a small potentiometer have an **LM393 comparator**: it compares the sensor's analog voltage with the voltage set by the pot, and outputs a clean HIGH or LOW. The on-board LED usually lights when the output is **LOW**. Turning the pot moves the threshold.

The **PIR** (HC-SR501) has two pots: *Sx* = sensitivity, *Tx* = how long the output stays HIGH after motion (3 s to 5 min, turn it fully counter-clockwise for the shortest time). It needs **~60 s to settle** after power-up and gives false triggers during that time.

## CubeMX setup
Set each signal pin to `GPIO_Input` with a label: `PIR`, `HALL`, `IR_OBST`, `VIBRATION`, `TOUCH`, `FLAME`. Pull: *No pull* (the modules drive the line). Generate.

## Steps

### Step 1: Measure every sensor
Don't trust guesses about which level means "triggered". Temporarily log all six:

```c
LOG("PIR%d HALL%d IR%d VIB%d TOUCH%d FLAME%d\n",
    HAL_GPIO_ReadPin(PIR_GPIO_Port, PIR_Pin),
    HAL_GPIO_ReadPin(HALL_GPIO_Port, HALL_Pin),
    HAL_GPIO_ReadPin(IR_OBST_GPIO_Port, IR_OBST_Pin),
    HAL_GPIO_ReadPin(VIBRATION_GPIO_Port, VIBRATION_Pin),
    HAL_GPIO_ReadPin(TOUCH_GPIO_Port, TOUCH_Pin),
    HAL_GPIO_ReadPin(FLAME_GPIO_Port, FLAME_Pin));
HAL_Delay(300);
```

Trigger each sensor and fill in this table:

| Sensor | Idle level | Triggered level |
|---|---|---|
| PIR | | |
| Hall | | |
| IR | | |
| Vibration | | |
| Touch | | |
| Flame | | |

A sensor whose triggered level is 0 is **active-low**.

### Step 2: Describe sensors with a table
Instead of six copies of the same `if`, put the differences in data:

`Core/Inc/alarm.h`:

```c
#pragma once

void Alarm_Init(void);
void Alarm_Update(void);
void Alarm_ToggleArmed(void);
```

`Core/Src/alarm.c`:

```c
#include "alarm.h"
#include "main.h"
#include "log.h"

typedef struct
{
  const char *name;
  GPIO_TypeDef *port;
  uint16_t pin;
  GPIO_PinState activeLevel;
  GPIO_TypeDef *ledPort;
  uint16_t ledPin;
  uint8_t wasActive;
} Sensor;

static Sensor sensors[] =
{
  { "PIR",       PIR_GPIO_Port,       PIR_Pin,       GPIO_PIN_SET,   LD3_GPIO_Port,       LD3_Pin },
  { "Hall",      HALL_GPIO_Port,      HALL_Pin,      GPIO_PIN_RESET, LD4_GPIO_Port,       LD4_Pin },
  { "IR",        IR_OBST_GPIO_Port,   IR_OBST_Pin,   GPIO_PIN_RESET, LD5_GPIO_Port,       LD5_Pin },
  { "Vibration", VIBRATION_GPIO_Port, VIBRATION_Pin, GPIO_PIN_SET,   LD6_GPIO_Port,       LD6_Pin },
  { "Touch",     TOUCH_GPIO_Port,     TOUCH_Pin,     GPIO_PIN_SET,   TL_GREEN_GPIO_Port,  TL_GREEN_Pin },
  { "Flame",     FLAME_GPIO_Port,     FLAME_Pin,     GPIO_PIN_RESET, TL_YELLOW_GPIO_Port, TL_YELLOW_Pin },
};

#define SENSOR_COUNT (sizeof(sensors) / sizeof(sensors[0]))
```

**Fix the `activeLevel` column to match your measurements from step 1.**

Remove `TrafficLight_Update()` and the old LED blinking from `app.c`, because those LEDs now belong to the alarm.

### Step 3: Poll and detect edges

```c
void Alarm_Init(void)
{
}

void Alarm_Update(void)
{
  for (uint32_t i = 0; i < SENSOR_COUNT; i++)
  {
    Sensor *s = &sensors[i];
    uint8_t active = HAL_GPIO_ReadPin(s->port, s->pin) == s->activeLevel;

    HAL_GPIO_WritePin(s->ledPort, s->ledPin, active ? GPIO_PIN_SET : GPIO_PIN_RESET);

    if (active && !s->wasActive)
    {
      LOG("%s triggered\n", s->name);
    }
    s->wasActive = active;
  }
}
```

`active && !wasActive` is a **rising edge**: it's true only on the one call where the sensor changes from idle to triggered. Without it, you'd print thousands of lines per second while the sensor is active.

### Step 4: Arm, latch, siren
Add the alarm logic:

```c
static uint8_t armed;
static uint8_t alarmOn;
static const char *alarmSource;
```

- `Alarm_ToggleArmed()`: flips `armed`, clears `alarmOn`, turns the red light on while armed.
- In `Alarm_Update()`: if `armed` and a sensor has a rising edge, set `alarmOn = 1` and remember `alarmSource = s->name`.
- While `alarmOn`: alternate the buzzer between two tones every 250 ms (reuse the `Tone()` idea from lesson 02) and blink the red light.

Call `Alarm_ToggleArmed()` when the blue button is pressed. Write this part yourself; the pieces are all from lessons 01–02.

Give yourself an **exit delay**: after arming, ignore sensors for 10 s so you can walk away (the PIR will see you).

## Check it works
- [ ] Each sensor lights its LED only while triggered
- [ ] Each trigger logs exactly one line
- [ ] Armed + any trigger → siren continues even after the sensor goes idle
- [ ] Blue button stops the siren and disarms

## Troubleshooting
| Symptom | Cause |
|---|---|
| PIR triggers all the time | still warming up, sensitivity too high, or a heater/sun in view |
| IR always triggered | range pot too high, or a nearby surface reflects |
| Flame reacts to sunlight / lamps | it's an IR sensor, so lower sensitivity |
| Vibration flickers | that's normal: a spring contact bounces. Treat any edge within 500 ms as one event |
| Hall never reacts | try the other side of the magnet |

## Challenges
1. Add a 5-second **entry delay**: when the alarm trips, the siren only starts if you don't disarm within 5 s.
2. **Zones**: give each sensor an `enabled` flag. The touch sensor cycles which sensor you're editing, the button module toggles it.
3. Rewrite the inputs to use EXTI interrupts. What happens with the vibration sensor? Which approach is better here and why?
