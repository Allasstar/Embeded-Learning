# Lesson 20: FreeRTOS

## Goal
Add FreeRTOS to the clean base project and build a small device on it using only the board's own parts: separate tasks for a temperature sensor (the chip's internal one), control, status output and a button handler, talking through queues and protected by a mutex.

## What you learn
- What an RTOS does: tasks, scheduler, priorities
- Blocking without wasting CPU: `osDelay`, waiting on queues
- Passing data between tasks and from interrupts with **queues**
- Protecting shared resources (the debug output) with a **mutex**
- Stack sizes, stack overflow detection
- Interrupt priorities and the RTOS
- When an RTOS helps, and when a superloop is enough

## Superloop vs RTOS
Until now, `App_Update()` called every module's `Update()` in a loop. Each one had to return quickly, so everything became a state machine with `now - start >= duration` checks. That's a **superloop** (cooperative multitasking). It works well, and many products ship exactly like that.

Its weak spot: one slow function (a 25 ms OLED update, a blocking UART print) delays everyone else.

An **RTOS** gives each job its own **task** with its own stack. Each task can be written as a simple endless loop that *blocks* ("wait 100 ms", "wait until a byte arrives"). The **scheduler** switches the CPU to whichever ready task has the highest priority, even in the middle of another task's work (preemption).

## CubeMX setup
CubeMX changes a lot here. Your lesson branch starts from a clean `master`, so `git diff` after generating shows exactly what the RTOS added.

1. **Middleware → FREERTOS**: Interface **CMSIS_V2**.
2. **Config parameters**:
   - `USE_NEWLIB_REENTRANT`: *Enabled* (you use `printf` from several tasks; CubeMX warns about this)
   - `CHECK_FOR_STACK_OVERFLOW`: *Option2*
   - `TOTAL_HEAP_SIZE`: `32768` (bytes for all task stacks and queues)
3. **System Core → SYS → Timebase Source**: **TIM6**. FreeRTOS needs SysTick for itself, so HAL's 1 ms tick moves to TIM6.
4. **NVIC**: CubeMX switches priority grouping to 4 bits. For every interrupt whose callback will call RTOS functions (here EXTI line0), tick **Uses FreeRTOS functions**. CubeMX then forces their priority to ≥ 5. Interrupts with priority 0–4 must **never** call RTOS functions.
5. **PA0** (the blue button, `B1`) → `GPIO_EXTI0`, *Rising edge*, no pull. NVIC: **EXTI line0 interrupt**, *Uses FreeRTOS functions*. *Why:* the button press becomes an interrupt that hands an event to a task (Step 4).
6. **Analog → ADC1**: tick **Temperature Sensor Channel**, Resolution *12 bits*, Scan and Continuous *Disabled*, Rank 1 → Temperature Sensor Channel, Sampling Time *480 Cycles*. *Why:* the internal sensor needs at least 10 µs of sampling time to read correctly; 480 cycles at the ADC clock is comfortably above that.
7. Generate. You get `Core/Src/freertos.c` and a `defaultTask`. `main()` now ends with `osKernelStart()`, which **never returns**: the `while (1)` loop and `App_Update()` no longer run.

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

(`LD6`, the blue on-board LED, is still a plain GPIO on the clean base project.) The old superloop in `app.c` never runs again once `osKernelStart()` is called, so delete its LED blinker and button code.

Compare with the `tick % 1000 < 500` blinker the superloop used: `osDelay(500)` blocks *this task only*. While it waits, the CPU runs other tasks, or sleeps if there's nothing to do. No `HAL_GetTick()` arithmetic.

**Never call `HAL_Delay` in a task.** It busy-waits and hogs the CPU.

### Step 2: Plan the tasks

| Task | Priority | Loop |
|---|---|---|
| `control` | AboveNormal | wait for sensor data from a queue → if armed, switch the warning LED (LD5) on when the chip is too warm |
| `sensors` | Normal | every 1 s: read the chip temperature → put a `SensorData` struct in the queue |
| `status` | BelowNormal | every 1 s: print the latest data and (Step 6) the stack headroom |
| `button` | Normal | wait for a press event from the interrupt → arm / disarm → LD4 shows the state |
| `defaultTask` | Normal | USB host (generated) |

