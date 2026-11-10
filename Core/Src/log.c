#include "log.h"
#include "main.h"

#ifdef DEBUG_LOG_ENABLED
static int SemihostCall(int op, void *args)
{
    register int r0 __asm("r0") = op;
    register void *r1 __asm("r1") = args;
    __asm volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
    return r0;
}

// Overrides the weak _write in syscalls.c; sending the whole buffer in one
// call avoids the console putting every character on its own line
int _write(int file, char *ptr, int len)
{
    // Semihosting BKPT HardFaults when no debugger is attached
    if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) == 0)
        return len;

    static int stdoutHandle = -1;
    if (stdoutHandle < 0)
    {
        // SYS_OPEN (0x01) of ":tt" in mode 4 ("w") is how newlib's rdimon gets the host console
        static const char tt[] = ":tt";
        int openArgs[3] = { (int)tt, 4, 3 };
        stdoutHandle = SemihostCall(0x01, openArgs);
    }

    // SYS_WRITE (0x05) returns the number of bytes NOT written
    int writeArgs[3] = { stdoutHandle, (int)ptr, len };
    return len - SemihostCall(0x05, writeArgs);
}

// syscalls.c declares __io_putchar weak; without a definition, code that calls it jumps to address 0 and HardFaults
int __io_putchar(int ch)
{
    char c = (char)ch;
    _write(1, &c, 1);
    return ch;
}
#else
// Must not call _write here: in Release the weak _write in syscalls.c calls __io_putchar, which would recurse forever
int __io_putchar(int ch)
{
    return ch;
}
#endif
