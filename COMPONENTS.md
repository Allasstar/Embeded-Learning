# Sensor Kit Components

Kit tutorials: https://umsk.rtfd.io

Chip names marked *(likely)* are the usual chip for that module type. Check the marking on your board to be sure.

Interface key: **DO** = digital out (HIGH/LOW, threshold set by the on-board pot), **AO** = analog out (read with the ADC), **I2C / SPI / UART / 1-Wire** = protocol.

## Sensors: environment

| Module | Qty | Interface | Notes |
|---|---|---|---|
| DHT11 Module | 1 | single-wire (custom timing) | temperature + humidity; needs µs-accurate timing (use a timer, not `HAL_Delay`) |
| DS18B20 Module | 1 | 1-Wire | precise temperature probe, -55..125 °C |
| Atmospheric Pressure Module | 1 | I2C | BMP280 / BMP180 *(likely)*; pressure + temperature |
| MQ-2 Gas Module | 1 | DO + AO | smoke / LPG / methane; heater draws ~150 mA at 5 V, needs warm-up |
| Raindrops Detection Module | 1 | DO + AO | sensing plate + LM393 comparator board |
| Soil Moisture Module | 1 | DO + AO | probe + comparator board |
| Water Level Module | 1 | AO | exposed traces, reading rises with water depth |
| Photoresistor Module | 1 | DO + AO | light level |
| Flame Module | 1 | DO + AO | IR flame sensor |

## Sensors: motion, distance, position

| Module | Qty | Interface | Notes |
|---|---|---|---|
| MPU6050 Module | 1 | I2C (0x68) | 3-axis accelerometer + 3-axis gyro |
| Ultrasonic Module | 1 | GPIO (Trig / Echo) | HC-SR04 *(likely)*; 2..400 cm; 5 V supply, Echo is 5 V (use an FT pin) |
| Laser Ranging Module | 1 | I2C | VL53L0X *(likely)*; time-of-flight, up to ~2 m |
| PIR Motion Sensor Module | 1 | DO | HC-SR501 *(likely)*; detects people; 5 V supply, output 3.3 V |
| IR Obstacle Avoidance Module | 1 | DO | IR reflection, range set by pot |
| Hall Switch Module | 1 | DO | detects a magnet |
| Vibration Module | 1 | DO | shake / knock switch |
| Speed Module | 1 | DO | slotted optocoupler; use with the encoder disk to count RPM |
| Heart Rate Module | 1 | I2C | MAX30102 *(likely)*; pins VIN, GND, SCL, SDA, INT |

## Input

| Module | Qty | Interface | Notes |
|---|---|---|---|
| Button Module | 1 | DO | push button with pull resistor |
| Touch Module | 1 | DO | capacitive touch (TTP223 *(likely)*) |
| Potentiometer Module | 1 | AO | knob -> 0..3.3 V |
| Joystick Module | 1 | 2× AO + DO | X / Y axes + press button |
| Rotary Encoder Module | 1 | 2× DO (A/B) + DO (SW) | KY-040 *(likely)*; endless knob; timer encoder mode can count it in hardware |

## Output: display, light, sound

| Module | Qty | Interface | Notes |
|---|---|---|---|
| OLED Screen | 1 | I2C (0x3C) | 0.96" 128×64, SSD1306 *(likely)* |
| I2C LCD 1602 | 1 | I2C (0x27 or 0x3F) | 16×2 text LCD with PCF8574 backpack; 5 V |
| LED Traffic Light Module | 1 | 3× GPIO | red / yellow / green LEDs |
| RGB LED Module | 1 | 3× GPIO / PWM | mix colours with timer PWM |
| Passive Buzzer Module | 1 | PWM | needs a square wave; frequency = pitch |

## Output: actuators

| Module | Qty | Interface | Notes |
|---|---|---|---|
| 9G Servo | 1 | PWM 50 Hz | SG90 *(likely)*; pulse 1..2 ms = 0..180°; power from 5 V, not 3.3 V |
| TT Motor / Encoder Disk | 1 | via driver | yellow DC gear motor + slotted disk for the Speed Module |
| Pump / Tube | 1 | via driver | small DC water pump + tubing |
| L9110 Module | 1 | 2× PWM per motor | dual DC motor driver, ~800 mA per channel; drives the TT motor / pump |
| Relay Module | 1 | DO | switches a separate circuit; check if it triggers on HIGH or LOW |

## Communication and helper chips

| Module | Qty | Interface | Notes |
|---|---|---|---|
| ESP8266 Module / Adapter | 1 | UART (AT commands) | Wi-Fi; ESP-01 + adapter board; default 115200 baud |
| Bluetooth Module | 1 | UART | HC-05 / HC-06 *(likely)*; default 9600 baud |
| PCF8591 Module | 1 | I2C (0x48) | 4-channel 8-bit ADC + 1 DAC |
| DS1302 RTC Module | 1 | 3-wire (CE, IO, SCLK) | real-time clock with backup battery; **not** I2C |

## Power and wiring

| Item | Qty | Notes |
|---|---|---|
| Breadboard + Breadboard Power Module | 1 | power module gives 3.3 V / 5 V rails from barrel jack or USB |
| 9V Battery / Cable | 1 | feeds the breadboard power module |
| Jump Wire F/M | 50 | female (module pin) to male (breadboard / board header) |

## Using them with the STM32F4-DISC1

- **Logic is 3.3 V.** Most DISC1 pins are 5 V tolerant (FT in the datasheet pin table) as **inputs**, but the ADC pins are not: keep every AO signal at 3.3 V max (power analog modules from 3V, or add a divider).
- **5 V parts** (servo, motor, pump, MQ-2, LCD 1602, HC-SR04): take 5 V from the board's `5V` pin or the breadboard power module, and connect all grounds together.
- **Never drive motors, pump or servo straight from a GPIO pin.** Use the L9110 or the relay.
- **I2C addresses on one bus** don't clash: OLED 0x3C, LCD 0x27/0x3F, MPU6050 0x68, PCF8591 0x48, on-board audio DAC 0x4A. Remember HAL wants them shifted: `0x3C << 1`.
- **Pins already used on the board:** PA0 (user button), PD12–PD15 (LEDs), PA5–PA7 + PE3 (accelerometer SPI), PB6/PB9 + PC7/PC10/PC12/PA4 + PD4 (audio), PA9–PA12 (USB). Pick free pins in CubeMX's pinout view.
