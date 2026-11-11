#include "app.h"
#include "blue_button.h"
#include "log.h"
#include "main.h"
#include "ultrasonic.h"

#define LED_COUNT (sizeof(ledPins) / sizeof(ledPins[0]))
#define DISTANCE_LOG_PERIOD_MS 250

static const uint16_t ledPins[] = {LD4_Pin, LD3_Pin, LD5_Pin, LD6_Pin};
static uint8_t ledIndex = 0;
static uint32_t lastDistanceLogTick;

static bool IsBlinkOn(uint32_t periodMs);
static void WriteLedPin(uint8_t index, GPIO_PinState pinState);
static void LogDistance(void);

void App_Start(void)
{
    LOG("App started\n");
    Ultrasonic_Init();
}

void App_Update(void)
{
    Ultrasonic_Update();
    LogDistance();

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
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
        Ultrasonic_OnCapture(htim);
}

// Semihosting printf halts the CPU for milliseconds; logging every 60 ms reading would flood it
static void LogDistance(void)
{
    uint32_t now = HAL_GetTick();
    if (now - lastDistanceLogTick < DISTANCE_LOG_PERIOD_MS)
        return;

    lastDistanceLogTick = now;

    int32_t mm = Ultrasonic_GetMm();
    if (mm == ULTRASONIC_NO_READING)
        LOG("out of range\n");
    else
        LOG("%ld mm\n", mm);
}

static bool IsBlinkOn(uint32_t periodMs)
{
    return HAL_GetTick() % periodMs < periodMs / 2;
}

static void WriteLedPin(uint8_t index, GPIO_PinState pinState)
{
    HAL_GPIO_WritePin(GPIOD, ledPins[index], pinState);
}
