#include "app.h"
#include "main.h"
#include "log.h"
#include "led_fade.h"

void App_Start(void)
{
  LOG("App started\n");
  LedFade_Init();
}

void App_Update(void)
{
  LedFade_Update();
}
