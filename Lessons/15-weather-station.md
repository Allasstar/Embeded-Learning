# Lesson 15: Weather Station

## Goal
Read three temperature sensors with three different protocols (pressure sensor on I2C, DHT11 on a timing-critical single wire, DS18B20 on 1-Wire) and show temperature, humidity and pressure on the LCD.

## What you learn
- Factory **calibration data** and integer compensation formulas from a datasheet
- Microsecond timing with the **DWT cycle counter**
- **Bit-banging** a protocol: driving and reading a pin by hand
- Open-drain pins: one wire for both directions
- Critical sections: when and how long to disable interrupts
- Non-blocking waits for slow sensors

## Parts and wiring

| Module | Pin | Board pin |
|---|---|---|
| Atmospheric Pressure | VCC / GND / SCL / SDA | 3V / GND / PB6 / PB9 |
| DHT11 | VCC / GND / DATA (S) | 3V / GND / PD6 |
| DS18B20 | VCC / GND / DATA (S) | 3V / GND / PD7 |
| LCD 1602 | as lesson 12 | I2C3 |

## CubeMX setup
1. **PD6** → `GPIO_Output`, label `DHT`. **PD7** → `GPIO_Output`, label `DS18B20`.
2. For both (System Core → GPIO): Output level **High**, Mode **Output Open Drain**, Pull **Pull-up**, Speed Low.
3. Generate.

**Open drain** = the pin can only pull LOW or let go. When it lets go, the pull-up resistor (on the module, plus the internal one) brings the line HIGH, and the sensor can then pull it LOW itself. `HAL_GPIO_ReadPin` reads the real line level even while the pin is an output. So one pin both talks and listens, with no switching between input and output.

---

## Part 1: Pressure sensor (BMP280)

### Step 1: Which chip is it?
Scan I2C1: expect **0x76** or **0x77**. Read register **0xD0** (chip ID):

| ID | Chip |
|---|---|
| 0x58 | BMP280: continue with this lesson |
| 0x60 | BME280: same as BMP280 plus humidity, so this lesson works |
| 0x55 | BMP180: different protocol; follow the BMP180 datasheet instead (good exercise) |

### Step 2: Read calibration
Every chip is measured at the factory, and 12 correction coefficients are stored in it at 0x88–0x9F (little-endian, low byte first).

```c
extern I2C_HandleTypeDef hi2c1;

#define BMP_ADDR (0x76 << 1)

static uint16_t T1, P1;
static int16_t T2, T3, P2, P3, P4, P5, P6, P7, P8, P9;
static int32_t tFine;

static uint16_t Le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

void Bmp_Init(void)
{
  uint8_t c[24];
  HAL_I2C_Mem_Read(&hi2c1, BMP_ADDR, 0x88, I2C_MEMADD_SIZE_8BIT, c, 24, 10);
  T1 = Le16(&c[0]);  T2 = Le16(&c[2]);  T3 = Le16(&c[4]);
  P1 = Le16(&c[6]);  P2 = Le16(&c[8]);  P3 = Le16(&c[10]);
  P4 = Le16(&c[12]); P5 = Le16(&c[14]); P6 = Le16(&c[16]);
  P7 = Le16(&c[18]); P8 = Le16(&c[20]); P9 = Le16(&c[22]);

  uint8_t ctrl = 0x27;
  HAL_I2C_Mem_Write(&hi2c1, BMP_ADDR, 0xF4, I2C_MEMADD_SIZE_8BIT, &ctrl, 1, 10);
}
```

`0x27` in `ctrl_meas`: temperature ×1 oversampling, pressure ×1, **normal mode** (measures continuously by itself).

Compare with the MPU6050: that was big-endian. Always check byte order in the datasheet.

### Step 3: Read and compensate
Read 6 bytes from 0xF7 in one burst (pressure and temperature from the same measurement), build two 20-bit values, then apply the compensation formulas **copied exactly from the BMP280 datasheet, section "Compensation formula in 32/64-bit fixed point"**:

```c
void Bmp_Read(int32_t *centiCelsius, uint32_t *pascal)
{
  uint8_t d[6];
  HAL_I2C_Mem_Read(&hi2c1, BMP_ADDR, 0xF7, I2C_MEMADD_SIZE_8BIT, d, 6, 10);
  int32_t adcP = (d[0] << 12) | (d[1] << 4) | (d[2] >> 4);
  int32_t adcT = (d[3] << 12) | (d[4] << 4) | (d[5] >> 4);

  int32_t v1 = ((((adcT >> 3) - ((int32_t)T1 << 1))) * T2) >> 11;
  int32_t v2 = (((((adcT >> 4) - (int32_t)T1) * ((adcT >> 4) - (int32_t)T1)) >> 12) * T3) >> 14;
  tFine = v1 + v2;
  *centiCelsius = (tFine * 5 + 128) >> 8;

  int64_t p1 = (int64_t)tFine - 128000;
  int64_t p2 = p1 * p1 * P6;
  p2 += (p1 * P5) << 17;
  p2 += ((int64_t)P4) << 35;
  p1 = ((p1 * p1 * P3) >> 8) + ((p1 * P2) << 12);
  p1 = ((((int64_t)1) << 47) + p1) * P1 >> 33;
  if (p1 == 0)
  {
    *pascal = 0;
    return;
  }
  int64_t p = 1048576 - adcP;
  p = (((p << 31) - p2) * 3125) / p1;
  p1 = ((int64_t)P9 * (p >> 13) * (p >> 13)) >> 25;
  p2 = ((int64_t)P8 * p) >> 19;
  p = ((p + p1 + p2) >> 8) + ((int64_t)P7 << 4);
  *pascal = (uint32_t)(p / 256);
}
```

