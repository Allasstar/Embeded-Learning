# STM32F4DISCOVERY cheatsheet

Quick reference for pins, LEDs, buttons and peripherals in this project (STM32F407VG, HAL, CubeMX).

---

## 0. Glossary

### Port and Pin: what `LD4_GPIO_Port` / `LD4_Pin` really are

The MCU's pins are grouped into **ports** of 16 pins each: `GPIOA`, `GPIOB` … `GPIOI`. A pin name like **PD12** means *Port D, pin 12*.

CubeMX writes these into `main.h` from the user label `LD4`:
```c
#define LD4_Pin        GPIO_PIN_12   // which pin inside the port: a bit mask, 0x1000 (= 1 << 12)
#define LD4_GPIO_Port  GPIOD         // which port: pointer to the GPIOD register block (0x40020C00)
```

So these three lines do exactly the same thing:
```c
HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET);    // by label (preferred)
HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);        // by raw name
GPIOD->BSRR = (1 << 12);                                    // by register
```

| | Port | Pin |
|---|---|---|
| Type | `GPIO_TypeDef *` (a struct of registers) | `uint16_t` bit mask |
| Values | `GPIOA` … `GPIOI` | `GPIO_PIN_0` … `GPIO_PIN_15`, `GPIO_PIN_All` |
| Combine? | no, one port per call | yes, OR them: `LD3_Pin \| LD4_Pin` (same port only) |

Use the labels: if you move the LED to another pin in CubeMX, only `main.h` changes and your code still works.

### Keywords

