#include "led_fade.h"
#include "main.h"

#define LED_COUNT        4
#define LEVEL_MAX        1000
#define BREATH_PERIOD    (2 * LEVEL_MAX)
#define LED_PHASE_STEP   250
#define BREATH_STEP_MS   2
#define HUE_COUNT        1536
#define HUE_STEP_MS      5

extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

static const uint32_t ledChannels[LED_COUNT] =
{
  TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
};

static uint32_t lastBreathStep;
static uint32_t lastHueStep;
static uint32_t breathPhase;
static uint32_t hue;

static uint32_t Gamma(uint32_t level)
{
  return (level * level) / LEVEL_MAX;
}

static uint32_t Triangle(uint32_t phase)
{
  phase %= BREATH_PERIOD;
  return phase < LEVEL_MAX ? phase : BREATH_PERIOD - phase;
}

static void Rgb_Set(uint32_t r, uint32_t g, uint32_t b)
{
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, Gamma(r));
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, Gamma(g));
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, Gamma(b));
}

static void Rgb_SetHue(uint32_t h)
{
  uint32_t segment = (h / 256) % 6;
  uint32_t x = (h % 256) * LEVEL_MAX / 255;

  switch (segment)
  {
    case 0:  Rgb_Set(LEVEL_MAX, x, 0);             break;
    case 1:  Rgb_Set(LEVEL_MAX - x, LEVEL_MAX, 0); break;
    case 2:  Rgb_Set(0, LEVEL_MAX, x);             break;
    case 3:  Rgb_Set(0, LEVEL_MAX - x, LEVEL_MAX); break;
    case 4:  Rgb_Set(x, 0, LEVEL_MAX);             break;
    default: Rgb_Set(LEVEL_MAX, 0, LEVEL_MAX - x); break;
  }
}

void LedFade_Init(void)
{
  for (uint32_t i = 0; i < LED_COUNT; i++)
    HAL_TIM_PWM_Start(&htim4, ledChannels[i]);

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
}

void LedFade_Update(void)
{
  uint32_t now = HAL_GetTick();

  if (now - lastBreathStep >= BREATH_STEP_MS)
  {
    lastBreathStep = now;
    breathPhase = (breathPhase + 1) % BREATH_PERIOD;

    for (uint32_t i = 0; i < LED_COUNT; i++)
    {
      uint32_t level = Triangle(breathPhase + i * LED_PHASE_STEP);
      __HAL_TIM_SET_COMPARE(&htim4, ledChannels[i], Gamma(level) * 100 / LEVEL_MAX);

    }
  }

  if (now - lastHueStep >= HUE_STEP_MS)
  {
    lastHueStep = now;
    hue = (hue + 1) % HUE_COUNT;
    Rgb_SetHue(hue);
  }
}