Don't try to understand the formula: it's Bosch's curve fit. Copying a vendor formula *exactly* (types and shifts included) is a real skill. Temperature must be computed first because pressure uses `tFine`.

Expected: ~2000–2500 (20–25 °C) and ~95 000–103 000 Pa (950–1030 hPa, depends on altitude and weather).

---

## Part 2: Microsecond timing (DWT)

`HAL_Delay` has 1 ms resolution: far too coarse for DHT11 and 1-Wire. The Cortex-M4 has a **cycle counter** (DWT->CYCCNT) that counts every CPU clock: at 168 MHz, 168 counts per µs.

`Core/Src/delay_us.c`:

```c
#include "delay_us.h"
#include "main.h"

void DelayUs_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t Micros(void)
{
  return DWT->CYCCNT / (SystemCoreClock / 1000000);
}

void DelayUs(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t cycles = us * (SystemCoreClock / 1000000);
  while (DWT->CYCCNT - start < cycles)
  {
  }
}
```

Call `DelayUs_Init()` once in `App_Start()`. (`Micros()` wraps every 25 s; fine for measuring short pulses with the `now - start` pattern.)

---

## Part 3: DHT11

### The protocol (from the DHT11 datasheet)
```
MCU:  pull LOW ≥18 ms, then release
DHT:  pulls LOW 80 µs, HIGH 80 µs        ← "I'm here"
DHT:  40 bits, each: LOW 50 µs, then HIGH 26–28 µs (= 0) or 70 µs (= 1)
Data: humidity int, humidity dec, temp int, temp dec, checksum
checksum = low 8 bits of the sum of the first four bytes
```

The information is in the **length of the HIGH pulse**. Measure it; anything > 40 µs is a 1.

### Step 4: Code

```c
#include "delay_us.h"

static int32_t WaitFor(GPIO_PinState level, uint32_t timeoutUs)
{
  uint32_t start = Micros();
  while (HAL_GPIO_ReadPin(DHT_GPIO_Port, DHT_Pin) != level)
  {
    if (Micros() - start > timeoutUs)
      return -1;
  }
  return Micros() - start;
}

static int ReadBits(uint8_t data[5])
{
  if (WaitFor(GPIO_PIN_RESET, 100) < 0) return 0;
  if (WaitFor(GPIO_PIN_SET, 100) < 0) return 0;
  if (WaitFor(GPIO_PIN_RESET, 100) < 0) return 0;

  for (int i = 0; i < 40; i++)
  {
    if (WaitFor(GPIO_PIN_SET, 80) < 0) return 0;
    int32_t high = WaitFor(GPIO_PIN_RESET, 100);
    if (high < 0) return 0;
    data[i / 8] <<= 1;
    if (high > 40) data[i / 8] |= 1;
  }
  return 1;
}

int Dht_Read(int32_t *celsius, int32_t *humidity)
{
  uint8_t data[5] = { 0 };

  HAL_GPIO_WritePin(DHT_GPIO_Port, DHT_Pin, GPIO_PIN_RESET);
  HAL_Delay(20);
  HAL_GPIO_WritePin(DHT_GPIO_Port, DHT_Pin, GPIO_PIN_SET);

  __disable_irq();
  int ok = ReadBits(data);
  __enable_irq();

  if (!ok || (uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4])
    return 0;

  *humidity = data[0];
  *celsius = data[2];
  return 1;
}
```

**Why disable interrupts?** The bits last 50–120 µs. If the USB or SysTick interrupt fires in the middle of a measurement, the pulse looks longer than it was and a 0 becomes a 1. The whole read takes ~5 ms, short enough that pausing interrupts is acceptable. Keep critical sections as short as possible, and **always** re-enable on every path (that's why the reading is in its own function with a single exit point around it).

The 20 ms LOW start pulse uses `HAL_Delay`. That's a block, but a short one. Read the DHT11 no more often than every **2 s** (it won't give new data faster).

---

## Part 4: DS18B20 (1-Wire)

### The protocol (from the DS18B20 datasheet, "1-Wire Signaling")
Everything is built from three timed operations:

| Operation | MCU does |
|---|---|
| **Reset** | LOW 480 µs, release, wait 70 µs, read: LOW = device present. Wait 410 µs |
| **Write 1** | LOW 6 µs, release, wait 64 µs |
| **Write 0** | LOW 60 µs, release, wait 10 µs |
| **Read bit** | LOW 3 µs, release, wait 10 µs, read line, wait 53 µs |

