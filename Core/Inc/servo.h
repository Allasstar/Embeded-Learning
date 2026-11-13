#pragma once
#include <stdint.h>

void Servo_Init(void);
void Servo_MoveTo(uint32_t degrees);
void Servo_Update(void);
