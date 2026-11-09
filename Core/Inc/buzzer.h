#pragma once

#include <stdint.h>

void Buzzer_Init(void);
void Buzzer_Tone(uint32_t frequency);
void Buzzer_Off(void);