| Term | Meaning |
|---|---|
| **MCU** | Microcontroller: CPU + flash + RAM + peripherals on one chip. Here the STM32F407VG. |
| **Cortex-M4** | The ARM CPU core inside the STM32F407. |
| **Peripheral** | A hardware block inside the MCU besides the CPU: GPIO, SPI, I2C, TIM, UART, ADC… |
| **Register** | A memory address that controls hardware. Writing a bit there flips a real wire. |
| **HAL** | *Hardware Abstraction Layer*, ST's C library (`HAL_*` functions) so you don't touch registers directly. Lives in `Drivers/STM32F4xx_HAL_Driver/`. |
| **LL** | *Low-Layer* drivers: ST's thinner, faster alternative to HAL (`LL_*`). Not used here. |
| **CMSIS** | ARM's standard headers: register names (`GPIOD->ODR`), `NVIC`, `__disable_irq()`. Lives in `Drivers/CMSIS/`. |
| **BSP** | *Board Support Package*: drivers for a specific board's chips (accelerometer, audio). Not used here. |
| **CubeMX** | ST's graphical configurator. Pick pins/clocks/peripherals, it generates the init code. |
| **`.ioc`** | The CubeMX project file (`Test_DISC1.ioc`). The source of truth for pin setup. |
| **User label** | Name you give a pin in CubeMX (`LD4`). Becomes `LD4_Pin` / `LD4_GPIO_Port`. |
| **`MX_*_Init()`** | Init functions CubeMX generates (`MX_GPIO_Init`, `MX_SPI1_Init`). |
| **MSP** | *MCU Support Package*: `stm32f4xx_hal_msp.c`, where CubeMX sets up pins/clocks a peripheral needs (`HAL_SPI_MspInit`). |
| **GPIO** | *General-Purpose Input/Output*: a pin your code can drive HIGH/LOW or read. |
| **HIGH / LOW** | 3.3 V / 0 V on a pin. In HAL: `GPIO_PIN_SET` / `GPIO_PIN_RESET`. |
| **Push-pull** | Output actively drives both HIGH and LOW. The normal mode. |
| **Open-drain** | Output can only pull LOW; HIGH needs a pull-up resistor. Used for shared lines (I2C). |
| **Pull-up / pull-down** | Weak internal resistor holding an input HIGH / LOW when nothing drives it. |
| **Floating** | Input with nothing driving it and no pull: reads random noise. |
| **Active LOW** | Signal "on" when the wire is LOW (e.g. SPI CS). |
| **AF (Alternate Function)** | Pin handed to a peripheral (SPI, TIM, UART) instead of plain GPIO. |
| **Clock / RCC** | Every peripheral needs its clock enabled (`__HAL_RCC_xxx_CLK_ENABLE()`) or it's dead. RCC is the clock controller. |
| **SYSCLK / HCLK / PCLK** | CPU clock (168 MHz), AHB bus clock, APB bus clocks (APB1 42 MHz, APB2 84 MHz). |
| **SysTick** | ARM core timer firing every 1 ms; drives `HAL_GetTick()` and `HAL_Delay()`. |
| **Tick** | One SysTick period = 1 ms here. |
| **Handle** | Struct describing one peripheral instance (`hspi1`, `hi2c1`). Pass its address to HAL calls. |
| **`HAL_StatusTypeDef`** | Return code: `HAL_OK`, `HAL_ERROR`, `HAL_BUSY`, `HAL_TIMEOUT`. |
| **Blocking** | Function waits until the job is done (or times out) before returning. |
| **Polling** | Checking a flag/pin repeatedly in the loop instead of using an interrupt. |
| **Interrupt / IRQ** | Hardware event that pauses `main` and runs a handler immediately. |
| **ISR / IRQHandler** | The function that runs on an interrupt (`EXTI0_IRQHandler` in `stm32f4xx_it.c`). |
| **NVIC** | *Nested Vectored Interrupt Controller*: enables interrupts and sets their priorities. |
| **EXTI** | *External Interrupt* controller: turns a pin edge into an interrupt. Line N = pin N of any port. |
| **Edge** | Change of a signal: **rising** (LOW→HIGH) or **falling** (HIGH→LOW). |
| **Event mode** | EXTI wakes the CPU but calls no handler. |
| **Callback** | Function *you* write that HAL calls on an event (`HAL_GPIO_EXTI_Callback`). |
| **`__weak`** | Default empty function that yours overrides if it has the exact same name. |
| **`volatile`** | Tells the compiler a variable can change behind its back (ISR, hardware). Always use for ISR-shared flags. |
| **Debounce** | Ignoring the few ms of on/off chatter a mechanical button produces. |
| **DMA** | *Direct Memory Access*: hardware copies data between memory and a peripheral without the CPU. |
| **Timer (TIM)** | Hardware counter. Base for delays, periodic interrupts, PWM, input capture. |
| **Prescaler** | Divides the timer's input clock. `PSC = 84-1` → 84 MHz / 84 = 1 MHz. |
| **Period (ARR)** | Value the timer counts to before restarting. Sets the overflow/PWM frequency. |
| **PWM** | Pin toggled fast with a variable **duty cycle** (% of time HIGH). Dims LEDs, drives servos. |
| **Compare (CCR)** | Timer value at which a PWM output switches; sets the duty cycle. |
| **SPI** | Fast 4-wire bus: **SCK** clock, **MOSI** master-out, **MISO** master-in, **CS/NSS** chip select. |
| **I2C** | 2-wire bus with addresses: **SCL** clock, **SDA** data. Open-drain, needs pull-ups. |
| **I2S** | Audio variant of SPI for streaming samples. |
| **UART / USART** | Serial port: **TX**, **RX**, agreed baud rate (e.g. 115200). |
| **ADC** | *Analog-to-Digital Converter*: voltage 0–3.3 V → number 0–4095. |
| **ODR / IDR / BSRR** | GPIO registers: Output Data, Input Data, Bit Set/Reset. |
| **Flash** | Non-volatile memory where your program lives. "Flashing" = writing it there. |
| **ST-LINK / SWD** | Debugger chip on the board and the 2-wire debug protocol it speaks to the MCU. |
| **Semihosting** | Debug feature: `printf` output goes through the debugger to your PC (see `SEMIHOSTING.md`). |
| **Linker script** | `STM32F407xx_FLASH.ld`: where code and data go in flash/RAM. |
| **Startup file** | `startup_stm32f407xx.s`: vector table and reset code that runs before `main`. |
| **Vector table** | List of handler addresses the CPU jumps to on reset and each interrupt. |
| **NRST** | Reset pin (the black button). |

---

## 1. The golden workflow

Every hardware change follows the same loop:

1. **Configure** in CubeMX: open `Test_DISC1.ioc`, set the pin or peripheral.
2. **Generate Code**: CubeMX rewrites `main.c`, `main.h`, `stm32f4xx_hal_msp.c`, `stm32f4xx_it.c`.
3. **Write your logic** only between `/* USER CODE BEGIN x */` and `/* USER CODE END x */`.
4. **Build** and **flash**:
   ```
   cmake --preset Debug
   cmake --build --preset Debug
   ```
   then F5 in VS Code.

> Code outside the `USER CODE` markers is **deleted** on the next Generate Code.

### Where to put what in `main.c`

