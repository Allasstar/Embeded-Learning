#include "app.h"
#include "analog_in.h"
#include "blue_button.h"
#include "log.h"
#include "main.h"
#include "timer.h"

#define LED_COUNT (sizeof(ledPins) / sizeof(ledPins[0]))

static const uint16_t ledPins[] = {LD4_Pin, LD3_Pin, LD5_Pin, LD6_Pin};
static uint8_t ledIndex = 0;

static bool IsBlinkOn(uint32_t periodMs);
static void WriteLedPin(uint8_t index, GPIO_PinState pinState);

static Timer logTimer = { .periodMs = 200 };

void App_Start(void)
{
    LOG("App started\n");
    AnalogIn_Init();
}

void App_Update(void)
{
    if (BlueButton_WasPressed())
    {
        ledIndex = (ledIndex + 1) % LED_COUNT;
        LOG("ledIndex = %d\n", ledIndex);

        for (uint8_t i = 0; i < LED_COUNT; i++)
        {
            WriteLedPin(i, GPIO_PIN_RESET);
        }
    }

    if (IsBlinkOn(1000))
    {
        WriteLedPin(ledIndex, GPIO_PIN_SET);
    }
    else
    {
        WriteLedPin(ledIndex, GPIO_PIN_RESET);
    }

    AnalogIn_Update();

    if (Timer_Fired(&logTimer))
    {
        LOG("pot: %4u  joyX: %4u  joyY: %4u  light: %4u\n",
            AnalogIn_Raw(ANALOG_POT),
            AnalogIn_Raw(ANALOG_JOY_X),
            AnalogIn_Raw(ANALOG_JOY_Y),
            AnalogIn_Raw(ANALOG_LIGHT));
    }
}

static bool IsBlinkOn(uint32_t periodMs)
{
    return HAL_GetTick() % periodMs < periodMs / 2;
}

static void WriteLedPin(uint8_t index, GPIO_PinState pinState)
{
    HAL_GPIO_WritePin(GPIOD, ledPins[index], pinState);
}
