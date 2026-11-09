#include "buzzer.h"
#include "main.h"
#include <stdint.h>

extern TIM_HandleTypeDef htim9;

uint32_t const VOLUME = 1;

void Buzzer_Init(void)
{
    Buzzer_Off();
    HAL_TIM_PWM_Start(&htim9, TIM_CHANNEL_1);
}

void Buzzer_Tone(uint32_t frequency)
{
    uint32_t period = 1000000 / frequency - 1;
    __HAL_TIM_SET_AUTORELOAD(&htim9, period);
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, (period + 1) * VOLUME / 200);
}

void Buzzer_Off(void)
{
    __HAL_TIM_SET_COMPARE(&htim9, TIM_CHANNEL_1, 0);
}