# Lesson 20: FreeRTOS

## Goal
Rebuild your lesson 18 (or 19) project on top of FreeRTOS: separate tasks for sensors, control, display and console, talking through queues and protected by a mutex.

## What you learn
- What an RTOS does: tasks, scheduler, priorities
- Blocking without wasting CPU: `osDelay`, waiting on queues
- Passing data between tasks and from interrupts with **queues**
- Protecting shared resources (the I2C bus) with a **mutex**
- Stack sizes, stack overflow detection
- Interrupt priorities and the RTOS
- When an RTOS helps, and when a superloop is enough

## Superloop vs RTOS
Until now, `App_Update()` called every module's `Update()` in a loop. Each one had to return quickly, so everything became a state machine with `now - start >= duration` checks. That's a **superloop** (cooperative multitasking). It works well, and many products ship exactly like that.

Its weak spot: one slow function (a 25 ms OLED update, a blocking UART print) delays everyone else.

An **RTOS** gives each job its own **task** with its own stack. Each task can be written as a simple endless loop that *blocks* ("wait 100 ms", "wait until a byte arrives"). The **scheduler** switches the CPU to whichever ready task has the highest priority, even in the middle of another task's work (preemption).

## CubeMX setup
Make a git commit before this lesson. CubeMX changes a lot here.

1. **Middleware → FREERTOS**: Interface **CMSIS_V2**.
2. **Config parameters**:
   - `USE_NEWLIB_REENTRANT`: *Enabled* (you use `snprintf` from several tasks; CubeMX warns about this)
   - `CHECK_FOR_STACK_OVERFLOW`: *Option2*
   - `TOTAL_HEAP_SIZE`: `32768` (bytes for all task stacks and queues)
3. **System Core → SYS → Timebase Source**: **TIM6**. FreeRTOS needs SysTick for itself, so HAL's 1 ms tick moves to TIM6.
4. **NVIC**: CubeMX switches priority grouping to 4 bits. For every interrupt whose callback will call RTOS functions (USART2, USART3, EXTI...), tick **Uses FreeRTOS functions**. CubeMX then forces their priority to ≥ 5. Interrupts with priority 0–4 must **never** call RTOS functions.
5. Generate. You get `Core/Src/freertos.c` and a `defaultTask`. `main()` now ends with `osKernelStart()`, which **never returns**: the `while (1)` loop and `App_Update()` no longer run.

The USB host code moves into `defaultTask`. Leave it there.

## Steps

### Step 1: First task
In `freertos.c`, inside `/* USER CODE BEGIN RTOS_THREADS */`, call your own function:

```c
App_CreateTasks();
```

`app.c`:

```c
#include "cmsis_os.h"

static void BlinkTask(void *argument)
{
  for (;;)
  {
    HAL_GPIO_TogglePin(LD6_GPIO_Port, LD6_Pin);
    osDelay(500);
  }
}

void App_CreateTasks(void)
{
  static const osThreadAttr_t blinkAttr =
  {
    .name = "blink",
    .stack_size = 256 * 4,
    .priority = osPriorityLow,
  };
  osThreadNew(BlinkTask, NULL, &blinkAttr);
}
```

(Use any LED still configured as GPIO, or a traffic light pin.)

Compare with lesson 01: `osDelay(500)` blocks *this task only*. While it waits, the CPU runs other tasks, or sleeps if there's nothing to do. No `HAL_GetTick()` arithmetic.

**Never call `HAL_Delay` in a task.** It busy-waits and hogs the CPU.

### Step 2: Plan the tasks

| Task | Priority | Loop |
|---|---|---|
| `control` | AboveNormal | wait for sensor data from a queue → run the plant state machine → pump |
| `sensors` | Normal | every 1 s: read soil, RTC, (BMP280...) → put a `SensorData` struct in the queue |
| `display` | BelowNormal | every 200 ms: draw OLED from the latest data |
| `console` | Normal | wait for a received byte from a queue → build lines → run commands |
| `defaultTask` | Normal | USB host (generated) |

Higher priority = more urgent. The pump logic must not be delayed by a slow screen redraw, so `control` is above `display`.

### Step 3: Queue from sensors to control

```c
typedef struct
{
  uint32_t soilPercent;
  uint8_t hours, minutes, seconds;
} SensorData;

static osMessageQueueId_t sensorQueue;

static void SensorTask(void *argument)
{
  uint32_t next = osKernelGetTickCount();
  for (;;)
  {
    SensorData data;
    data.soilPercent = Soil_Percent();
    Rtc_GetTime(&data.hours, &data.minutes, &data.seconds);
    osMessageQueuePut(sensorQueue, &data, 0, 0);

    next += 1000;
    osDelayUntil(next);
  }
}

static void ControlTask(void *argument)
{
  for (;;)
  {
    SensorData data;
    if (osMessageQueueGet(sensorQueue, &data, NULL, osWaitForever) == osOK)
      Plant_Step(&data);
  }
}
```

Create the queue in `App_CreateTasks()` **before** the tasks: `sensorQueue = osMessageQueueNew(4, sizeof(SensorData), NULL);`

