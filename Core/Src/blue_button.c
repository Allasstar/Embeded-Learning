#include "blue_button.h"
#include "button.h"
#include "main.h"

static Button button = {
    .port = B1_GPIO_Port,
    .pin = B1_Pin,
    .debounceMs = 20,
    .pressedState = GPIO_PIN_SET,
};

void BlueButton_SetDebounce(uint32_t ms)
{
    button.debounceMs = ms;
}

bool BlueButton_WasPressed(void)
{
    return Button_WasPressed(&button);
}
