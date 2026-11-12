#include "analog_in.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;

static volatile uint16_t dmaBuffer[ANALOG_COUNT];
static int32_t filteredX16[ANALOG_COUNT];
static uint32_t lastFilterTick;

void AnalogIn_Init(void)
{
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)dmaBuffer, ANALOG_COUNT);
}

void AnalogIn_Update(void)
{
  uint32_t now = HAL_GetTick();
  if (now - lastFilterTick < 1)
    return;
  lastFilterTick = now;

  for (uint32_t i = 0; i < ANALOG_COUNT; i++)
  {
    filteredX16[i] += dmaBuffer[i] - filteredX16[i] / 16;
  }
}

uint16_t AnalogIn_Raw(AnalogInput input)
{
  return dmaBuffer[input];
}

uint16_t AnalogIn_Filtered(AnalogInput input)
{
  return filteredX16[input] / 16;
}
