#include "app.h"
#include "blue_button.h"
#include "log.h"
#include "main.h"

#define LED_COUNT (sizeof(ledPins) / sizeof(ledPins[0]))

static const uint16_t ledPins[] = {LD4_Pin, LD3_Pin, LD5_Pin, LD6_Pin};
static uint8_t ledIndex = 0;

static bool IsBlinkOn(uint32_t periodMs);
static void WritePin(uint8_t index, GPIO_PinState pinState);

void App_Start(void)
{
    LOG("App started\n");
}

void App_Update(void)
{
    if (BlueButton_WasPressed())
    {
        ledIndex = (ledIndex + 1) % LED_COUNT;
        LOG("ledIndex = %d\n", ledIndex);

        for (uint8_t i = 0; i < LED_COUNT; i++)
        {
            WritePin(i, GPIO_PIN_RESET);
        }
    }

    if (IsBlinkOn(1000))
    {
        WritePin(ledIndex, GPIO_PIN_SET);
    }
    else
    {
        WritePin(ledIndex, GPIO_PIN_RESET);
    }
}

static bool IsBlinkOn(uint32_t periodMs)
{
    return HAL_GetTick() % periodMs < periodMs / 2;
}

static void WritePin(uint8_t index, GPIO_PinState pinState)
{
    HAL_GPIO_WritePin(GPIOD, ledPins[index], pinState);
}