Higher priority = more urgent. The warning logic must not be delayed by slow printing, so `control` is above `status`.

### Step 3: Queue from sensors to control

```c
typedef struct
{
  int32_t temperatureTenths;
  uint32_t uptimeSeconds;
} SensorData;

static osMessageQueueId_t sensorQueue;

static int32_t ReadChipTemperatureTenths(void)
{
  HAL_ADC_Start(&hadc1);
  HAL_ADC_PollForConversion(&hadc1, 10);
  uint32_t raw = HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);

  int32_t millivolts = raw * 3300 / 4095;
  return (760 - millivolts) * 10 * 10 / 25 + 250;
}

static void SensorTask(void *argument)
{
  uint32_t next = osKernelGetTickCount();
  for (;;)
  {
    SensorData data;
    data.temperatureTenths = ReadChipTemperatureTenths();
    data.uptimeSeconds = next / 1000;
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
      Control_Step(&data);
  }
}
```

The formula is from the STM32F407 datasheet: `T = (V25 - Vsense) / slope + 25` with V25 = 0.76 V and slope = 2.5 mV/°C, here in tenths of °C. It is only accurate to a few degrees, which is fine for a warning: hold your finger on the chip and watch the value rise.

`Control_Step()` stores the latest data in a file-scope struct (the `status` task prints it; Step 5 explains why that copy needs protecting) and, if `armed` (set by the button task), switches `LD5` on when `temperatureTenths` is above `WARN_TENTHS` (pick a value just above the idle reading you see in the log).

Create the queues in `App_CreateTasks()` **before** the tasks: `sensorQueue = osMessageQueueNew(4, sizeof(SensorData), NULL);` and, for Step 4, `buttonQueue = osMessageQueueNew(8, sizeof(uint8_t), NULL);`. A task or interrupt that uses a queue before it exists crashes.

- The queue **copies** the struct, so there's no shared variable and no `volatile`.
- `osWaitForever`: the control task uses **zero CPU** until data arrives.
- `osDelayUntil(next)` keeps an exact 1 s period; `osDelay(1000)` would drift by the time the loop body takes.

### Step 4: Interrupt → task: the button
The EXTI interrupt only forwards an event; the work happens in a task. In the callback (it runs in the interrupt):

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == B1_Pin)
  {
    uint8_t event = 1;
    osMessageQueuePut(buttonQueue, &event, 0, 0);
  }
}
```

From an interrupt, the timeout **must be 0**: an interrupt can't wait.

The button task:

```c
static void ButtonTask(void *argument)
{
  for (;;)
  {
    uint8_t event;
    osMessageQueueGet(buttonQueue, &event, NULL, osWaitForever);
    armed = !armed;
    HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, armed);
    Log_Print("armed=%d\n", armed);
  }
}
```

The task sleeps until a press, then wakes up within microseconds. Mechanical contacts bounce, so one press can send several events: ignore events less than 50 ms after the last accepted one (compare `osKernelGetTickCount()` values). `armed` is written by one task and read by another; a single `uint8_t` is atomic on a Cortex-M, so `volatile` is enough.

### Step 5: Mutex for the debug output
Three tasks print (`button`, `status`, and `control` when it warns). `printf` through semihosting is one long operation. If the scheduler switches tasks in the middle of it, the lines come out mixed together. Protect it:

```c
static osMutexId_t logMutex;

void Log_Print(const char *format, ...)
{
#ifdef DEBUG_LOG_ENABLED
  osMutexAcquire(logMutex, osWaitForever);
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
  osMutexRelease(logMutex);
#endif
}
```

Create the mutex in `App_CreateTasks()` with `osMutexNew(NULL)`, and use `Log_Print` instead of `LOG` in every task. The `#ifdef` keeps the same rule as `LOG`: in Release no `printf` is linked or called.

