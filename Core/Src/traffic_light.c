#include "traffic_light.h"
#include "log.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef enum
{
    STATE_RED,
    STATE_RED_YELLOW,
    STATE_GREEN,
    STATE_YELLOW,
    STATE_COUNT
} LightState;

static const uint32_t durationMs[STATE_COUNT] = {5000, 1000, 5000, 2000};
static const char *const stateNames[STATE_COUNT] = {"Red", "Red and Yellow",
                                                    "Green", "Yellow"};

static LightState state;
static uint32_t stateStart;
static uint32_t lastRequest;

static void SetLights(GPIO_PinState red, GPIO_PinState yellow,
                      GPIO_PinState green)
{
    HAL_GPIO_WritePin(TL_RED_GPIO_Port, TL_RED_Pin, red);
    HAL_GPIO_WritePin(TL_YELLOW_GPIO_Port, TL_YELLOW_Pin, yellow);
    HAL_GPIO_WritePin(TL_GREEN_GPIO_Port, TL_GREEN_Pin, green);
}

static void EnterState(LightState next)
{
    state = next;
    stateStart = HAL_GetTick();
    LOG("Traffic light state %d: %s\n", state, stateNames[state]);

    switch (state)
    {
    case STATE_RED:
        SetLights(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
        break;
    case STATE_RED_YELLOW:
        SetLights(GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_RESET);
        break;
    case STATE_GREEN:
        SetLights(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
        break;
    case STATE_YELLOW:
        SetLights(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);
        break;
    default:
        break;
    }
}

void TrafficLight_Init(void)
{
    lastRequest = 0;
    EnterState(STATE_RED);
}

void TrafficLight_RequestCrossing(void)
{
    uint32_t diff = HAL_GetTick() - stateStart;
    uint32_t left = durationMs[state] - 1000;
    uint32_t lastRequestDiff = HAL_GetTick() - lastRequest;

    if (state == STATE_GREEN && diff < left && lastRequestDiff > 2000)
    {
        stateStart = HAL_GetTick() - left;
        lastRequest = HAL_GetTick();
    }
}

void TrafficLight_Update(void)
{
    if (HAL_GetTick() - stateStart >= durationMs[state])
    {
        EnterState((state + 1) % STATE_COUNT);
    }
}
