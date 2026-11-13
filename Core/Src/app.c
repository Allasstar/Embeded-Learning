#include "app.h"
#include "analog_in.h"
#include "joystick_servo.h"
#include "log.h"
#include "servo.h"

void App_Start(void)
{
    LOG("App started\n");
    AnalogIn_Init();
    Servo_Init();
    JoystickServo_Init();
}

void App_Update(void)
{
    AnalogIn_Update();
    JoystickServo_Update();
    Servo_Update();
}
