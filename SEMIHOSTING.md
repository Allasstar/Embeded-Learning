# printf debugging with semihosting (VS Code + ST-LINK)

Print text from the STM32 straight into VS Code over the ST-LINK USB cable you already use for flashing. No UART, no extra wires.

---

## 1. How it works

**Semihosting** lets the chip ask the debugger to perform I/O for it:

1. Firmware puts an operation number in `r0` and a pointer to an argument block in `r1`.
2. It executes `bkpt 0xAB`, which halts the core.
3. The debugger (ST-LINK GDB server) sees the breakpoint, reads `r0`/`r1`, does the job (e.g. prints text on the PC), writes the result into `r0` and resumes the core.

```
printf("...")  ->  newlib  ->  _write()  ->  SYS_WRITE via bkpt 0xAB  ->  ST-LINK GDB server  ->  STM32Cube TCP Console
```

Operations used here:

| Op | Name | Purpose |
|---|---|---|
| `0x01` | `SYS_OPEN` | Open `":tt"` in mode 4 (write): returns a handle for the host console |
| `0x05` | `SYS_WRITE` | Write `len` bytes from a buffer to that handle; returns the number of bytes **not** written |

---

## 2. Setup

### 2.1 `main.c`

Include `stdio.h`:

```c
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */
```

Add the semihosting functions:

```c
/* USER CODE BEGIN 0 */
static int SemihostCall(int op, void *args)
{
  register int r0 __asm("r0") = op;
  register void *r1 __asm("r1") = args;
  __asm volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
  return r0;
}

int _write(int file, char *ptr, int len)
{
  // Semihosting BKPT HardFaults when no debugger is attached
  if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == 0)
    return len;

  static int stdoutHandle = -1;
  if (stdoutHandle < 0)
  {
    static const char tt[] = ":tt";
    int openArgs[3] = { (int)tt, 4, 3 };
    stdoutHandle = SemihostCall(0x01, openArgs);
  }

  int writeArgs[3] = { stdoutHandle, (int)ptr, len };
  return len - SemihostCall(0x05, writeArgs);
}

int __io_putchar(int ch)
{
  char c = (char)ch;
  _write(1, &c, 1);
  return ch;
}
/* USER CODE END 0 */
```

Keep everything inside the `USER CODE` blocks, or CubeMX deletes it on the next Generate Code.

### 2.2 `.vscode/launch.json`

Add `serverSemihosting` to the ST-LINK configuration. Semihosting is on by default in the extension; this just makes it explicit.

```json
{
    "type": "stlinkgdbtarget",
    "request": "launch",
    "name": "STM32Cube: Launch ST-Link GDB Server",
    "cwd": "${workspaceFolder}",
    "preBuild": "${command:st-stm32-ide-debug-launch.build}",
    "runEntry": "main",
    "serverSemihosting": {
        "enabled": true,
        "mode": "console"
    },
    "imagesAndSymbols": [
        { "imageFileName": "${command:st-stm32-ide-debug-launch.get-projects-binary-from-context1}" }
    ]
}
```

### 2.3 Build preset

Select the **Debug** CMake preset. Release builds with `-g0`, which strips debug info, so breakpoints and stepping won't work. Semihosting output itself works in both.

---

## 3. Usage

```c
LOG("Toggling LD4\n");
LOG("counter = %d\n", counter);
```

1. Press **F5** (Run and Debug → *STM32Cube: Launch ST-Link GDB Server*).
2. It halts at `main()`. Press **F5** again to continue.
3. Open the **STM32Cube TCP Console** tab in the bottom panel. Output appears there.

End messages with `\n`. `stdout` is line-buffered, so text without a newline stays in the buffer and isn't shown yet.

---

## 4. Why each piece exists

