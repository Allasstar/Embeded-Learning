#pragma once
#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint32_t debounceMs;
    GPIO_PinState pressedState;
    GPIO_PinState lastState;
    uint32_t lastSampleTick;
} Button;

bool Button_WasPressed(Button *button);