| Block | Use it for |
|---|---|
| `USER CODE BEGIN Includes` | `#include <stdio.h>`, your own headers |
| `USER CODE BEGIN PD` | `#define BLINK_MS 500` |
| `USER CODE BEGIN PV` | global variables (`volatile uint8_t buttonPressed;`) |
| `USER CODE BEGIN PFP` | prototypes of your own functions |
| `USER CODE BEGIN 2` | one-time setup after all `MX_*_Init()` calls |
| `USER CODE BEGIN 3` | code that runs every loop iteration |
| `USER CODE BEGIN 4` | your function bodies and HAL callbacks |

---

## 2. On-board hardware map

| Thing | Pin | Name in `main.h` | Notes |
|---|---|---|---|
| Green LED (LD4) | PD12 | `LD4_GPIO_Port`, `LD4_Pin` | HIGH = on |
| Orange LED (LD3) | PD13 | `LD3_GPIO_Port`, `LD3_Pin` | HIGH = on |
| Red LED (LD5) | PD14 | `LD5_GPIO_Port`, `LD5_Pin` | HIGH = on |
| Blue LED (LD6) | PD15 | `LD6_GPIO_Port`, `LD6_Pin` | HIGH = on |
| Blue user button (B1) | PA0 | `B1_GPIO_Port`, `B1_Pin` | pressed = HIGH (external pull-down on board) |
| Black button | NRST | — | resets the MCU, not readable |
| Accelerometer | SPI1 (PA5/6/7), CS = PE3 | `hspi1`, `CS_I2C_SPI_*` | CS is **active LOW** |
| Accelerometer interrupt | PE1 | `MEMS_INT2_*` | |
| Audio DAC CS43L22 | I2C1 (PB6/PB9), reset = PD4 | `hi2c1`, `Audio_RST_*` | I2C address `0x94` |
| Audio data | I2S3 | `hi2s3` | |
| Microphone MP45DT02 | PB10 (CLK), PC3 (data) | `CLK_IN_*`, `PDM_OUT_*` | |
| USB OTG (micro-USB) | PA11/PA12 | | USB host, CDC |

**Free pins** for your own wiring (on the header strips): most of **PE4–PE15**, **PB0, PB1, PB4, PB5, PB7, PB8, PB11–PB15**, **PC1, PC2, PC4, PC5, PC6, PC8, PC9**, **PD0–PD3, PD6–PD11**. In CubeMX, a free pin is grey; a used one is green or yellow.

---

## 3. GPIO output (LED, relay, buzzer…)

### CubeMX setup
1. **Pinout view**: left-click the pin, choose **GPIO_Output**.
2. Right-click the pin, choose **Enter User Label**, and type e.g. `MY_LED`. This becomes `MY_LED_Pin` / `MY_LED_GPIO_Port` in `main.h`.
3. **System Core → GPIO** sets:
   - **Output level**: the starting state (Low/High).
   - **Mode**: Push-Pull (normal) or Open-Drain (for shared lines; needs a pull-up).
   - **Pull**: usually *No pull-up and no pull-down* for outputs.
   - **Speed**: Low is fine for LEDs. High is only for fast signals.
4. Generate Code.

### Code
```c
HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET);    // on
HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_RESET);  // off
HAL_GPIO_TogglePin(LD4_GPIO_Port, LD4_Pin);                 // flip

// several pins on the same port at once
HAL_GPIO_WritePin(GPIOD, LD3_Pin | LD4_Pin | LD5_Pin | LD6_Pin, GPIO_PIN_SET);
```

### External LED wiring
`pin → 330 Ω resistor → LED long leg (+) → LED short leg (−) → GND`

A GPIO pin gives at most about 20 mA. Never connect an LED without a resistor, and never drive motors straight from a pin.

---

## 4. GPIO input (button, switch, sensor)

### CubeMX setup
1. Click the pin, choose **GPIO_Input**.
2. **System Core → GPIO → Pull**:
   - **Pull-up** for a button wired from pin to **GND**. It reads LOW when pressed.
   - **Pull-down** for a button wired from pin to **3V3**. It reads HIGH when pressed.
   - **No pull** if the board already has a resistor (B1 does).

> A floating input with no pull resistor reads random values.

### Code
```c
if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET)
{
  HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, GPIO_PIN_SET);
}
else
{
  HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, GPIO_PIN_RESET);
}
```

