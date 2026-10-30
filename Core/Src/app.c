#include "app.h"
#include "log.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include "doorbell.h"

#define DEBOUNCE_MS 50

static volatile uint32_t pressCount;
static volatile uint32_t lastPressTick;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != BTN_Pin) return;

    uint32_t now = HAL_GetTick();
    uint32_t diff = now - lastPressTick;

    if (diff < DEBOUNCE_MS) return;

    lastPressTick = now;
    pressCount++;
    Doorbell_OnPress();
}

void App_Start(void)
{
    Doorbell_Init();
    LOG("App started\n");
}

void App_Update(void)
{
    Doorbell_Update();
    static uint32_t lastPrinted;
    uint32_t count = pressCount;

    if(lastPrinted == count) return;
    lastPrinted = count;

    uint16_t btn = HAL_GPIO_ReadPin(BTN_GPIO_Port, BTN_Pin);
    LOG("BTN = %d  PressCount %d\n", btn, (unsigned long)count);
}
