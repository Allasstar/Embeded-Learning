#pragma once
#include <stdint.h>

void Servo_Init(void);
void Servo_SetPulse(uint32_t us);
void Servo_SetAngle(uint32_t degrees);
void Servo_MoveTo(uint32_t degrees);
void Servo_Update(void);
void Servo_NextPosition(void);
