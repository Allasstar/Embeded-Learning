# Embedded Lessons for the STM32F4-DISC1 + Sensor Kit

Twenty lessons, ordered so each one teaches **one new embedded concept** using parts from [COMPONENTS.md](../COMPONENTS.md). Do them in order, but each lesson is **self-contained**: it starts from a clean `master` in its own branch, so it never depends on code or wiring from an earlier lesson.

| # | Lesson | New concept | Main parts |
|---|---|---|---|
| 01 | [Traffic light](01-traffic-light.md) | GPIO output, non-blocking code, state machines | LED Traffic Light |
| 02 | [Doorbell](02-doorbell.md) | GPIO input, EXTI interrupts, debouncing, `volatile` | Button, Passive Buzzer |
| 03 | [Alarm panel](03-alarm-panel.md) | Reading digital sensors, active-low vs active-high, tables of devices | PIR, Hall, IR, Vibration, on-board LEDs |
| 04 | [LED fading](04-pwm-led-fading.md) | Timers, PWM, duty cycle, gamma | On-board LEDs, RGB LED |
| 05 | [Buzzer melody](05-buzzer-melody.md) | Changing PWM frequency, preload registers | Passive Buzzer |
| 06 | [Servo](06-servo.md) | Precise pulse timing, external power | 9G Servo |
| 07 | [Ultrasonic distance](07-ultrasonic.md) | Input capture, measuring time in hardware | Ultrasonic |
| 08 | [Analog sensors](08-adc.md) | ADC, polling vs DMA, filtering | Potentiometer, Joystick, Photoresistor |
| 09 | [Joystick → servo](09-joystick-servo.md) | Combining peripherals, calibration, mapping | Joystick, Servo |
| 10 | [Bluetooth console](10-uart-bluetooth.md) | UART, interrupt RX, ring buffer, command parser | Bluetooth |
| 11 | [I2C scanner](11-i2c-scanner.md) | I2C bus, addresses, first I2C device | PCF8591, any I2C module |
| 12 | [LCD 1602](12-lcd1602.md) | Writing a driver from a datasheet, I/O expanders | I2C LCD 1602 |
| 13 | [OLED display](13-oled.md) | Framebuffers, bulk transfers, fonts | OLED Screen |
| 14 | [Tilt meter](14-mpu6050.md) | Multi-byte registers, signed data, sensor fusion | MPU6050, on-board LEDs |
| 15 | [Weather station](15-weather-station.md) | Calibration data, bit-banged protocols, µs timing | Pressure, DHT11, DS18B20 |
| 16 | [Motor speed](16-motor-speed.md) | H-bridge, high-frequency PWM, measuring RPM | L9110, TT Motor, Speed Module |
| 17 | [PID speed control](17-pid.md) | Closed-loop control, tuning | L9110, TT Motor, Speed Module |
| 18 | [Plant watering system](18-plant-watering.md) | Putting it all together, RTC, encoder mode | Soil, Pump, DS1302, Rotary Encoder, OLED |
| 19 | [Wi-Fi sensor node](19-esp8266-wifi.md) | AT command state machines, networking | ESP8266 |
| 20 | [FreeRTOS](20-freertos.md) | Tasks, queues, mutexes | on-board LEDs, blue button, internal temperature sensor |

Each lesson has: **Goal**, **What you learn**, **Parts and wiring**, **CubeMX setup**, numbered **Steps** with code, **Check it works**, **Troubleshooting** and **Challenges**. Do the challenges: that's where you really learn.

---

## Conventions used in every lesson

### 0. One branch per lesson, always from clean `master`

```
git switch master
git switch -c 03-alarm-panel
```

`master` holds only the CubeMX base project, the lesson texts and the logging helper. Your lesson code and CubeMX changes live in the lesson's branch and are **never merged back**, so the next lesson starts clean. Consequences:

- A lesson never reuses code, CubeMX settings or wiring from an earlier lesson. Where a lesson needs an output it uses the on-board LEDs (LD3–LD6) and the blue button.
- Unplug the previous lesson's parts from the breadboard before you start. The lesson's wiring table lists everything you need.
- Lesson texts that mention an earlier lesson do it only as a pointer ("see lesson 05") or as an optional challenge.

### 1. One module per lesson

Put each lesson's code in its own pair of files, not in `main.c`:

```
Core/Inc/traffic_light.h
Core/Src/traffic_light.c
```

Then add the `.c` file to [CMakeLists.txt](../CMakeLists.txt):

```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/app.c
    Core/Src/log.c
    Core/Src/traffic_light.c
)
```

Call the module from [app.c](../Core/Src/app.c): `Xxx_Init()` from `App_Start()`, `Xxx_Update()` from `App_Update()`. `main.c` already calls `App_Start()` and `App_Update()`, so you never need to touch it.

### 2. `Update()` functions never block

`App_Update()` runs thousands of times per second. Each module's `Update()` checks the time, does a little work and returns. No `HAL_Delay()` outside of init code. Lesson 01 explains why.

### 3. HAL callbacks live in `app.c`

