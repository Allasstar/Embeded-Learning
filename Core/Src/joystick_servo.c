#include "joystick_servo.h"
#include "analog_in.h"
#include "button.h"
#include "log.h"
#include "main.h"
#include "servo.h"
#include "timer.h"

extern TIM_HandleTypeDef htim4;

#define ADC_MAX 4095
#define DEAD_ZONE 80
#define CALIBRATION_SAMPLES 32
#define RATE_DIVISOR 400
#define LED_MAX 1000
#define MODE_BLINK_MS 150

typedef enum
{
    MODE_POSITION,
    MODE_RATE
} Mode;

static Timer controlTimer = { .periodMs = 20 };
static Button joyButton = {
    .port = JOY_SW_GPIO_Port,
    .pin = JOY_SW_Pin,
    .debounceMs = 20,
    .pressedState = GPIO_PIN_RESET,
    .lastState = GPIO_PIN_SET,
};

static int32_t centerX;
static int32_t centerY;
static int32_t angle = 90;
static Mode mode = MODE_POSITION;
static uint32_t blinkStartTick;

static int32_t Map(int32_t x, int32_t inMin, int32_t inMax, int32_t outMin, int32_t outMax)
{
    if (x < inMin) x = inMin;
    if (x > inMax) x = inMax;
    return outMin + (x - inMin) * (outMax - outMin) / (inMax - inMin);
}

static int32_t MapAxis(int32_t raw, int32_t center, int32_t outLow, int32_t outMid, int32_t outHigh)
{
    if (raw > center - DEAD_ZONE && raw < center + DEAD_ZONE)
        return outMid;
    if (raw < center)
        return Map(raw, 0, center - DEAD_ZONE, outLow, outMid);
    return Map(raw, center + DEAD_ZONE, ADC_MAX, outMid, outHigh);
}

static void PositionMode(int32_t raw)
{
    angle = MapAxis(raw, centerX, 0, 90, 180);
    Servo_MoveTo(angle);
}

static void RateMode(int32_t raw)
{
    int32_t offset = raw - centerX;
    if (offset > -DEAD_ZONE && offset < DEAD_ZONE)
        return;

    angle += offset / RATE_DIVISOR;
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    Servo_MoveTo(angle);
}

static void UpdateMode(void)
{
    if (!Button_WasPressed(&joyButton))
        return;

    mode = mode == MODE_POSITION ? MODE_RATE : MODE_POSITION;
    LOG("Mode: %s\n", mode == MODE_POSITION ? "position" : "rate");

    blinkStartTick = HAL_GetTick();
    HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
}

static void UpdateBlink(void)
{
    if (HAL_GetTick() - blinkStartTick >= MODE_BLINK_MS)
        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);
}

void JoystickServo_Init(void)
{
    int32_t sumX = 0;
    int32_t sumY = 0;

    for (int i = 0; i < CALIBRATION_SAMPLES; i++)
    {
        HAL_Delay(1);
        sumX += AnalogIn_Raw(ANALOG_JOY_X);
        sumY += AnalogIn_Raw(ANALOG_JOY_Y);
    }

    centerX = sumX / CALIBRATION_SAMPLES;
    centerY = sumY / CALIBRATION_SAMPLES;
    LOG("Joystick centre %ld, %ld\n", centerX, centerY);

    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    Timer_Restart(&controlTimer);
}

void JoystickServo_Update(void)
{
    UpdateMode();
    UpdateBlink();

    if (!Timer_Fired(&controlTimer))
        return;

    int32_t x = AnalogIn_Filtered(ANALOG_JOY_X);
    if (mode == MODE_POSITION)
        PositionMode(x);
    else
        RateMode(x);

    int32_t y = AnalogIn_Filtered(ANALOG_JOY_Y);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, MapAxis(y, centerY, 0, LED_MAX / 2, LED_MAX));
}