| Piece | Reason |
|---|---|
| `_write` override | `syscalls.c` defines `_write` as `weak`, so this one replaces it. It sends the whole `printf` buffer in one `SYS_WRITE`. |
| `DHCSR` `C_DEBUGEN` check | `bkpt` with no debugger attached causes a HardFault. The check lets the same firmware run standalone; output is simply dropped. |
| `__io_putchar` | `syscalls.c` declares it `weak` and never defines it. If anything calls it, execution jumps to address 0 and HardFaults. |
| `SYS_OPEN ":tt"` once | The standard way (used by newlib's `rdimon`) to get the host console handle. The handle is cached in a `static`. |

---

## 5. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| `implicit declaration of function 'printf'` | Missing `#include <stdio.h>`. |
| Board freezes on the first `printf`, LED stops | `__io_putchar` / `_write` not defined (jump to address 0), or `bkpt` without the `DHCSR` check while running without a debugger. |
| One character per line | Output sent char by char (`SYS_WRITEC`). Use `SYS_WRITE` from `_write` as above. |
| Boxes (`□`) instead of text | `SYS_WRITE0` isn't handled correctly by the ST-LINK GDB server. Use `SYS_OPEN` + `SYS_WRITE`. |
| Nothing printed | Program is halted at `main` (press Continue); message has no `\n`; looking at the wrong tab (use **STM32Cube TCP Console**). |
| Breakpoints don't hit, `0x0800xxxx in main ()` with no line | Release build. Switch to the **Debug** preset. |
| Serial Monitor on COM3 shows nothing | Expected. COM3 is the ST-LINK virtual COM port, which isn't used here and isn't wired to the STM32 by default. |

---

## 6. Disabling it in production (Release)

Logging is compiled in only for the **Debug** CMake config.

`CMakeLists.txt` defines `DEBUG_LOG_ENABLED` for Debug builds only:

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEBUG_LOG_ENABLED>
)
```

`main.c` routes logging through a macro:

```c
/* USER CODE BEGIN PD */
#ifdef DEBUG_LOG_ENABLED
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif
/* USER CODE END PD */
```

and wraps the semihosting code:

```c
#ifdef DEBUG_LOG_ENABLED
// SemihostCall, _write, __io_putchar (section 2.1)
#else
int __io_putchar(int ch)
{
  return ch;
}
#endif
```

Rules:

- Always log with `LOG(...)`, never `printf` directly. In Release, `LOG` calls disappear completely: no `bkpt`, no CPU stalls, and newlib's `printf` isn't linked (saves several KB of flash).
- The Release `__io_putchar` must **not** call `_write`. In Release the weak `_write` from `syscalls.c` is used, and it calls `__io_putchar`, so they would call each other forever.
- Arguments inside `LOG(...)` aren't evaluated in Release, so don't put side effects in them (`LOG("%d", i++)`).

### Switching between Debug and Release

The Run and Debug dropdown is **not** where you pick it; that only chooses the launch configuration. Use the CMake extension:

- **CMake sidebar**: CMake icon in the activity bar → *Project Status* → *Configure* → click the preset → **Debug** or **Release**. Set *Build* to the same preset.
- **Command Palette** (`Ctrl+Shift+P`): `CMake: Select Configure Preset`, then `CMake: Select Build Preset`.

F5 then builds the active preset (the `preBuild` step in `launch.json`) and flashes it.

| Preset | `LOG` output | Breakpoints / stepping | Use for |
|---|---|---|---|
| Debug | Yes (STM32Cube TCP Console) | Yes (`-O0 -g3`) | Development |
| Release | Compiled out | No (`-Os -g0`) | Production firmware |

Check that Release really dropped it with the map file (`build/Release/Test_DISC1.map`): it shouldn't contain `_printf_i` / `_vfprintf_r`.

---

## 7. Limitations

- **Slow**: every `printf` halts the core while the PC handles it (around milliseconds). Don't print in tight loops or interrupts, and expect timing-sensitive code (USB, audio) to misbehave while printing.
- **Debugger only**: without a debug session the output is discarded.
- For fast or always-on logging, use a UART (USART2 on PA2/PA3) or SWO/ITM instead.
