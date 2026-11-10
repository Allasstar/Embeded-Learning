#include "app.h"
#include "log.h"
#include "main.h"
#include "servo.h"

#define LED_COUNT (sizeof(ledPins) / sizeof(ledPins[0]))

static const uint16_t ledPins[] = {LD4_Pin, LD3_Pin, LD5_Pin, LD6_Pin};
static uint8_t ledIndex = 0;
static GPIO_PinState lastButton = GPIO_PIN_RESET;
static uint32_t lastTick = 0;

void App_Start(void)
{
    LOG("App started\n");
    Servo_Init();
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
            ledIndex = (ledIndex + 1) % LED_COUNT;
            LOG("ledIndex = %d\n", ledIndex);

            for (uint8_t i = 0; i < LED_COUNT; i++)
            {
                HAL_GPIO_WritePin(GPIOD, ledPins[i], GPIO_PIN_RESET);
            }

            Servo_NextPosition();
        }
        lastButton = button;
    }

    if (tick % 1000 < 500)
        HAL_GPIO_WritePin(GPIOD, ledPins[ledIndex], GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(GPIOD, ledPins[ledIndex], GPIO_PIN_RESET);
}
