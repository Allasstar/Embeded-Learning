#pragma once

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define ULTRASONIC_NO_READING (-1)

void Ultrasonic_Init(void);
void Ultrasonic_Update(void);
void Ultrasonic_OnCapture(TIM_HandleTypeDef *htim);
int32_t Ultrasonic_GetMm(void);