### Detect a single press (edge + debounce)
```c
/* USER CODE BEGIN PV */
static GPIO_PinState lastButton = GPIO_PIN_RESET;
static uint32_t lastChangeTick;
/* USER CODE END PV */

/* inside USER CODE BEGIN 3 */
GPIO_PinState now = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
// mechanical contacts bounce for a few ms; ignore changes faster than 30 ms
if (now != lastButton && HAL_GetTick() - lastChangeTick > 30)
{
  lastChangeTick = HAL_GetTick();
  lastButton = now;
  if (now == GPIO_PIN_SET)
  {
    HAL_GPIO_TogglePin(LD6_GPIO_Port, LD6_Pin);
  }
}
```

---

## 5. Button interrupt (EXTI)

Instead of checking the pin every loop, the hardware calls a function when it changes.

### CubeMX setup
1. Click PA0 and choose **GPIO_EXTI0**. Right now it is set to *Event* mode, which does **not** call any code.
2. **System Core → GPIO → PA0 → GPIO mode**: *External Interrupt Mode with Rising edge trigger detection*.
3. **System Core → NVIC**: tick **EXTI line0 interrupt**.
4. Generate Code. CubeMX adds `EXTI0_IRQHandler` to `stm32f4xx_it.c`.

### Code
```c
/* USER CODE BEGIN PV */
volatile uint8_t buttonPressed;
/* USER CODE END PV */

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == B1_Pin)
  {
    buttonPressed = 1;
  }
}
/* USER CODE END 4 */

/* inside USER CODE BEGIN 3 */
if (buttonPressed)
{
  buttonPressed = 0;
  HAL_GPIO_TogglePin(LD4_GPIO_Port, LD4_Pin);
}
```

Rules for interrupt callbacks:
- Keep them **short**: set a flag and leave.
- Never call `HAL_Delay()` inside one. It freezes, because SysTick can't interrupt you there.
- Variables shared with the main loop must be `volatile`.

The name `HAL_GPIO_EXTI_Callback` is fixed: the HAL calls it for **every** EXTI pin, so check `GPIO_Pin`.

---

## 6. Timing

```c
HAL_Delay(500);               // blocking wait, ms
uint32_t t = HAL_GetTick();   // ms since boot, wraps after ~49 days
```

### Non-blocking blink (preferred)
```c
/* USER CODE BEGIN PV */
static uint32_t lastBlink;
/* USER CODE END PV */

/* inside USER CODE BEGIN 3 */
// subtraction stays correct even when the tick counter wraps around
if (HAL_GetTick() - lastBlink >= 500)
{
  lastBlink = HAL_GetTick();
  HAL_GPIO_TogglePin(LD4_GPIO_Port, LD4_Pin);
}
```

Several of these blocks with different periods can run "at the same time" in one loop.

---

## 7. PWM: LED brightness

The four LEDs sit on timer **TIM4** channels, so you can dim them.

| LED | Pin | Timer channel |
|---|---|---|
| Green LD4 | PD12 | TIM4_CH1 |
| Orange LD3 | PD13 | TIM4_CH2 |
| Red LD5 | PD14 | TIM4_CH3 |
| Blue LD6 | PD15 | TIM4_CH4 |

### CubeMX setup
1. Click PD12 and choose **TIM4_CH1**. This replaces GPIO_Output, so `HAL_GPIO_WritePin` stops working on that pin.
2. **Timers → TIM4**: Clock Source = *Internal Clock*, Channel1 = *PWM Generation CH1*.
3. Parameter Settings:
   - **Prescaler** = `84 - 1`. The TIM4 clock is 84 MHz, so this makes it 1 MHz.
   - **Counter Period** = `1000 - 1`. That gives 1 kHz PWM with 1000 brightness steps.
4. Generate Code.

### Code
```c
/* USER CODE BEGIN 2 */
HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
/* USER CODE END 2 */

__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);     // off
__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 250);   // 25 %
__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 1000);  // full
```

---

## 8. SPI: the on-board accelerometer

Already configured: `hspi1`, CS on PE3.

Your board has either a **LIS302DL** (older boards) or a **LIS3DSH** (newer, MB997C). Read register `0x0F` (WHO_AM_I) to find out: `0x3B` means LIS302DL, `0x3F` means LIS3DSH.