Bytes are sent **LSB first**.

### Step 5: Code

```c
static void Line(GPIO_PinState level)
{
  HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, level);
}

static int OneWire_Reset(void)
{
  Line(GPIO_PIN_RESET);
  DelayUs(480);
  Line(GPIO_PIN_SET);
  DelayUs(70);
  int present = HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_RESET;
  DelayUs(410);
  return present;
}

static void OneWire_WriteBit(int bit)
{
  __disable_irq();
  Line(GPIO_PIN_RESET);
  DelayUs(bit ? 6 : 60);
  Line(GPIO_PIN_SET);
  __enable_irq();
  DelayUs(bit ? 64 : 10);
}

static int OneWire_ReadBit(void)
{
  __disable_irq();
  Line(GPIO_PIN_RESET);
  DelayUs(3);
  Line(GPIO_PIN_SET);
  DelayUs(10);
  int bit = HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin) == GPIO_PIN_SET;
  __enable_irq();
  DelayUs(53);
  return bit;
}

static void OneWire_WriteByte(uint8_t value)
{
  for (int i = 0; i < 8; i++)
    OneWire_WriteBit((value >> i) & 1);
}

static uint8_t OneWire_ReadByte(void)
{
  uint8_t value = 0;
  for (int i = 0; i < 8; i++)
    value |= OneWire_ReadBit() << i;
  return value;
}
```

Here interrupts are disabled **per bit**, only for the timing-critical part (a few µs), not for the whole transaction. Compare with the DHT11, where the sensor sends everything in one burst and you can't pause between bits.

### Step 6: Non-blocking temperature
A 12-bit conversion takes up to **750 ms**. Don't wait: start it, do other things, come back.

```c
#define CMD_SKIP_ROM   0xCC
#define CMD_CONVERT    0x44
#define CMD_READ_SCRATCH 0xBE

static uint32_t convertStart;
static uint8_t converting;
static int32_t dsTenths;

void Ds18b20_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (!converting)
  {
    if (OneWire_Reset())
    {
      OneWire_WriteByte(CMD_SKIP_ROM);
      OneWire_WriteByte(CMD_CONVERT);
      converting = 1;
      convertStart = now;
    }
  }
  else if (now - convertStart >= 750)
  {
    converting = 0;
    if (OneWire_Reset())
    {
      OneWire_WriteByte(CMD_SKIP_ROM);
      OneWire_WriteByte(CMD_READ_SCRATCH);
      uint8_t lsb = OneWire_ReadByte();
      uint8_t msb = OneWire_ReadByte();
      int16_t raw = (int16_t)((msb << 8) | lsb);
      dsTenths = raw * 10 / 16;
    }
  }
}
```

The raw value is in 1/16 °C, signed: `0x0191` = 401 → 25.06 °C; `0xFF5E` = -162 → -10.1 °C.

*Skip ROM* (0xCC) addresses "everyone": fine with one sensor on the wire.

---

## Part 5: Display

Every 2 s, read the DHT11 and the BMP280, and show on the LCD:

```
T 23.4 21.9 22.0
H 45%  P 1013hPa
```

(BMP280, DS18B20, DHT11 temperatures on line 1.)

Formatting tenths without `%f`: `"%ld.%ld", t / 10, labs(t % 10)` (careful with values between -1 and 0).

The three temperatures will disagree by 1–2 °C. Why? DHT11 is ±2 °C, BMP280 gets warmed by its own board, DS18B20 is ±0.5 °C. Which do you trust? Breathe on each and watch how fast each reacts.

## Check it works
- [ ] BMP280 chip ID matches, temperature plausible, pressure 950–1050 hPa
- [ ] DHT11 checksum passes on almost every read
- [ ] DS18B20 present pulse detected; temperature plausible
- [ ] Display refreshes every 2 s, other modules keep running

## Troubleshooting
| Symptom | Cause |
|---|---|
| BMP280 pressure absurd | calibration types wrong (`P1`/`T1` unsigned, others signed), or temp not computed first |
| DHT11 always times out | pin not open-drain, no pull-up, or `DelayUs_Init` not called |
| DHT11 checksum fails sometimes | interrupts not disabled during read |
| DS18B20 "not present" | wrong pin, no pull-up, or VCC/GND swapped (it gets hot!) |
| DS18B20 reads 85.0 °C | that's the power-on value: you read before a conversion finished |
| DS18B20 reads -0.1 / 4095 | read returned all 1s: line stuck HIGH, sensor missing |

## Challenges
1. Check the DS18B20 **CRC** (byte 9 of the scratchpad, Dallas/Maxim CRC-8).
2. Compute **altitude** from pressure: `h = 44330 × (1 − (p / p0)^(1/5.255))`, p0 = today's sea-level pressure from a weather site.
3. Log all values over Bluetooth (lesson 10) every minute for a day and plot them.
4. Replace the DHT11's polling with **timer input capture** (lesson 07): no disabling interrupts needed.