- The queue **copies** the struct, so there's no shared variable and no `volatile`.
- `osWaitForever`: the control task uses **zero CPU** until data arrives.
- `osDelayUntil(next)` keeps an exact 1 s period; `osDelay(1000)` would drift by the time the loop body takes.

Your lesson 18 state machine still works, but now it's driven by events (new data) instead of polled.

### Step 4: Interrupt → task: the console
Replace the ring buffer with a queue. In the UART callback (it runs in the interrupt):

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    osMessageQueuePut(consoleQueue, &consoleByte, 0, 0);
    HAL_UART_Receive_IT(&huart2, &consoleByte, 1);
  }
}
```

From an interrupt, the timeout **must be 0**: an interrupt can't wait.

The console task:

```c
static void ConsoleTask(void *argument)
{
  for (;;)
  {
    uint8_t c;
    osMessageQueueGet(consoleQueue, &c, NULL, osWaitForever);
    Console_HandleChar(c);
  }
}
```

The task sleeps until a key is pressed, then wakes up within microseconds. The queue *is* the ring buffer, and the RTOS handles the locking.

### Step 5: Mutex for the I2C bus
The OLED (display task) and BMP280 (sensor task) share **I2C1**. If the scheduler switches tasks in the middle of an OLED transfer and the sensor task starts its own transfer, both break. Protect the bus:

```c
static osMutexId_t i2c1Mutex;

void I2c1_Lock(void)   { osMutexAcquire(i2c1Mutex, osWaitForever); }
void I2c1_Unlock(void) { osMutexRelease(i2c1Mutex); }
```

Wrap every group of I2C1 calls that belong together:

```c
I2c1_Lock();
Oled_Update();
I2c1_Unlock();
```

Keep the locked section short. The display task holding the lock for 25 ms means the sensor task waits up to 25 ms. A mutex (unlike a plain semaphore) supports **priority inheritance**: if a high-priority task waits for a lock held by a low-priority task, the low one is temporarily boosted so it finishes quickly.

Do the same for the console's `Console_Printf` (several tasks print).

### Step 6: Check the stacks
Each task has a fixed stack. Too small → it overflows into other memory and causes strange crashes much later.

1. Add the hook in `freertos.c` (CubeMX creates the stub with Option2):

   ```c
   void vApplicationStackOverflowHook(TaskHandle_t xTask, signed char *pcTaskName)
   {
     __disable_irq();
     for (;;) {}
   }
   ```

   Put a breakpoint inside: if you ever land there, `pcTaskName` tells you which task.
2. Add a console command `tasks` that prints, for each task, `uxTaskGetStackHighWaterMark(handle)`: the smallest amount of stack that has ever been free, in words. Keep at least ~50 words of headroom. `snprintf` alone uses a few hundred bytes.

### Step 7: Compare
- Measure how quickly the pump reacts while the OLED is redrawing, in the superloop version vs the RTOS version.
- Look at the code size (build output) and RAM usage of both.
- Which version is easier to read? Which is easier to debug?

There is no universal answer. Small, simple devices are often better as a superloop; devices with networking, displays and several independent jobs benefit from an RTOS.

## Check it works
- [ ] All tasks run; the LED blinks steadily even during OLED updates
- [ ] Plant logic works as in lesson 18
- [ ] Console responds instantly
- [ ] No I2C errors after hours of running
- [ ] `tasks` shows reasonable stack headroom for every task

## Troubleshooting
| Symptom | Cause |
|---|---|
| Hard fault at start | queue/mutex used before it was created, or heap too small (`osThreadNew` returned NULL) |
| Hangs in `vPortValidateInterruptPriority` / assert | an interrupt with priority < 5 called an RTOS function |
| `HAL_GetTick` stuck | Timebase still on SysTick: use TIM6 |
| Task never runs | a higher-priority task never blocks (busy loop without `osDelay`) |
| Random corruption in `printf` output | two tasks printing at once without a mutex |
| Stack overflow hook hit | increase that task's `stack_size` |

## Challenges
1. Make the OLED update use **DMA + a semaphore**: start `HAL_I2C_Mem_Write_DMA`, then wait on a semaphore that the transfer-complete callback releases. The display task sleeps during the transfer instead of busy-waiting.
2. Use a **software timer** (`osTimerNew`) for the pump's maximum run time: a safety net independent of the control task.
3. Add a **watchdog** (IWDG): a supervisor task refreshes it only if every other task reported in recently. If any task hangs, the board resets itself.
4. Use FreeRTOS's run-time stats to show CPU usage per task.

---

## Where next?
You've now used every major peripheral type, written drivers from datasheets, and built a complete device. Good next steps:
- **Low power**: STOP mode, waking from the RTC or a button, measuring µA.
- **Bare-metal registers**: rewrite one driver (e.g. GPIO or UART) without HAL, using RM0090.
- **A custom PCB**: design a board for your plant system in KiCad.
- **Unit tests** on the PC for the pure-logic modules (PID, state machines, parsers).
