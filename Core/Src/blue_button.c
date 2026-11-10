#include "blue_button.h"
#include "main.h"

static uint32_t debounceMs = 20;
static GPIO_PinState lastState = GPIO_PIN_RESET;
static uint32_t lastSampleTick = 0;

void BlueButton_SetDebounce(uint32_t ms)
{
    debounceMs = ms;
}

bool BlueButton_WasPressed(void)
{
    uint32_t tick = HAL_GetTick();
    if (tick - lastSampleTick < debounceMs)
        return false;

    lastSampleTick = tick;
    GPIO_PinState state = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
    bool pressed = state == GPIO_PIN_SET && lastState == GPIO_PIN_RESET;
    lastState = state;
    return pressed;
}
