# Lesson 10: Bluetooth Console (UART)

## Goal
Talk to the board wirelessly: print status messages to a terminal on your phone or PC, and type commands like `led on`, `servo 45`, `status`.

## What you learn
- UART: baud rate, TX/RX crossover, 8N1 framing
- Receiving with interrupts, one byte at a time
- A **ring buffer** shared safely between an interrupt and the main loop
- Building lines and parsing commands
- The hidden cost of blocking transmit

## How UART works
Two wires, one per direction, no clock. Both sides agree on a speed (**baud rate**, bits per second). Each byte is sent as: 1 start bit (LOW), 8 data bits, 1 stop bit (HIGH): "8N1". At 9600 baud one byte takes ~1 ms.

**TX of one side goes to RX of the other.** Connecting TX→TX is the #1 UART mistake.

The Bluetooth module is a "wireless serial cable": bytes you send to its RX pin come out on the paired phone/PC, and vice versa.

## Parts and wiring

| Bluetooth pin | Board pin |
|---|---|
| VCC | 5V (module has its own 3.3 V regulator) |
| GND | GND |
| TXD | PA3 (USART2_RX) |
| RXD | PA2 (USART2_TX) |
| STATE / EN | leave unconnected |

The module's logic pins are 3.3 V, so they connect directly.

### Which module do you have?
Read the chip / label:
- **HC-05 / HC-06 / JDY-31**: *Classic* Bluetooth (SPP). Works with **Android** and **Windows PCs**. **Does not work with iPhones**: iOS doesn't support classic serial Bluetooth.
- **HM-10 / JDY-23 / AT-09**: Bluetooth Low Energy. Works with iPhone and Android using a BLE terminal app (e.g. *LightBlue*, *BLE Terminal*).

Default baud rate is usually **9600**. Default PIN **1234** or **0000**.

### Connecting from Windows
1. Settings → Bluetooth → Add device → pick "HC-05" (or similar), PIN 1234.
2. Device Manager → Ports: two new "Standard Serial over Bluetooth link (COMx)" ports. The **outgoing** one is usually the lower number.
3. Open it with PuTTY (Serial, COMx, 9600) or the VS Code *Serial Monitor* extension.

### Connecting from Android
Install *Serial Bluetooth Terminal*, pair the module in system settings, then connect inside the app.

## CubeMX setup
1. **Connectivity → USART2**: Mode *Asynchronous*. Pins become PA2/PA3.
2. Parameters: Baud Rate `9600`, Word Length 8, Parity None, Stop Bits 1.
3. **NVIC Settings**: tick **USART2 global interrupt**.
4. Generate.

## Steps

### Step 1: Send text

`Core/Inc/console.h`:

```c
#pragma once

void Console_Init(void);
void Console_Update(void);
void Console_Printf(const char *format, ...);
void Console_OnRxByte(void);
void Console_OnError(void);
```

`Core/Src/console.c` (first part):

```c
#include "console.h"
#include "main.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart2;

void Console_Printf(const char *format, ...)
{
  char buffer[128];
  va_list args;
  va_start(args, format);
  int length = vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  if (length > (int)sizeof(buffer) - 1)
    length = sizeof(buffer) - 1;
  if (length > 0)
    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, length, 100);
}
```

In `App_Start()`: `Console_Printf("Hello from STM32\r\n");`. Connect from the phone/PC, press the board's reset button, and see the greeting.

Terminals expect `\r\n` (carriage return + line feed) at line ends.

### Step 2: Receive with an interrupt into a ring buffer
`HAL_UART_Receive_IT(&huart2, &rxByte, 1)` says "interrupt me when 1 byte has arrived". In the callback we store the byte and immediately ask for the next one.

The main loop can't process each byte inside the interrupt, and it may be busy when bytes arrive. So bytes go into a **ring buffer**: an array with a write index (`head`, moved only by the interrupt) and a read index (`tail`, moved only by the main loop). When an index reaches the end it wraps to 0.

```
 [ . . h e l l o . . ]
       ^tail     ^head      head == tail → empty
```

Because each index has exactly **one writer**, no locking is needed. This is the most common data structure in embedded code. Learn it by heart.

