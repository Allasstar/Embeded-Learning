#include "app.h"
#include "buzzer.h"
#include "log.h"
#include "main.h"

void App_Start(void)
{
    Buzzer_Init();
    Buzzer_Tone(440);
    LOG("App started\n");
}

void App_Update(void)
{
}
