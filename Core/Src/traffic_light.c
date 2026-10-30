#include "traffic_light.h"
#include "main.h"

static void SetLights(GPIO_PinState red, GPIO_PinState yellow, GPIO_PinState green)
{
  HAL_GPIO_WritePin(TL_RED_GPIO_Port, TL_RED_Pin, red);
  HAL_GPIO_WritePin(TL_YELLOW_GPIO_Port, TL_YELLOW_Pin, yellow);
  HAL_GPIO_WritePin(TL_GREEN_GPIO_Port, TL_GREEN_Pin, green);
}

void TrafficLight_Init(void)
{
  SetLights(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_RESET);
}

void TrafficLight_Update(void)
{
  SetLights(GPIO_PIN_SET, GPIO_PIN_RESET, GPIO_PIN_RESET);
  HAL_Delay(5000);
  SetLights(GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_RESET);
  HAL_Delay(1000);
  SetLights(GPIO_PIN_RESET, GPIO_PIN_RESET, GPIO_PIN_SET);
  HAL_Delay(5000);
  SetLights(GPIO_PIN_RESET, GPIO_PIN_SET, GPIO_PIN_RESET);
  HAL_Delay(2000);
}