```c
/* USER CODE BEGIN 4 */
static uint8_t Accel_Read(uint8_t reg)
{
  uint8_t tx = reg | 0x80;  // bit 7 = read
  uint8_t rx;
  HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, &tx, 1, 10);
  HAL_SPI_Receive(&hspi1, &rx, 1, 10);
  HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_SET);
  return rx;
}

static void Accel_Write(uint8_t reg, uint8_t value)
{
  uint8_t tx[2] = { reg, value };
  HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, tx, 2, 10);
  HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_SET);
}
/* USER CODE END 4 */

/* USER CODE BEGIN 2 */
// MX_GPIO_Init leaves CS low (= chip selected); deselect before the first transfer
HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_SET);
uint8_t id = Accel_Read(0x0F);
if (id == 0x3B || id == 0x3F)
{
  HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, GPIO_PIN_SET);
}
/* USER CODE END 2 */
```

You can also fix the CS start level permanently in CubeMX: **GPIO → PE3 → GPIO output level = High**.

---

## 9. I2C basics

```c
uint8_t data[2] = { reg, value };
HAL_I2C_Master_Transmit(&hi2c1, 0x94, data, 2, 10);   // write register

HAL_I2C_Mem_Read(&hi2c1, 0x94, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 10);  // read register
```

HAL uses **8-bit addresses**, which are the 7-bit datasheet address shifted left by 1. The CS43L22 is `0x4A`, so you pass `0x94`. The CS43L22 also needs `Audio_RST` (PD4) set HIGH before it answers.

---

## 10. HAL patterns you'll see everywhere

| Pattern | Meaning |
|---|---|
| `HAL_XXX_Init()` | configures a peripheral (CubeMX writes these) |
| `HAL_XXX_Start()` / `Stop()` | runs a timer, PWM, ADC… |
| `HAL_XXX_Transmit(…, timeout)` | **blocking**, waits up to `timeout` ms |
| `HAL_XXX_Transmit_IT(…)` | starts the transfer and returns at once; calls `HAL_XXX_TxCpltCallback()` when done |
| `HAL_XXX_Transmit_DMA(…)` | same, but DMA moves the data without the CPU |
| `hxxx` (`hspi1`, `hi2c1`, `htim4`) | handle: the struct describing that peripheral instance |
| returns `HAL_StatusTypeDef` | `HAL_OK`, `HAL_ERROR`, `HAL_BUSY`, `HAL_TIMEOUT` |

```c
if (HAL_SPI_Transmit(&hspi1, &tx, 1, 10) != HAL_OK)
{
  HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, GPIO_PIN_SET);  // red = error
}
```

---

## 11. Function reference (signatures and parameters)

### Where to find this yourself

| Source | How | Best for |
|---|---|---|
| **Hover** | rest the mouse on a function name | quick check of the parameter list |
| **Signature help** | type `(` after a function name, or `Ctrl+Shift+Space` | which parameter you're on right now |
| **Go to definition** | `Ctrl+Click` or `F12` | jumps to the prototype in the `.h` file |
| **The `.c` file** | `F12` again on the prototype, or open `Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_xxx.c` | the `@param` / `@retval` comment above each function, plus a "How to use this driver" section at the top of the file |
| **Headers** | `Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_hal_xxx.h` | all functions **and** the allowed values (`GPIO_MODE_*`, `TIM_CHANNEL_*`…) |
| **User manual UM1725** | search "UM1725" on st.com: *Description of STM32F4 HAL and low-layer drivers* | the official PDF reference for every HAL function |
| **Reference manual RM0090** | st.com | what the hardware registers really do |
| **Board user manual UM1472** | st.com | the DISC1 schematic: which component sits on which pin |

> CubeMX copies only the drivers you enable. Right now there is **no** `stm32f4xx_hal_tim.h` in `Drivers/`. It appears after you enable a timer in CubeMX and generate code. If you can't find a function's header, the peripheral isn't enabled yet.

Search tricks in VS Code:
- `Ctrl+T`, then type `HAL_SPI_`: lists every symbol with that prefix in the project.
- `Ctrl+Shift+F` in `Drivers/`: find which file defines something like `GPIO_SPEED_FREQ_LOW`.

### Parameter naming conventions

| Name | Means | Example |
|---|---|---|
| `GPIOx` | port | `GPIOD`, or `LD4_GPIO_Port` |
| `GPIO_Pin` | pin bit mask (can OR several) | `GPIO_PIN_12`, or `LD4_Pin` |
| `hspi`, `hi2c`, `htim` | pointer to a handle | `&hspi1`, `&hi2c1`, `&htim4` |
| `pData` | pointer to your buffer | `&value` or `buffer` |
| `Size` | number of **items** (bytes for 8-bit data) | `sizeof(buffer)` |
| `Timeout` | max wait in ms | `10`, or `HAL_MAX_DELAY` to wait forever |
| `DevAddress` | I2C address, **8-bit form** (7-bit << 1) | `0x94` |

