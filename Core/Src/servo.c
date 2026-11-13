#include "servo.h"
#include "main.h"

extern TIM_HandleTypeDef htim12;

#define SERVO_MIN_US 1000
#define SERVO_MAX_US 2000
#define SERVO_STEP_MS 2

static uint32_t current = 90;
static uint32_t target = 90;
static uint32_t lastStep;

static void SetAngle(uint32_t degrees)
{
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1,
        SERVO_MIN_US + degrees * (SERVO_MAX_US - SERVO_MIN_US) / 180);
}

void Servo_Init(void)
{
    SetAngle(current);
    HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1);
}

void Servo_MoveTo(uint32_t degrees)
{
    target = degrees > 180 ? 180 : degrees;
}

void Servo_Update(void)
{
    uint32_t now = HAL_GetTick();
    if (now - lastStep < SERVO_STEP_MS)
        return;
    lastStep = now;

    if (current < target)
        current++;
    else if (current > target)
        current--;
    else
        return;

    SetAngle(current);
}
