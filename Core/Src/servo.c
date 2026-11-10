#include "servo.h"
#include "log.h"
#include "main.h"

extern TIM_HandleTypeDef htim12;

#define SERVO_MIN_US 500
#define SERVO_MAX_US 2400
#define SERVO_MAX_DEG 180
#define SERVO_STEP_DEG 45
#define SERVO_MS_PER_DEG 10

static uint32_t currentAngle = 0;
static uint32_t targetAngle = 0;
static uint32_t lastStepTick = 0;

static uint32_t ClampAngle(uint32_t degrees)
{
    return degrees > SERVO_MAX_DEG ? SERVO_MAX_DEG : degrees;
}

static void ApplyAngle(uint32_t degrees)
{
    currentAngle = degrees;
    Servo_SetPulse(SERVO_MIN_US + degrees * (SERVO_MAX_US - SERVO_MIN_US) / SERVO_MAX_DEG);
}

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
    targetAngle = ClampAngle(degrees);
    ApplyAngle(targetAngle);
}

void Servo_MoveTo(uint32_t degrees)
{
    targetAngle = ClampAngle(degrees);
}

void Servo_Update(void)
{
    if (currentAngle == targetAngle)
        return;

    uint32_t now = HAL_GetTick();
    if (now - lastStepTick < SERVO_MS_PER_DEG)
        return;
    lastStepTick = now;

    ApplyAngle(currentAngle < targetAngle ? currentAngle + 1 : currentAngle - 1);
}

void Servo_NextPosition(void)
{
    uint32_t next = targetAngle + SERVO_STEP_DEG;
    if (next > SERVO_MAX_DEG)
        next = 0;
    Servo_MoveTo(next);
    LOG("servo target = %lu\n", next);
}
