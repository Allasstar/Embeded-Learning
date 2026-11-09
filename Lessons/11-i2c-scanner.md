# Lesson 11: I2C Scanner and First I2C Device

## Goal
Find every device on the I2C bus, prove the scanner works with a device you switch on and off, then read and write your first I2C chip: the PCF8591 ADC/DAC.

## What you learn
- How I2C works: two wires, addresses, ACK/NACK, pull-ups
- 7-bit vs 8-bit address (HAL's `<< 1`)
- Write-then-read transactions
- Reading a datasheet to find a device's protocol
- Debugging a bus that doesn't answer

## How I2C works
- Two shared wires: **SCL** (clock, driven by the master) and **SDA** (data).
- Every device on the bus has a **7-bit address**. Many devices share the same two wires.
- Lines are **open-drain**: devices can only pull them LOW. **Pull-up resistors** pull them HIGH. Most modules have pull-ups on board.
- A transaction: START → address + R/W bit → device answers **ACK** (pulls SDA low) → data bytes, each ACKed → STOP.
- No device with that address → nobody pulls SDA low → **NACK** → HAL returns `HAL_ERROR` and `HAL_I2C_GetError()` is `HAL_I2C_ERROR_AF` ("acknowledge failure").

HAL wants the address **shifted left by 1** (`addr << 1`), because the lowest bit is the R/W bit. Datasheets give 7-bit addresses (e.g. `0x48`); HAL calls use `0x48 << 1` = `0x90`.

## Parts and wiring
I2C1 is already enabled in this project on **PB6 (SCL)** and **PB9 (SDA)** at 100 kHz. It's used for the on-board audio DAC (CS43L22), and other devices can share the bus.

| PCF8591 pin | Board pin |
|---|---|
| VCC | 3V |
| GND | GND |
| SCL | PB6 |
| SDA | PB9 |

**Only 3.3 V-powered modules go on I2C1.** A module powered from 5 V has pull-ups to 5 V and would put 5 V on every chip on the bus. The 5 V LCD gets its own bus in lesson 12.

## CubeMX setup
Nothing. I2C1 is already set up. (Check: **Connectivity → I2C1**, Speed Mode *Standard*, 100000 Hz.)

## Steps

### Step 1: Scanner

```c
#include "main.h"
#include "log.h"

extern I2C_HandleTypeDef hi2c1;

static void I2cScan(I2C_HandleTypeDef *bus)
{
  LOG("Scanning...\n");
  for (uint16_t address = 1; address < 128; address++)
  {
    if (HAL_I2C_IsDeviceReady(bus, address << 1, 2, 5) == HAL_OK)
      LOG("  found 0x%02X\n", address);
  }
  LOG("done\n");
}
```

Call `I2cScan(&hi2c1)` from `App_Start()`; `LOG` prints to the debug console (Debug build). To scan again without reflashing, call it when the blue button is pressed.

Run it with only the PCF8591 connected. Expected: `found 0x48`.

### Step 2: Prove it with the audio chip
The CS43L22 audio DAC is on the same bus at **0x4A**, but it's held in reset by **PD4** (`Audio_RST`), and a chip in reset doesn't answer.

1. Scan: 0x4A missing.
2. In `App_Start()`, after the first scan, do `HAL_GPIO_WritePin(Audio_RST_GPIO_Port, Audio_RST_Pin, GPIO_PIN_SET);` (check the label in `main.h`).
3. Scan again: **0x4A appears**.

Now you know the scanner really sees the bus. This "change one thing, observe" approach is how you debug hardware.

### Step 3: Scan everything
Connect all 3.3 V I2C modules in parallel (all SCL together, all SDA together), one at a time, scanning after each. Fill in the table:

| Module | Expected 7-bit address | Found |
|---|---|---|
| PCF8591 | 0x48 | |
| OLED | 0x3C (sometimes 0x3D) | |
| MPU6050 | 0x68 (0x69 if AD0 high) | |
| Atmospheric pressure (BMP280/BMP180) | 0x76 or 0x77 | |
| Laser ranging (VL53L0X) | 0x29 | |
| Heart rate (MAX30102) | 0x57 | |

Unexpected address? Look the number up: it tells you which chip you really have.

### Step 4: Read the PCF8591
From the PCF8591 datasheet:
- Write a **control byte** to choose the input channel: `0x40 | channel` (bit 6 = enable analog output; keep it set so the DAC works too).
- Then read bytes. Each read returns a conversion, but **the first byte is the result of the previous conversion**, so read 2 and use the second.

```c
#define PCF8591_ADDR (0x48 << 1)

static uint8_t Pcf8591_Read(uint8_t channel)
{
  uint8_t control = 0x40 | (channel & 0x03);
  uint8_t data[2];
  HAL_I2C_Master_Transmit(&hi2c1, PCF8591_ADDR, &control, 1, 10);
  HAL_I2C_Master_Receive(&hi2c1, PCF8591_ADDR, data, 2, 10);
  return data[1];
}
```

Print all four channels with `LOG` every 500 ms from `App_Update()`. The module has jumpers connecting some inputs to an on-board **photoresistor**, **thermistor** and **potentiometer**. Cover the board, warm it with your finger, turn its pot, and work out which channel is which. Don't trust online pinouts; measure.

The result is 8-bit (0..255). Compare with the STM32's 12-bit ADC from lesson 08.

### Step 5: Write the DAC
Control byte, then the output value:

```c
static void Pcf8591_Write(uint8_t value)
{
  uint8_t data[2] = { 0x40, value };
  HAL_I2C_Master_Transmit(&hi2c1, PCF8591_ADDR, data, 2, 10);
}
```

The module's small LED is on the analog output. Ramp the value 0→255 and watch it fade.

### Step 6: Check return values
Every HAL I2C function returns a status. Ignoring it means a disconnected wire silently gives you garbage. Print errors:

```c
HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(&hi2c1, PCF8591_ADDR, &control, 1, 10);
if (status != HAL_OK)
  LOG("I2C error %d, code 0x%lX\n", status, HAL_I2C_GetError(&hi2c1));
```

Unplug SDA while it runs and see what you get.

## Check it works
- [ ] Scanner finds 0x48 with the PCF8591 connected
- [ ] 0x4A appears only after releasing the audio reset
- [ ] All 3.3 V I2C modules found at the same time
- [ ] PCF8591 channels react to light/heat/pot
- [ ] DAC LED fades

## Troubleshooting
| Symptom | Cause |
|---|---|
| Nothing found at all | SDA/SCL swapped, no GND, module not powered |
| Every address "found" | SDA stuck LOW: a device is hanging the bus. Power-cycle everything |
| Scan hangs | Timeout too large, or a broken pull-up, so lines never go HIGH |
| `HAL_BUSY` forever after an error | the I2C peripheral got stuck: re-init with `HAL_I2C_DeInit` + `HAL_I2C_Init` |
| PCF8591 value lags one reading behind | you used the first byte instead of the second |

## Challenges
1. Make the scanner print a table like Linux `i2cdetect` (16 columns, `--` for empty).
2. Use the PCF8591 DAC to generate a slow sine wave (precomputed 32-entry table). Measure AOUT with an STM32 ADC channel and plot both.
3. Switch I2C1 to **400 kHz** (Fast Mode) and time a 100-read loop at both speeds.