A HAL callback like `HAL_GPIO_EXTI_Callback` can only be defined **once** in the whole program. Define each callback once in `app.c` and forward to the module:

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BTN_Pin) Doorbell_OnPress();
}
```

If a challenge adds a second source (another EXTI pin, another UART), add one `if` per source in the same function.

### 4. CubeMX workflow

1. Open `Test_DISC1.ioc` in CubeMX.
2. Click a pin → choose its function (e.g. `GPIO_Output`, `TIM4_CH1`).
3. Right-click the pin → **Enter User Label** (e.g. `TL_RED`). CubeMX then generates `TL_RED_Pin` and `TL_RED_GPIO_Port` in `main.h`.
4. Configure the peripheral in the left panel.
5. **Generate Code**. Your code inside `USER CODE BEGIN/END` blocks survives; everything else is rewritten.
6. Rebuild.

### 5. Commit inside the lesson's branch

`git commit` after each step that works. If you break something, you can see exactly what changed. Push the branch when the lesson is done, then switch back to `master` for the next one.

---

## Pin map (all lessons)

This is a per-lesson reference. Lessons never share a build, so two lessons may use the same pin differently (PE5 is a plain GPIO in lesson 02 and `TIM9_CH1` in lesson 05). The pins were chosen so the board's built-in parts (accelerometer, audio, USB, microphone) keep working.

| Lesson | Signal | Pin | Pin mode |
|---|---|---|---|
| 01 | Traffic light R / Y / G | PD0 / PD1 / PD2 | GPIO_Output |
| 02 | Button module | PE4 | GPIO_EXTI4 |
| 02 | Passive buzzer | PE5 | GPIO_Output |
| 05 | Passive buzzer | PE5 | TIM9_CH1 |
| 03 | PIR / Hall / IR / Vibration | PE7 / PE8 / PE10 / PE12 | GPIO_Input (Hall: pull-up) |
| 03 (challenge) | Touch / Flame | PE13 / PE15 | GPIO_Input |
| 04 | On-board LEDs | PD12–PD15 | TIM4_CH1–CH4 |
| 04 | RGB LED R / G / B | PB4 / PB5 / PB0 | TIM3_CH1 / CH2 / CH3 |
| 06 | Servo signal | PB14 | TIM12_CH1 |
| 07 | Ultrasonic Echo / Trig | PE9 / PE11 | TIM1_CH1 (input capture) / TIM1_CH2 (PWM) |
| 08 | Potentiometer | PA1 | ADC1_IN1 |
| 08 | Joystick X / Y | PC1 / PC2 | ADC1_IN11 / ADC1_IN12 |
| 08 | Photoresistor AO | PC4 | ADC1_IN14 |
| 09 | Joystick SW | PE14 | GPIO_Input, pull-up |
| 09 | Servo signal | PB14 | TIM12_CH1 |
| 09 | Joystick X / Y | PC1 / PC2 | ADC1_IN11 / ADC1_IN12 |
| 09 | On-board LED LD4 | PD12 | TIM4_CH1 |
| 10 | Bluetooth TX→ / ←RX | PA2 / PA3 | USART2_TX / USART2_RX |
| 11, 13–15, 18 | I2C bus for 3.3 V modules | PB6 SCL / PB9 SDA | I2C1 (already enabled) |
| 12 | I2C bus for the 5 V LCD | PA8 SCL / PC9 SDA | I2C3 |
| 15 | DHT11 data / DS18B20 data | PD6 / PD7 | GPIO_Output, open-drain |
| 16 | L9110 A-IA / A-IB | PB8 / PB7 | TIM10_CH1 / GPIO_Output |
| 16, 17 | Speed module DO | PD3 | GPIO_EXTI3 |
| 18 | Soil AO | PC5 | ADC1_IN15 |
| 18 | L9110 B-IA (pump) | PB12 | GPIO_Output |
| 18 | DS1302 CE / CLK / IO | PD10 / PD11 / PB15 | GPIO_Output / GPIO_Output / GPIO_Output open-drain |
| 18 | Rotary encoder CLK / DT / SW | PA15 / PB3 / PC11 | TIM2_CH1 / TIM2_CH2 (encoder mode) / GPIO_Input pull-up |
| 18 | Relay IN (optional) | PC8 | GPIO_Output |
| 19 | ESP8266 TX→ / ←RX | PD8 / PD9 | USART3_TX / USART3_RX |
| 20 | Blue button | PA0 | GPIO_EXTI0 |

**Don't use:** PA0 (user button), PA4–PA7 + PE3 + PE0/PE1 (accelerometer), PB6/PB9 except as I2C1, PC3 + PB10 (microphone), PC7/PC10/PC12/PD4 (audio), PA9–PA12 + PC0 + PD5 (USB), PA13/PA14 (debugger), PC14/PC15 (32 kHz crystal), PH0/PH1 (8 MHz crystal).

## Power rules (read before wiring anything)

- **GND of every module connects to a GND pin on the board.** No common ground = random behaviour.
- **3.3 V logic.** Power sensors from the board's `3V` pin unless the lesson says 5 V. A sensor powered from 5 V can send 5 V back into the board.
- **ADC pins are not 5 V tolerant.** Analog outputs must stay ≤ 3.3 V.
- **Motors, servo, pump, ESP8266:** power them from the breadboard power module (5 V rail), not from the board. Connect the power module's GND to the board's GND.
- **Wire with the USB cable unplugged.** Check twice, then plug in.
- **Never connect anything to mains (230/110 V)**, not even through the relay.
