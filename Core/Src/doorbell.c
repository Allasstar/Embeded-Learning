#include "doorbell.h"
#include "main.h"

typedef enum
{
    DB_IDLE,
    DB_DING,
    DB_DONG
} DoorbellState;

static volatile uint8_t pressPending;
static volatile uint32_t lastPressTick;

static DoorbellState state = DB_IDLE;
static uint32_t stateStart;
static uint32_t lastToggle;

void Doorbell_Init(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

void Doorbell_OnPress(void)
{
    uint32_t now = HAL_GetTick();
    if (now - lastPressTick >= 50)
    {
        lastPressTick = now;
        pressPending = 1;
    }
}

static void Tone(uint32_t halfPeriodMs)
{
    uint32_t now = HAL_GetTick();
    if (now - lastToggle >= halfPeriodMs)
    {
        lastToggle = now;
        HAL_GPIO_TogglePin(BUZZER_GPIO_Port, BUZZER_Pin);
    }
}

void Doorbell_Update(void)
{
    uint32_t now = HAL_GetTick();

    if (pressPending && state == DB_IDLE)
    {
        pressPending = 0;
        state = DB_DING;
        stateStart = now;
    }

    switch (state)
    {
    case DB_DING:
        Tone(1);
        if (now - stateStart >= 300)
        {
            state = DB_DONG;
            stateStart = now;
        }
        break;

    case DB_DONG:
        Tone(2);
        if (now - stateStart >= 500)
        {
            state = DB_IDLE;
        }
        break;

    default:
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
        break;
    }
}
