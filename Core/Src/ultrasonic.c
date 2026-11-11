#include "ultrasonic.h"

extern TIM_HandleTypeDef htim1;

#define CYCLE_US 60000
#define NO_ECHO_TIMEOUT_MS 200
#define MIN_MM 20
#define MAX_MM 4000
#define HISTORY_SIZE 5

static volatile uint32_t riseTime;
static volatile uint32_t echoUs;
static volatile bool newReading;

static int32_t history[HISTORY_SIZE];
static uint32_t historyPos;

static int32_t distanceMm = ULTRASONIC_NO_READING;
static uint32_t lastReadingTick;

static int32_t EchoToMm(uint32_t us);
static int32_t Median(void);

void Ultrasonic_Init(void)
{
    for (uint32_t i = 0; i < HISTORY_SIZE; i++)
    {
        history[i] = ULTRASONIC_NO_READING;
    }

    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
}

void Ultrasonic_OnCapture(TIM_HandleTypeDef *htim)
{
    uint32_t t = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

    // "Both edges" doesn't report which edge fired; the echo pulse lasts >= 150 us,
    // far longer than interrupt entry, so the pin still shows the new level
    if (HAL_GPIO_ReadPin(US_ECHO_GPIO_Port, US_ECHO_Pin) == GPIO_PIN_SET)
    {
        riseTime = t;
    }
    else
    {
        echoUs = t >= riseTime ? t - riseTime : t + CYCLE_US - riseTime;
        newReading = true;
    }
}

void Ultrasonic_Update(void)
{
    uint32_t now = HAL_GetTick();

    if (newReading)
    {
        uint32_t us = echoUs;
        newReading = false;
        lastReadingTick = now;

        history[historyPos] = EchoToMm(us);
        historyPos = (historyPos + 1) % HISTORY_SIZE;
        distanceMm = Median();
    }
    else if (now - lastReadingTick > NO_ECHO_TIMEOUT_MS)
    {
        distanceMm = ULTRASONIC_NO_READING;
    }
}

int32_t Ultrasonic_GetMm(void)
{
    return distanceMm;
}

static int32_t EchoToMm(uint32_t us)
{
    // 343 m/s = 0.343 mm/us, halved for the round trip
    int32_t mm = (int32_t)(us * 343 / 2000);

    if (mm < MIN_MM || mm > MAX_MM)
        return ULTRASONIC_NO_READING;

    return mm;
}

static int32_t Median(void)
{
    int32_t s[HISTORY_SIZE];

    for (uint32_t i = 0; i < HISTORY_SIZE; i++)
    {
        s[i] = history[i];
    }

    for (uint32_t i = 1; i < HISTORY_SIZE; i++)
    {
        for (uint32_t j = i; j > 0 && s[j - 1] > s[j]; j--)
        {
            int32_t tmp = s[j];
            s[j] = s[j - 1];
            s[j - 1] = tmp;
        }
    }

    return s[HISTORY_SIZE / 2];
}
