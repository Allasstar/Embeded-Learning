#include "alarm.h"
#include "log.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef struct
{
    const char *name;
    GPIO_TypeDef *port;
    u16 pin;
    GPIO_PinState activeLevel;
    GPIO_TypeDef *ledPort;
    u16 ledPin;
    u8 wasActive;
} Sensor;

static Sensor sensors[] =
    {
        {"PIR", PIR_GPIO_Port, PIR_Pin, GPIO_PIN_SET, LD3_GPIO_Port, LD3_Pin},
        {"Hall", HALL_GPIO_Port, HALL_Pin, GPIO_PIN_RESET, LD4_GPIO_Port, LD4_Pin},
        {"IR", IR_OBST_GPIO_Port, IR_OBST_Pin, GPIO_PIN_RESET, LD5_GPIO_Port, LD5_Pin},
        {"Vibration", VIBRATION_GPIO_Port, VIBRATION_Pin, GPIO_PIN_SET, LD6_GPIO_Port, LD6_Pin},
};

#define SENSOR_COUNT (sizeof(sensors) / sizeof(sensors[0]))

static u8 armed;
static u8 alarmOn;
static u32 lastTick;
static const char *alarmSource;
static u8 sirenPhase;
static u32 armedAt;

void Alarm_Init(void)
{
    alarmOn = 0;
    armed = 0;
}

void Alarm_ToggleArmed(void)
{
    armed ^= 1;
    alarmOn = 0;

    for (u32 i = 0; i < SENSOR_COUNT; i++)
    {
        HAL_GPIO_WritePin(sensors[i].ledPort, sensors[i].ledPin, GPIO_PIN_RESET);
    }

    if (armed)
    {
        armedAt = HAL_GetTick();
    }
    LOG(armed ? "ARMED\n" : "DISARMED\n");
}


void Alarm_Update(void)
{
    for (u32 i = 0; i < SENSOR_COUNT; i++)
    {
        Sensor *s = &sensors[i];
        u8 active = HAL_GPIO_ReadPin(s->port, s->pin) == s->activeLevel;

        if(!alarmOn)
        {
            HAL_GPIO_WritePin(s->ledPort, s->ledPin, active ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        if (active && !s->wasActive)
        {
            LOG("%s triggered\n", s->name);
            if (armed && HAL_GetTick() - armedAt >= 10000)
            {
                alarmOn = 1;
                alarmSource = s->name;
            }
        }
        s->wasActive = active;
    }

    if (alarmOn)
    {
        u32 now = HAL_GetTick();

        if (now - lastTick >= 250)
        {
            lastTick = now;
            sirenPhase ^= 1;
        }

        HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, sirenPhase ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LD5_GPIO_Port, LD5_Pin, sirenPhase ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LD4_GPIO_Port, LD4_Pin, sirenPhase ? GPIO_PIN_RESET : GPIO_PIN_SET);
        HAL_GPIO_WritePin(LD6_GPIO_Port, LD6_Pin, sirenPhase ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}
