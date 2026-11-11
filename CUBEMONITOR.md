# Live variable graphs with STM32CubeMonitor

Plot firmware variables as live graphs while the board runs: no `printf`, no code changes, no extra wires. CubeMonitor reads RAM through the ST-LINK on the board.

Running example: lesson 07, plotting `distanceMm` (filtered) and `echoUs` (raw) from `ultrasonic.c`.

---

## 1. How it works

```
.elf file  ──►  CubeMonitor finds each variable's RAM address (e.g. distanceMm = 0x20000044)
                        │
ST-LINK (SWD) ──►  reads that address N times per second while the CPU keeps running
                        │
                   chart in the Dashboard (browser)
```

- **The CPU is never halted.** The debug port reads RAM in the background (Cortex-M "direct" memory access).
- **Only global and `static` variables** can be read. Locals live on the stack or in registers and have no fixed address.
- **The `.elf` must match the firmware on the board.** Addresses come from the `.elf`; if they don't match, you plot garbage.

| Tool | Shows | Use it for |
|---|---|---|
| Semihosting `LOG` | Text, when the code prints | Events, messages |
| **CubeMonitor** | Variables over time (ms resolution) | How data behaves: distances, angles, ADC values, filter output |
| Logic analyzer | Real pin signals (µs/ns) | PWM, trigger/echo pulses, UART/I2C bytes |

---

## 2. Install

1. st.com → search **STM32CubeMonitor** → *Get Software* → **STM32CubeMon-Win** (*Get latest*). Needs a free ST account.
2. Ignore the other rows: **-PWR** (power measurement board), **-RF** (STM32WB radio), **-UCPD** (USB-C Power Delivery).
3. Install and start it. It opens a flow editor (based on Node-RED) with a ready-made **Basic_Flow**.

---

## 3. The Basic_Flow

```
START Acquisition ─┐                  ┌─► myProbe_Out        (sends "read these variables" to the ST-LINK)
                   ├─► myVariables ───┤
STOP Acquisition  ─┘    (list)        └─► show notification

myProbe_In ──► myVariables ──► myChart  (values coming back → graph)
               (processing)
Clear Graphs ─────────────────► myChart
```

Use it as it is. You only configure 3 nodes: **myVariables** (top), **myProbe_Out**, **myProbe_In**.
A red triangle on a node means it's not configured yet.

---

## 4. Setup

### 4.1 Build first
Build the **Debug** configuration so `build/Debug/Test_DISC1.elf` exists. Release is optimized: variables may be removed or kept in registers.

### 4.2 Variables node (top `myVariables`)
Double-click it.

1. **Executable** → **+** (new exe-config):
   - **Name**: anything, e.g. `Debug_Disc1`
   - **Folder**: the **folder only**, no file name:
     ```
     W:\Embedded\Test_DISC1\build\Debug
     ```
     The grey `/Users/myname/exefolder` is only placeholder text. If the field stays red ("path does not exist"), the folder is wrong. Try forward slashes: `W:/Embedded/Test_DISC1/build/Debug`.
   - **File**: open the dropdown → `Test_DISC1.elf`. Wait for "parsing (…%)" to finish.
   - **Variable List**: use *Filter on variable name* at the bottom, tick the variables you want (`distanceMm`, `echoUs`).
   - **Add**.
2. Check the list: name, address, type must make sense.

   | Name | Start Address | Type |
   |---|---|---|
   | distanceMm | 0x20000044 | Signed 32-bit (`int32_t`) |
   | echoUs | 0x200009cc | Unsigned 32-bit (`uint32_t`) |

3. **Acquisition parameters**:

   | Field | Value | Why |
   |---|---|---|
   | Sampling frequency | Custom 20 Hz | Sample a bit faster than the data changes. Ultrasonic gives a new reading every 60 ms (≈16 Hz); faster sampling only repeats the same value |
   | Acquisition mode | direct | Reads RAM while the CPU runs. *Snapshot* needs code support in the firmware |
   | Trigger start mode | off | Start immediately. A trigger starts recording only when a variable crosses a threshold |

4. **Done**.

### 4.3 Probe nodes
1. Double-click **myProbe_Out** → next to *Probe* click the pencil (new probe config):
   - **Name**: e.g. `ST-Link-Disc1`
   - **Probe**: the DISC1 shows up as `ST-Link v2-1B <serial>`
   - **Protocol**: SWD. **Frequency**: default (1.8 MHz)
   - **Add** → **Done**
2. Double-click **myProbe_In** → pick the **same** `ST-Link-Disc1` (don't create a second one) → **Done**.

The red triangles disappear.

---

## 5. Use

1. Flash the board, then **stop the VS Code debug session**. Only one program can own the ST-LINK at a time. The firmware keeps running.
2. **DEPLOY** (top right). Needed after **every** change in the flow.
3. **DASHBOARD**: opens in the browser.
4. **START Acquisition**: lines appear in the chart.
5. **STOP Acquisition** before you go back to VS Code to debug or flash.

In the chart:
- Click a variable name in the legend to hide or show it.
- **Clear Graphs** resets the chart.

What to look for in lesson 07: move your hand in front of the sensor. `echoUs` jumps on every bad reading; `distanceMm` stays smooth. The gap between the two lines is the median filter at work.

---

## 6. After changing code

Every rebuild can **move variables to new addresses**. CubeMonitor keeps the old ones until told otherwise.

1. Build + flash.
2. Open the variables node → pencil next to *Executable* → re-select the file so it parses the new `.elf`.
3. Re-tick the variables if needed → **Done** → **DEPLOY**.

Symptom of forgetting this: values that make no sense (huge numbers, never changing) while the firmware works fine.

---

## 7. Troubleshooting

| Symptom | Cause |
|---|---|
| "path does not exist" | Folder field empty or contains the file name. Folder only. |
| Variable not in the list | Wrong `.elf` (Release, old build), or the variable is a local inside a function |
| Probe not listed / connection failed on START | VS Code debug session still running, or USB unplugged |
| Values stuck at 0 or -1 | Firmware not running: press the black RESET button |
| Nonsense values | `.elf` doesn't match the flashed firmware: re-parse (section 6) |
| Chart is a staircase | Sampling faster than the variable changes. Normal, not a bug |
| Nothing changes after editing the flow | Forgot **DEPLOY** |

---

## 8. Ideas per lesson

| Lesson | Plot |
|---|---|
| 04 PWM fading | brightness / CCR value: watch the fade curve |
| 06 Servo | target angle vs pulse width |
| 07 Ultrasonic | `echoUs` vs `distanceMm`: raw vs filtered |
| 08 ADC | raw ADC value: see noise, try averaging |
| 14 MPU6050 | accel X/Y/Z, gyro: tilt the board and watch |
| 17 PID | setpoint, measured value, output: tune by watching the curves |

To plot something from a function, make it a `static` variable at file level (or a global). Locals can't be read.
