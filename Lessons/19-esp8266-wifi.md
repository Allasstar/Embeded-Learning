# Lesson 19: Wi-Fi Sensor Node (ESP8266)

## Goal
Connect the board to your Wi-Fi through the ESP8266 and send sensor readings to your PC, then to a free cloud dashboard (ThingSpeak).

## What you learn
- Talking to a "smart" module with **AT commands**
- An interrupt-driven UART **ring buffer** as a reusable driver
- A **command/response state machine** with timeouts and error recovery
- Protocols that don't fit line-based parsing (the `>` prompt)
- Keeping secrets (Wi-Fi password) out of git
- Basic TCP and HTTP from the device side

## How it works
The ESP-01 is a complete Wi-Fi microcontroller. Its factory firmware accepts text commands over UART:

```
you send:    AT\r\n
it answers:  AT          (echo)
             OK
```

Every command ends with `\r\n`. Answers end with `OK`, `ERROR`, or `FAIL`. Some take seconds (joining Wi-Fi: up to ~15 s), so the code must **never block** waiting for them.

## Parts and wiring
Use the **adapter board** (the small board the ESP-01 plugs into). It has a 3.3 V regulator and level shifting, so it takes 5 V.

| ESP adapter pin | Connect to |
|---|---|
| VCC | power module **5V** (the ESP draws up to ~300 mA peaks) |
| GND | GND |
| TX | PD9 (USART3_RX) |
| RX | PD8 (USART3_TX) |

## CubeMX setup
1. **Connectivity → USART3**: *Asynchronous*, pins **PD8 / PD9**. Baud `115200`, 8N1.
2. NVIC: **USART3 global interrupt**.
3. Generate.

## Steps

### Step 1: A ring buffer for the ESP's answers
The ESP answers at its own pace while the main loop is busy, so receiving is interrupt-driven: `HAL_UART_Receive_IT` takes one byte, the callback stores it in a **ring buffer** (an array with a `head` index where the interrupt writes and a `tail` index where the main code reads, both wrapping around) and asks for the next byte. Nothing is lost as long as the main loop drains it often enough. It's a struct, so the same code can serve any UART:

`Core/Inc/uart_rx.h`:

```c
#pragma once
#include "main.h"

#define UART_RX_SIZE 256

typedef struct
{
  UART_HandleTypeDef *huart;
  uint8_t byte;
  volatile uint8_t buffer[UART_RX_SIZE];
  volatile uint32_t head;
  volatile uint32_t tail;
} UartRx;

void UartRx_Start(UartRx *rx, UART_HandleTypeDef *huart);
void UartRx_OnByte(UartRx *rx);
int UartRx_Read(UartRx *rx, uint8_t *out);
```

`Core/Src/uart_rx.c`:

```c
#include "uart_rx.h"

void UartRx_Start(UartRx *rx, UART_HandleTypeDef *huart)
{
  rx->huart = huart;
  rx->head = 0;
  rx->tail = 0;
  HAL_UART_Receive_IT(huart, &rx->byte, 1);
}

void UartRx_OnByte(UartRx *rx)
{
  uint32_t next = (rx->head + 1) & (UART_RX_SIZE - 1);
  if (next != rx->tail)
  {
    rx->buffer[rx->head] = rx->byte;
    rx->head = next;
  }
  HAL_UART_Receive_IT(rx->huart, &rx->byte, 1);
}

int UartRx_Read(UartRx *rx, uint8_t *out)
{
  if (rx->tail == rx->head)
    return 0;
  *out = rx->buffer[rx->tail];
  rx->tail = (rx->tail + 1) & (UART_RX_SIZE - 1);
  return 1;
}
```

