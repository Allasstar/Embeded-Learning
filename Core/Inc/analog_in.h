#pragma once
#include <stdint.h>

// Order must match the ADC1 rank order in CubeMX: rank 1 = IN11 (PC1), rank 2 = IN12 (PC2)
typedef enum
{
    ANALOG_JOY_X,
    ANALOG_JOY_Y,
    ANALOG_COUNT
} AnalogInput;

void AnalogIn_Init(void);
void AnalogIn_Update(void);
uint16_t AnalogIn_Raw(AnalogInput input);
uint16_t AnalogIn_Filtered(AnalogInput input);
