#include "traffic_light.h"
#include "main.h"
#include "log.h"

typedef enum
{
  STATE_RED,
  STATE_RED_YELLOW,
  STATE_GREEN,
  STATE_YELLOW,
  STATE_COUNT
} LightState;

static const uint32_t durationMs[STATE_COUNT] = { 5000, 1000, 5000, 2000 };

static LightState state;
static uint32_t stateStart;

static void SetLights(GPIO_PinState red, GPIO_PinState yellow, GPIO_PinState green)
{
  HAL_GPIO_WritePin(TL_RED_GPIO_Port, TL_RED_Pin, red);
  HAL_GPIO_WritePin(TL_YELLOW_GPIO_Port, TL_YELLOW_Pin, yellow);
  HAL_GPIO_WritePin(TL_GREEN_GPIO_Port, TL_GREEN_Pin, green);
}

static void EnterState(LightState next)
{
  state = next;
  stateStart = HAL_GetTick();
  LOG("Traffic light state %d\n", state);

  switch (state)
  {
    case STATE_RED:        SetLights(GPIO_PIN_SET,   GPIO_PIN_RESET, GPIO_PIN_RESET); break;
    case STATE_RED_YELLOW: SetLights(GPIO_PIN_SET,   GPIO_PIN_SET,   GPIO_PIN_RESET); break;
    case STATE_GREEN:      SetLights(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);   break;
    case STATE_YELLOW:     SetLights(GPIO_PIN_RESET, GPIO_PIN_SET,   GPIO_PIN_RESET); break;
    default: break;
  }
}

void TrafficLight_Init(void)
{
  EnterState(STATE_RED);
  LOG("StateStart: %lu\n", stateStart);
}

void TrafficLight_Update(void)
{
  if (HAL_GetTick() - stateStart >= durationMs[state])
  {
    EnterState((state + 1) % STATE_COUNT);
  }
}
