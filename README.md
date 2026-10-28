# Test_DISC1

A learning project for the **STM32F4DISCOVERY** board (STM32F407VG, ARM Cortex-M4F, 168 MHz).
The code was generated with STM32CubeMX. It is built with CMake and debugged in VS Code over the ST-Link on the board.

## Hardware configuration

| Peripheral | Purpose |
|---|---|
| GPIO PD12–15 | User LEDs: green, orange, red, blue |
| GPIO PA0 | Blue user button |
| SPI1 + PE3 (CS) | LIS302DL accelerometer (PE1 = interrupt) |
| I2C1 (PB6/PB9) | Control bus for the CS43L22 audio DAC (PD4 = reset) |
| I2S3 | Audio data to the CS43L22: master TX, 16-bit, 96 kHz |
| USB OTG FS | USB Host, CDC class |

Clock: 8 MHz HSE crystal → PLL → 168 MHz SYSCLK. APB1 runs at 42 MHz, APB2 at 84 MHz, and the USB clock is 48 MHz.

## Project structure

```
Test_DISC1/
├── Test_DISC1.ioc            ← CubeMX config (pins, clocks, peripherals)
├── Core/                     ← application code
│   ├── Inc/main.h, stm32f4xx_hal_conf.h, stm32f4xx_it.h
│   └── Src/main.c, stm32f4xx_it.c, stm32f4xx_hal_msp.c, syscalls.c, sysmem.c, system_stm32f4xx.c
├── Drivers/                  ← ST libraries (do not edit)
│   ├── CMSIS/                ← ARM core + register definitions
│   └── STM32F4xx_HAL_Driver/ ← HAL: HAL_GPIO_WritePin(), HAL_SPI_Transmit()...
├── Middlewares/ST/STM32_USB_Host_Library/  ← USB host stack
├── USB_HOST/                 ← glue between the USB stack and the board
├── startup_stm32f407xx.s     ← first code that runs after reset
├── STM32F407xx_FLASH.ld      ← linker script: memory map
├── CMakeLists.txt, CMakePresets.json, cmake/  ← build system
└── .vscode/, .settings/, .clangd, .mxproject  ← editor/tool config
```

## Boot sequence

1. **`startup_stm32f407xx.s`** runs first. It holds the **vector table**, the addresses the CPU jumps to on reset and on each interrupt. On reset it copies `.data` from flash into RAM, zeroes `.bss`, then calls `SystemInit()` and `main()`.
2. **`system_stm32f4xx.c`** (`SystemInit`) does the minimal CPU setup: it enables the FPU and sets the vector table location.
3. **`main.c`** calls `HAL_Init()`, then `SystemClock_Config()` (168 MHz), then the `MX_*_Init()` function for each peripheral, and then enters the endless `while (1)` loop. Firmware never returns from `main`.

## Key files in `Core/`

- **`main.c`**: the application. Write only inside the `/* USER CODE BEGIN ... */` / `/* USER CODE END ... */` blocks. CubeMX overwrites everything else when it regenerates.
- **`stm32f4xx_it.c`**: interrupt handlers. `SysTick_Handler` fires every 1 ms and drives `HAL_Delay`. `OTG_FS_IRQHandler` handles the USB interrupt.
- **`stm32f4xx_hal_msp.c`**: the low-level hardware setup for each peripheral (MSP = MCU Support Package), meaning its pins and its clock. `MX_SPI1_Init()` sets the SPI parameters, and the MSP file connects SPI1 to pins PA5/6/7.
- **`syscalls.c` / `sysmem.c`**: stubs that let the C standard library run without an OS. Redirect `printf` in `_write`. `malloc` gets its memory from `_sbrk`.
- **`stm32f4xx_hal_conf.h`**: selects which HAL modules are compiled in and sets `HSE_VALUE`.

## Memory map (linker script)

`STM32F407xx_FLASH.ld` places everything in memory:

| Region | Address | Size | Contents |
|---|---|---|---|
| FLASH | `0x08000000` | 1 MB | code, constants, initial values of `.data` |
| RAM | `0x20000000` | 128 KB | `.data`, `.bss`, heap, stack |
| CCMRAM | `0x10000000` | 64 KB | core-coupled RAM (CPU-only, no DMA) |

## Abstraction layers

| Layer | Example | Location |
|---|---|---|
| Registers | `GPIOD->ODR \|= (1 << 12);` | CMSIS `stm32f407xx.h` |
| HAL | `HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);` | `Drivers/STM32F4xx_HAL_Driver` |
| Middleware | USB host, file systems, RTOS | `Middlewares/` |

## Workflow

1. Change pins and peripherals in `Test_DISC1.ioc` with CubeMX, then regenerate the code.
2. Write application logic inside the `USER CODE` blocks.
3. Build with CMake (`Debug` / `Release` presets). The output is `build/<preset>/Test_DISC1.elf`.
4. Flash and debug through the on-board ST-Link (F5 in VS Code).

> `.clangd` points to `build/Release` for `compile_commands.json`. Configure the Release preset, or change the path to `build/Debug`, so the code indexing works.
