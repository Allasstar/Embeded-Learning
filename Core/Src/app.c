#include "app.h"
#include "log.h"
#include "main.h"


#define LED_COUNT (sizeof(ledPins) / sizeof(ledPins[0]))

static const uint16_t ledPins[] = {LD4_Pin, LD3_Pin, LD5_Pin, LD6_Pin};
static uint8_t ledIndex = 0;
static GPIO_PinState lastButton = GPIO_PIN_RESET;
static uint32_t lastTick = 0;

void App_Start(void)
{
    LOG("App started\n");
}

void App_Update(void)
{
    LOG("PIR%d HALL%d IR%d VIB%d\n",
        HAL_GPIO_ReadPin(PIR_GPIO_Port, PIR_Pin),
        HAL_GPIO_ReadPin(HALL_GPIO_Port, HALL_Pin),
        HAL_GPIO_ReadPin(IR_OBST_GPIO_Port, IR_OBST_Pin),
        HAL_GPIO_ReadPin(VIBRATION_GPIO_Port, VIBRATION_Pin));
        
    HAL_Delay(300);
}
