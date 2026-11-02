#include "app.h"
#include "alarm.h"
#include "log.h"
#include "main.h"

static GPIO_PinState lastButton = GPIO_PIN_RESET;
static uint32_t lastTick = 0;

void App_Start(void)
{
    Alarm_Init();
    LOG("App started\n");
}

void App_Update(void)
{
    GPIO_PinState button = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);
    uint32_t tick = HAL_GetTick();

    if (tick - lastTick >= 20)
    {
        lastTick = tick;

        if (button == GPIO_PIN_SET && lastButton == GPIO_PIN_RESET)
        {
            Alarm_ToggleArmed();
        }
        lastButton = button;
    }

    Alarm_Update();
}
