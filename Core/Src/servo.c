#include "servo.h"
#include "log.h"
#include "main.h"

extern TIM_HandleTypeDef htim12;

#define SERVO_MIN_US 500
#define SERVO_MAX_US 2400
#define SERVO_MAX_DEG 180
#define SERVO_STEP_DEG 45

static uint32_t currentAngle = 0;

void Servo_Init(void)
{
    Servo_SetAngle(0);
    HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1);
}

void Servo_SetPulse(uint32_t us)
{
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, us);
}

void Servo_SetAngle(uint32_t degrees)
{
    if (degrees > SERVO_MAX_DEG)
        degrees = SERVO_MAX_DEG;
    currentAngle = degrees;
    Servo_SetPulse(SERVO_MIN_US + degrees * (SERVO_MAX_US - SERVO_MIN_US) / SERVO_MAX_DEG);
}

void Servo_NextPosition(void)
{
    uint32_t next = currentAngle + SERVO_STEP_DEG;
    if (next > SERVO_MAX_DEG)
        next = 0;
    Servo_SetAngle(next);
    LOG("servo angle = %lu\n", next);
}
