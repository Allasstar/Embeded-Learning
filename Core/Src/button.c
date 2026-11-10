#include "button.h"

bool Button_WasPressed(Button *button)
{
    uint32_t tick = HAL_GetTick();
    if (tick - button->lastSampleTick < button->debounceMs)
        return false;

    button->lastSampleTick = tick;
    GPIO_PinState state = HAL_GPIO_ReadPin(button->port, button->pin);
    bool pressed = state == button->pressedState && button->lastState != button->pressedState;
    button->lastState = state;
    return pressed;
}
