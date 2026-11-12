#pragma once
#include <stdint.h>

typedef enum
{
  ANALOG_POT,
  ANALOG_JOY_X,
  ANALOG_JOY_Y,
  ANALOG_LIGHT,
  ANALOG_COUNT
} AnalogInput;

void AnalogIn_Init(void);
void AnalogIn_Update(void);
uint16_t AnalogIn_Raw(AnalogInput input);
uint16_t AnalogIn_Filtered(AnalogInput input);