In `app.c`, forward the callback to the instance (`static UartRx espRx;`, started with `UartRx_Start(&espRx, &huart3)` in `App_Start()`):

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3) UartRx_OnByte(&espRx);
}
```

and do the same in `HAL_UART_ErrorCallback`. **Why the error callback:** if a byte arrives while the previous one hasn't been read (overrun), HAL stops receiving; without calling `HAL_UART_Receive_IT` again the driver goes deaf forever.

### Step 2: Talk to the ESP by hand
Before writing any automation, send commands one at a time and look at the real answers. Keep a table of test commands in `esp.c`; the blue button (rising edge, already detected in `app.c`) sends the next one, and `App_Update()` prints every received byte with `LOG("%c", c)`:

```c
static const char *testCommands[] =
{
  "AT\r\n",
  "AT+GMR\r\n",
  "AT+CWMODE=1\r\n",
  "AT+CWLAP\r\n",
  "AT+CWJAP=\"YourSSID\",\"YourPassword\"\r\n",
  "AT+CIFSR\r\n",
};
```

Send with `HAL_UART_Transmit(&huart3, (uint8_t *)cmd, strlen(cmd), 100)`.

Semihosting prints slowly (milliseconds per line) while the ESP sends a byte every 87 µs, and `AT+CWLAP` can answer with 1–2 KB. Set `UART_RX_SIZE` to `2048` for this step so the buffer absorbs the burst. It must stay a power of two, because the index wraps with `& (UART_RX_SIZE - 1)`.

Expected answers:

| Command | Expect |
|---|---|
| `AT` | `OK` |
| `AT+GMR` | firmware version |
| `AT+CWMODE=1` | `OK` (station mode: join a network) |
| `AT+CWLAP` | list of Wi-Fi networks nearby |
| `AT+CWJAP="YourSSID","YourPassword"` | `WIFI CONNECTED`, `WIFI GOT IP`, `OK` |
| `AT+CIFSR` | the ESP's IP address |

Do this before writing any automation. You need to know what the real answers look like.

No answer at all? Try 9600 baud: some modules ship with that.

### Step 3: Keep secrets out of git
`Core/Inc/secrets.h`:

```c
#pragma once

#define WIFI_SSID "YourSSID"
#define WIFI_PASSWORD "YourPassword"
#define SERVER_IP "192.168.1.50"
#define THINGSPEAK_KEY "XXXXXXXXXXXXXXXX"
```

Add `Core/Inc/secrets.h` to `.gitignore`, and commit a `secrets.example.h` with placeholder values instead. **Check `git status` before every commit** in this lesson.

### Step 4: Line reader
Responses arrive line by line. Collect them into lines:

```c
static char line[128];
static uint32_t lineLength;

static int ReadLine(void)
{
  uint8_t c;
  while (UartRx_Read(&espRx, &c))
  {
    if (c == '>' && lineLength == 0)
    {
      strcpy(line, ">");
      return 1;
    }
    if (c == '\n')
    {
      line[lineLength] = '\0';
      lineLength = 0;
      return 1;
    }
    if (c != '\r' && lineLength < sizeof(line) - 1)
      line[lineLength++] = c;
  }
  return 0;
}
```

**The `>` special case**: after `AT+CIPSEND=<n>`, the ESP answers with `>` and **no newline**, then waits for your data. A pure line-based reader would wait forever. Real protocols always have an exception like this.

### Step 5: Command state machine
Every step: send a command, then wait for an expected answer, an error, or a timeout.

```c
typedef struct
{
  const char *expect;
  uint32_t timeoutMs;
  uint32_t sentAt;
  uint8_t busy;
} Pending;

static Pending pending;

static void Send(const char *command, const char *expect, uint32_t timeoutMs)
{
  HAL_UART_Transmit(&huart3, (uint8_t *)command, strlen(command), 100);
  pending.expect = expect;
  pending.timeoutMs = timeoutMs;
  pending.sentAt = HAL_GetTick();
  pending.busy = 1;
}

typedef enum { RESULT_WAITING, RESULT_OK, RESULT_FAIL } Result;

