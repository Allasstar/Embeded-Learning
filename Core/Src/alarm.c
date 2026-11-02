#include "alarm.h"
#include "main.h"
#include "log.h"

typedef struct
{
  const char *name;
  GPIO_TypeDef *port;
  uint16_t pin;
  GPIO_PinState activeLevel;
  GPIO_TypeDef *ledPort;
  uint16_t ledPin;
  uint8_t wasActive;
} Sensor;

static Sensor sensors[] =
{
  { "PIR",       PIR_GPIO_Port,       PIR_Pin,       GPIO_PIN_SET,   LD3_GPIO_Port,       LD3_Pin },
  { "Hall",      HALL_GPIO_Port,      HALL_Pin,      GPIO_PIN_RESET, LD4_GPIO_Port,       LD4_Pin },
  { "IR",        IR_OBST_GPIO_Port,   IR_OBST_Pin,   GPIO_PIN_RESET, LD5_GPIO_Port,       LD5_Pin },
  { "Vibration", VIBRATION_GPIO_Port, VIBRATION_Pin, GPIO_PIN_SET,   LD6_GPIO_Port,       LD6_Pin },
};

#define SENSOR_COUNT (sizeof(sensors) / sizeof(sensors[0]))

void Alarm_Init(void)
{
}

void Alarm_ToggleArmed(void)
{
}

void Alarm_Update(void)
{
  for (uint32_t i = 0; i < SENSOR_COUNT; i++)
  {
    Sensor *s = &sensors[i];
    uint8_t active = HAL_GPIO_ReadPin(s->port, s->pin) == s->activeLevel;

    HAL_GPIO_WritePin(s->ledPort, s->ledPin, active ? GPIO_PIN_SET : GPIO_PIN_RESET);

    if (active && !s->wasActive)
    {
      LOG("%s triggered\n", s->name);
    }
    s->wasActive = active;
  }
}