### Cheatsheet

`HAL_StatusTypeDef` return values: `HAL_OK`, `HAL_ERROR`, `HAL_BUSY`, `HAL_TIMEOUT`. Functions marked `[cb]` are callbacks: you write them and the HAL calls them.

```c
module: system (stm32f4xx_hal.h, stm32f4xx_hal_rcc.h, stm32f4xx_hal_cortex.h, stm32f4xx_hal_pwr.h)

    // Init and time
    HAL_StatusTypeDef HAL_Init(void);                                    // Reset peripherals, start the 1 ms SysTick (first call in main)
    void     HAL_Delay(uint32_t Delay);                                  // Blocking wait, Delay in ms
    uint32_t HAL_GetTick(void);                                          // Milliseconds since boot
    void     HAL_SuspendTick(void);                                      // Pause SysTick (before sleep)
    void     HAL_ResumeTick(void);                                       // Resume SysTick
    uint32_t HAL_GetUIDw0(void);                                         // First word of the chip's unique 96-bit ID

    // Clocks
    uint32_t HAL_RCC_GetSysClockFreq(void);                              // CPU clock in Hz (168000000)
    uint32_t HAL_RCC_GetHCLKFreq(void);                                  // AHB bus clock in Hz
    uint32_t HAL_RCC_GetPCLK1Freq(void);                                 // APB1 bus clock in Hz (42 MHz; TIM2-7 run at x2)
    __HAL_RCC_GPIOx_CLK_ENABLE();                                        // Power a GPIO port (x = A..I), needed before HAL_GPIO_Init
    __HAL_RCC_SPIx_CLK_ENABLE();                                         // Power a peripheral (same pattern: I2Cx, TIMx, USARTx, ADCx...)

    // Interrupts (NVIC)
    void HAL_NVIC_SetPriority(IRQn_Type IRQn, uint32_t PreemptPriority, uint32_t SubPriority); // Lower number = more urgent
    void HAL_NVIC_EnableIRQ(IRQn_Type IRQn);                             // Allow an interrupt (EXTI0_IRQn, TIM4_IRQn, SPI1_IRQn...)
    void HAL_NVIC_DisableIRQ(IRQn_Type IRQn);                            // Block an interrupt
    void HAL_NVIC_SystemReset(void);                                     // Software reset of the whole MCU
    __disable_irq();                                                     // Block all interrupts (critical section start)
    __enable_irq();                                                      // Allow all interrupts again
    __NOP();                                                             // Do nothing for one CPU cycle

    // Power
    void HAL_PWR_EnterSLEEPMode(uint32_t Regulator, uint8_t SLEEPEntry); // Sleep until interrupt: (PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI)
    void HAL_PWR_EnterSTOPMode(uint32_t Regulator, uint8_t STOPEntry);   // Deep sleep, clocks stop; call SystemClock_Config() after wake-up

module: gpio (stm32f4xx_hal_gpio.h)

    // Pin access
    void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState); // Set pin HIGH (GPIO_PIN_SET) or LOW (GPIO_PIN_RESET)
    GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);                 // Read pin: GPIO_PIN_SET or GPIO_PIN_RESET
    void HAL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);                        // Flip pin state

    // Setup
    void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init);                   // Configure pins from a struct (Pin, Mode, Pull, Speed, Alternate)
    void HAL_GPIO_DeInit(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin);                           // Reset pins to default (analog input)
    HAL_StatusTypeDef HAL_GPIO_LockPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);             // Freeze pin config until next reset

    // Interrupts
    void HAL_GPIO_EXTI_IRQHandler(uint16_t GPIO_Pin);                                       // Called from EXTIx_IRQHandler (CubeMX writes this)
    void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);                                         // [cb] A pin interrupt fired; check GPIO_Pin

module: spi (stm32f4xx_hal_spi.h)  ->  &hspi1 = accelerometer

    // Blocking
    HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *hspi, const uint8_t *pData, uint16_t Size, uint32_t Timeout);   // Send Size bytes
    HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size, uint32_t Timeout);          // Receive Size bytes
    HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi, const uint8_t *pTxData, uint8_t *pRxData,
                                              uint16_t Size, uint32_t Timeout);                                          // Send and receive at once

    // Non-blocking (interrupt / DMA)
    HAL_StatusTypeDef HAL_SPI_Transmit_IT(SPI_HandleTypeDef *hspi, const uint8_t *pData, uint16_t Size);   // Start send, returns immediately
    HAL_StatusTypeDef HAL_SPI_Receive_IT(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size);          // Start receive, returns immediately
    HAL_StatusTypeDef HAL_SPI_Transmit_DMA(SPI_HandleTypeDef *hspi, const uint8_t *pData, uint16_t Size);  // Send via DMA (enable DMA in CubeMX)
    HAL_StatusTypeDef HAL_SPI_Receive_DMA(SPI_HandleTypeDef *hspi, uint8_t *pData, uint16_t Size);         // Receive via DMA
    void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);                                                  // [cb] Send finished
    void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);                                                  // [cb] Receive finished
    void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);                                                // [cb] Send+receive finished
    void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);                                                   // [cb] Something went wrong

    // Status
    HAL_SPI_StateTypeDef HAL_SPI_GetState(const SPI_HandleTypeDef *hspi);                                  // READY, BUSY_TX, BUSY_RX...
    uint32_t HAL_SPI_GetError(const SPI_HandleTypeDef *hspi);                                              // Error bits (HAL_SPI_ERROR_*)

module: i2c (stm32f4xx_hal_i2c.h)  ->  &hi2c1 = audio DAC   (DevAddress = 7-bit address << 1)

    // Blocking
    HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout); // Send raw bytes
    HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size, uint32_t Timeout);  // Read raw bytes
    HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress,
                                        uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);  // Write device register(s); MemAddSize = I2C_MEMADD_SIZE_8BIT
    HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress,
                                       uint16_t MemAddSize, uint8_t *pData, uint16_t Size, uint32_t Timeout);   // Read device register(s)
    HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint32_t Trials, uint32_t Timeout); // HAL_OK if a device answers (bus scan)

    // Non-blocking
    HAL_StatusTypeDef HAL_I2C_Master_Transmit_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size); // Start send
    HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint8_t *pData, uint16_t Size);  // Start receive
    HAL_StatusTypeDef HAL_I2C_Mem_Read_IT(I2C_HandleTypeDef *hi2c, uint16_t DevAddress, uint16_t MemAddress,
                                          uint16_t MemAddSize, uint8_t *pData, uint16_t Size);                  // Start register read
    void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c);                                                // [cb] Send finished
    void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c);                                                // [cb] Receive finished
    void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c);                                                   // [cb] Register read finished
    void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c);                                                       // [cb] NACK, bus error...

    // Status
    HAL_I2C_StateTypeDef HAL_I2C_GetState(I2C_HandleTypeDef *hi2c);                                            // READY, BUSY...
    uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *hi2c);                                                        // Error bits (HAL_I2C_ERROR_AF = no answer)

module: i2s (stm32f4xx_hal_i2s.h)  ->  &hi2s3 = audio out

    HAL_StatusTypeDef HAL_I2S_Transmit(I2S_HandleTypeDef *hi2s, uint16_t *pData, uint16_t Size, uint32_t Timeout); // Send audio samples, blocking
    HAL_StatusTypeDef HAL_I2S_Transmit_DMA(I2S_HandleTypeDef *hi2s, uint16_t *pData, uint16_t Size);              // Stream samples via DMA (circular = endless)
    HAL_StatusTypeDef HAL_I2S_DMAStop(I2S_HandleTypeDef *hi2s);                                                    // Stop streaming
    void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s);                                                      // [cb] First half sent, refill it
    void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s);                                                          // [cb] Second half sent, refill it

module: tim (stm32f4xx_hal_tim.h)  ->  enable a timer in CubeMX first, e.g. &htim4 = LED PWM

    // Basic timer
    HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *htim);                       // Start counting
    HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim);                    // Start counting + interrupt on every overflow
    HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef *htim);                     // Stop counting + interrupt
    void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim);                         // [cb] Timer overflowed (periodic tick)

    // PWM
    HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel);      // Start PWM output on TIM_CHANNEL_1..4
    HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t Channel);       // Stop PWM output

    // Register macros
    __HAL_TIM_SET_COMPARE(htim, Channel, Compare);                                       // PWM duty: 0..Period
    __HAL_TIM_GET_COUNTER(htim);                                                         // Current counter value
    __HAL_TIM_SET_COUNTER(htim, Counter);                                                // Overwrite counter
    __HAL_TIM_SET_AUTORELOAD(htim, Period);                                              // Change period (PWM frequency)
    __HAL_TIM_SET_PRESCALER(htim, Prescaler);                                            // Change counter speed

module: uart (stm32f4xx_hal_uart.h)  ->  enable e.g. USART2 (PA2 TX, PA3 RX) first; needs a USB-serial adapter

    HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, const uint8_t *pData, uint16_t Size, uint32_t Timeout); // Send bytes, blocking
    HAL_StatusTypeDef HAL_UART_Receive(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout);        // Receive bytes, blocking
    HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);                       // Start receive, returns immediately
    void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);                                                               // [cb] Size bytes arrived

module: adc (stm32f4xx_hal_adc.h)  ->  enable e.g. ADC1 IN1 (PA1) first

    HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *hadc);                            // Start a conversion
    HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *hadc, uint32_t Timeout); // Wait until it finishes
    uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *hadc);                                  // Result: 0..4095 (12-bit, 0..3.3 V)
    HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *hadc);                             // Stop the ADC
```