static Result Poll(void)
{
  while (ReadLine())
  {
    if (strstr(line, pending.expect)) { pending.busy = 0; return RESULT_OK; }
    if (strstr(line, "ERROR") || strstr(line, "FAIL")) { pending.busy = 0; return RESULT_FAIL; }
  }
  if (HAL_GetTick() - pending.sentAt > pending.timeoutMs)
  {
    pending.busy = 0;
    return RESULT_FAIL;
  }
  return RESULT_WAITING;
}
```

Then the node's states:

```c
typedef enum
{
  NODE_RESET,
  NODE_MODE,
  NODE_JOIN,
  NODE_IDLE,
  NODE_CONNECT,
  NODE_SEND_LENGTH,
  NODE_SEND_DATA,
  NODE_CLOSE,
  NODE_ERROR_WAIT,
} NodeState;
```

| State | Send | Wait for | Timeout | On OK → | On fail → |
|---|---|---|---|---|---|
| RESET | `AT+RST` | `ready` | 5 s | MODE | ERROR_WAIT |
| MODE | `AT+CWMODE=1` | `OK` | 1 s | JOIN | ERROR_WAIT |
| JOIN | `AT+CWJAP="ssid","pass"` | `OK` | 20 s | IDLE | ERROR_WAIT |
| IDLE | (nothing; every 20 s →) | | | CONNECT | |
| CONNECT | `AT+CIPSTART="TCP","host",port` | `OK` | 5 s | SEND_LENGTH | IDLE |
| SEND_LENGTH | `AT+CIPSEND=<n>` | `>` | 2 s | SEND_DATA | CLOSE |
| SEND_DATA | the payload | `SEND OK` | 5 s | CLOSE | CLOSE |
| CLOSE | `AT+CIPCLOSE` | `OK` (or `CLOSED`) | 2 s | IDLE | IDLE |
| ERROR_WAIT | (wait 10 s) | | | RESET | |

Implement `Esp_Update()` as a `switch (state)`: in each state, if not `pending.busy`, `Send(...)`; otherwise `Poll()` and move on depending on the result. Log every state change to the console. You'll need it.

**ERROR_WAIT → RESET** is the most important row: when *anything* goes wrong (router reboots, ESP crashes), the node recovers by itself. A device that needs a human to press reset is a broken device.

### Step 6: Send to your PC
On the PC, save as `server.py` and run `python server.py` (allow it through the Windows firewall):

```python
import socket

server = socket.socket()
server.bind(("", 5000))
server.listen()
print("listening on 5000")
while True:
    client, address = server.accept()
    print(address[0], client.recv(1024).decode().strip())
    client.close()
```

Find the PC's IP with `ipconfig`, put it in `SERVER_IP`, and send a payload like `uptime=123,presses=4\n` to port 5000 every 20 s (`presses` counts blue-button presses; any two numbers will do). Build the payload with `snprintf`, and use its length in `AT+CIPSEND`.

### Step 7: Send to ThingSpeak
1. Create a free account at thingspeak.com, create a channel with fields 1–3, copy the **Write API key** into `secrets.h`.
2. Connect to `api.thingspeak.com`, port 80, and send an HTTP request:

```c
snprintf(payload, sizeof(payload),
         "GET /update?api_key=%s&field1=%ld&field2=%ld HTTP/1.1\r\n"
         "Host: api.thingspeak.com\r\n"
         "Connection: close\r\n\r\n",
         THINGSPEAK_KEY, uptimeSeconds, pressCount);
```

3. Watch the chart on the website update. The free tier accepts one update per **15 s**.

HTTP is just text over TCP. You now have the whole picture down to the bytes.

## Check it works
- [ ] All manual AT commands answered (the replies show in the debug console)
- [ ] Node joins Wi-Fi by itself after power-up
- [ ] PC server prints a reading every 20 s
- [ ] ThingSpeak chart updates
- [ ] Turn your router off and on: the node reconnects without help
- [ ] `secrets.h` is not in `git status`

## Troubleshooting
| Symptom | Cause |
|---|---|
| Garbage, then `ready` after reset | normal: the ESP prints its boot log at 74880 baud |
| No response at all | TX/RX swapped, wrong baud (try 9600), not enough power |
| Random resets, `WDT reset` | power: the ESP's current peaks need the power module, not the board |
| `busy p...` | you sent a command before the last one finished |
| `CIPSEND` hangs | waiting for a newline after `>`: special-case it |
| `ALREADY CONNECTED` | previous connection not closed: treat it as OK |
| ThingSpeak returns `0` | updates faster than every 15 s, or wrong key |

## Challenges
1. Send real sensor readings instead: for example the soil moisture and waterings from lesson 18, or the temperature from lesson 15.
2. **Receive commands**: make the ESP a TCP server (`AT+CIPMUX=1`, `AT+CIPSERVER=1,80`), parse `+IPD,...` lines, and serve a tiny HTML page with the current values.
3. Use the PC side to **send** commands back to the board over the same TCP connection.
4. Replace polling `HAL_UART_Transmit` with DMA so big payloads don't block.
