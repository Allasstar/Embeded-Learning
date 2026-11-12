#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t periodMs;
    uint32_t activeMs;
    uint32_t startTick;
} Timer;

void Timer_Restart(Timer *timer);
bool Timer_Fired(Timer *timer);
bool Timer_IsActive(const Timer *timer);