The latest-data struct is shared too: `control` writes both fields, `status` reads them, and a task switch between the two writes gives a half-updated copy. Protect it with a second mutex (`dataMutex`), held only while copying the struct in or out.

Keep the locked section short: a task holding the lock for several milliseconds (semihosting is slow) makes the others wait. A mutex (unlike a plain semaphore) supports **priority inheritance**: if a high-priority task waits for a lock held by a low-priority task, the low one is temporarily boosted so it finishes quickly. The same pattern protects any shared resource, such as an I2C bus used by two drivers.

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
2. In the `status` task, every 5 s print for each task `uxTaskGetStackHighWaterMark(handle)`: the smallest amount of stack that has ever been free, in words. Keep at least ~50 words of headroom. `printf` alone uses over a kilobyte of stack, so give tasks that print `stack_size = 512 * 4` or more.

### Step 7: Compare
- Measure how quickly LD4 reacts to a button press while the `status` task is printing, versus a superloop version with a blocking `printf` in `App_Update()`.
- Look at the code size (build output) and RAM usage of both.
- Which version is easier to read? Which is easier to debug?

There is no universal answer. Small, simple devices are often better as a superloop; devices with networking, displays and several independent jobs benefit from an RTOS.

## Check it works
- [ ] All tasks run; LD6 blinks steadily even while the `status` task is printing
- [ ] A button press toggles LD4 (armed) instantly, one toggle per press
- [ ] Armed + warm chip → LD5 lights; disarmed → it doesn't
- [ ] Printed lines never come out mixed together
- [ ] The stack report shows reasonable headroom for every task

## Troubleshooting
| Symptom | Cause |
|---|---|
| Hard fault at start | queue/mutex used before it was created, or heap too small (`osThreadNew` returned NULL) |
| Hangs in `vPortValidateInterruptPriority` / assert | an interrupt with priority < 5 called an RTOS function |
| `HAL_GetTick` stuck | Timebase still on SysTick: use TIM6 |
| Task never runs | a higher-priority task never blocks (busy loop without `osDelay`) |
| Mixed-up `printf` output | two tasks printing at once without a mutex |
| Stack overflow hook hit | increase that task's `stack_size` |

## Challenges
1. Use **DMA + a semaphore** for a slow transfer (for example an OLED update, lesson 13): start the DMA transfer, then wait on a semaphore that the transfer-complete callback releases. The task sleeps during the transfer instead of busy-waiting.
2. Use a **software timer** (`osTimerNew`) to auto-disarm 30 s after arming: a safety net independent of the control task.
3. Add a **watchdog** (IWDG): a supervisor task refreshes it only if every other task reported in recently. If any task hangs, the board resets itself.
4. Use FreeRTOS's run-time stats to show CPU usage per task.
5. **Capstone: the plant system on FreeRTOS.** This is the one place where building on a finished lesson makes sense, so it's an optional challenge and not part of the lesson. Cut a branch from your finished `18-plant-watering` branch (not from `master`), add FreeRTOS with the CubeMX steps above, and split the superloop into tasks:

   | Task | Priority | Loop |
   |---|---|---|
   | `control` | AboveNormal | wait for sensor data from a queue → plant state machine → pump |
   | `sensors` | Normal | every 1 s: read soil and RTC → `SensorData` into the queue |
   | `display` | BelowNormal | every 200 ms: draw the OLED from the latest data |
   | `input` | Normal | every 10 ms: read the encoder and button → menu |

   The pump must never wait for a 25 ms OLED redraw, so `control` sits above `display`. If you add an I2C sensor (BMP280, lesson 15) next to the OLED, both tasks share I2C1: guard each group of I2C calls with a mutex, as in Step 5.

---

## Where next?
You've now used every major peripheral type, written drivers from datasheets, and built a complete device. Good next steps:
- **Low power**: STOP mode, waking from the RTC or a button, measuring µA.
- **Bare-metal registers**: rewrite one driver (e.g. GPIO or UART) without HAL, using RM0090.
- **A custom PCB**: design a board for one of your lesson projects in KiCad.
- **Unit tests** on the PC for the pure-logic modules (PID, state machines, parsers).