Signatures for system, GPIO, SPI, I2C and I2S are copied from this project's headers. TIM, UART and ADC are from the standard F4 HAL; their headers appear in `Drivers/` once you enable the peripheral.

### Callbacks: you write them, HAL calls them

The HAL defines these as `__weak` empty functions. When you write a function with the **exact same name and parameters** in `USER CODE BEGIN 4`, yours replaces the empty one. A typo means yours is silently ignored. Copy the name from the `.h` file to be safe.

To list them all, search `__weak` in `Drivers/STM32F4xx_HAL_Driver/Src/`.

---

## 12. Under the hood (what CubeMX writes for you)

What `MX_GPIO_Init()` actually does, so you can recognise it:

```c
__HAL_RCC_GPIOD_CLK_ENABLE();   // 1. power the port (forget this and nothing works)

GPIO_InitTypeDef cfg = {0};     // 2. describe the pin
cfg.Pin   = GPIO_PIN_12;
cfg.Mode  = GPIO_MODE_OUTPUT_PP;
cfg.Pull  = GPIO_NOPULL;
cfg.Speed = GPIO_SPEED_FREQ_LOW;
HAL_GPIO_Init(GPIOD, &cfg);     // 3. apply
```

| `Mode` value | Use |
|---|---|
| `GPIO_MODE_INPUT` | read a pin |
| `GPIO_MODE_OUTPUT_PP` | drive HIGH/LOW |
| `GPIO_MODE_OUTPUT_OD` | pull LOW or let float (I2C-style lines) |
| `GPIO_MODE_AF_PP` | pin owned by a peripheral (SPI, UART, TIM…) |
| `GPIO_MODE_ANALOG` | ADC/DAC, lowest power |
| `GPIO_MODE_IT_RISING` / `_FALLING` / `_RISING_FALLING` | input + interrupt |
| `GPIO_MODE_EVT_RISING` | input + wake-up event, **no callback** |