```c
#define RX_SIZE 128
#define RX_MASK (RX_SIZE - 1)

static uint8_t rxByte;
static volatile uint8_t rxBuffer[RX_SIZE];
static volatile uint32_t rxHead;
static volatile uint32_t rxTail;

void Console_Init(void)
{
  HAL_UART_Receive_IT(&huart2, &rxByte, 1);
}

void Console_OnRxByte(void)
{
  uint32_t next = (rxHead + 1) & RX_MASK;
  if (next != rxTail)
  {
    rxBuffer[rxHead] = rxByte;
    rxHead = next;
  }
  HAL_UART_Receive_IT(&huart2, &rxByte, 1);
}

void Console_OnError(void)
{
  HAL_UART_Receive_IT(&huart2, &rxByte, 1);
}
```

`RX_SIZE` is a power of two so `& RX_MASK` can replace the slower `% RX_SIZE`.

If the buffer is full (`next == rxTail`), the byte is dropped: better than overwriting unread data.

**Why the error callback?** If a byte arrives while the previous one hasn't been read yet (overrun), HAL stops receiving and calls the error callback. Without re-arming, the console silently goes deaf forever.

In `app.c`:

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) Console_OnRxByte();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) Console_OnError();
}
```

### Step 3: Collect lines

```c
static char line[64];
static uint32_t lineLength;

static void HandleLine(char *text);

void Console_Update(void)
{
  while (rxTail != rxHead)
  {
    char c = rxBuffer[rxTail];
    rxTail = (rxTail + 1) & RX_MASK;

    if (c == '\r' || c == '\n')
    {
      if (lineLength > 0)
      {
        line[lineLength] = '\0';
        HandleLine(line);
        lineLength = 0;
      }
    }
    else if (lineLength < sizeof(line) - 1)
    {
      line[lineLength++] = c;
    }
  }
}
```

Some terminals send `\r`, some `\n`, some both. This handles all three because empty lines are ignored.

### Step 4: Commands

```c
static void HandleLine(char *text)
{
  char *command = strtok(text, " ");
  char *argument = strtok(NULL, " ");

  if (strcmp(command, "help") == 0)
  {
    Console_Printf("help, status, led on|off, servo <0-180>\r\n");
  }
  else if (strcmp(command, "status") == 0)
  {
    Console_Printf("uptime %lu ms\r\n", HAL_GetTick());
  }
  else if (strcmp(command, "led") == 0 && argument)
  {
    uint32_t level = strcmp(argument, "on") == 0 ? 1000 : 0;
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, level);
    Console_Printf("ok\r\n");
  }
  else if (strcmp(command, "servo") == 0 && argument)
  {
    Servo_MoveTo(atoi(argument));
    Console_Printf("ok\r\n");
  }
  else
  {
    Console_Printf("unknown: %s\r\n", command);
  }
}
```

Add `extern TIM_HandleTypeDef htim4;` (lesson 04's LED timer) and `#include "servo.h"` at the top. Add more commands: `dist` (lesson 07), `pot` (lesson 08), `play` (lesson 05).

### Step 5: Notice the transmit cost
At 9600 baud, `Console_Printf` of 50 characters **blocks for ~52 ms**. Measure it:

```c
uint32_t t = HAL_GetTick();
Console_Printf("0123456789012345678901234567890123456789012345678\r\n");
LOG("took %lu ms\n", HAL_GetTick() - t);
```

During that time the LED fade, melody and servo freeze. Challenge 1 fixes it.

## Check it works
- [ ] Greeting appears after reset
- [ ] `help`, `status`, `led on`, `servo 30` all work
- [ ] Pasting a long line doesn't crash anything (it's truncated)
- [ ] Console still works after you spam keys quickly

## Troubleshooting
| Symptom | Cause |
|---|---|
| Nothing received on phone | TX/RX swapped, wrong baud, or not connected in the app (module LED blinks fast = not connected, slow/solid = connected) |
| Garbage characters | baud rate mismatch |
| Board receives once then never again | `HAL_UART_Receive_IT` not re-armed in the callback, or error callback missing |
| Can't find module on iPhone | it's classic Bluetooth: use Android or a PC |

## Challenges
1. **Non-blocking transmit**: add a TX ring buffer. `Console_Printf` only copies into it. When the UART is idle, start `HAL_UART_Transmit_IT` with the bytes available; in `HAL_UART_TxCpltCallback` send the next chunk.
2. Echo typed characters back and support **Backspace** (`0x08` / `0x7F`).
3. Stream sensor values as CSV (`t,pot,dist\r\n`) every 100 ms and plot them on the PC (e.g. *SerialPlot*, or save and open in Excel).
4. Change the module's name/baud with **AT commands** (HC-05: hold its button while powering up to enter AT mode at 38400 baud).
