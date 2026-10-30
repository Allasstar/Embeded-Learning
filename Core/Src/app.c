#include "app.h"
#include "log.h"
#include "main.h"

void App_Start(void)
{
    LOG("App started\n");
}

void App_Update(void)
{
    LOG("BTN = %d\n", HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin));
    HAL_Delay(200);
}