The fastest raw register access, which you rarely need:
```c
GPIOD->BSRR = GPIO_PIN_12;          // set
GPIOD->BSRR = GPIO_PIN_12 << 16;    // reset
GPIOD->ODR ^= GPIO_PIN_12;          // toggle (not atomic)
uint32_t in = GPIOA->IDR & GPIO_PIN_0;
```

---

## 13. Troubleshooting

| Symptom | Likely cause |
|---|---|
| Your code vanished | it was outside a `USER CODE` block when you regenerated |
| Pin does nothing | port clock not enabled, or pin assigned to another function in CubeMX |
| Input reads random values | no pull-up/pull-down |
| Interrupt never fires | NVIC line not enabled, pin left in *Event* mode, or callback name misspelled |
| Program hangs in callback | `HAL_Delay()` inside an interrupt |
| SPI device returns `0x00`/`0xFF` | CS not toggled, or wrong read bit |
| I2C returns `HAL_ERROR` | address not shifted (`0x4A` vs `0x94`), device in reset, missing pull-ups |
| `htim4` undeclared | peripheral not enabled in CubeMX, or Generate Code not run |
| No autocomplete | rebuild once so `build/Release/compile_commands.json` exists, then *clangd: Restart* |

### Debugging tips (F5 session)
- Click left of a line number to set a **breakpoint**.
- Hover a variable to see its value. Add it to **Watch** to track it.
- Use the **Peripherals / Registers** view to look at `GPIOD → ODR` and see the LED bits live.
