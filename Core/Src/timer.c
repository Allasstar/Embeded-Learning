#include "timer.h"
#include "main.h"

void Timer_Restart(Timer *timer)
{
    timer->startTick = HAL_GetTick();
}

bool Timer_Fired(Timer *timer)
{
    uint32_t now = HAL_GetTick();
    if (now - timer->startTick < timer->periodMs)
        return false;

    // Advancing by a whole period keeps the rhythm exact even if this call came a few ms late.
    timer->startTick += timer->periodMs;
    // After a long stall, skip the missed periods instead of firing once for each of them.
    if (now - timer->startTick >= timer->periodMs)
        timer->startTick = now;
    return true;
}

bool Timer_IsActive(const Timer *timer)
{
    return (HAL_GetTick() - timer->startTick) % timer->periodMs < timer->activeMs;
}
