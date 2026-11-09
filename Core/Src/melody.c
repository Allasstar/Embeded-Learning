#include "melody.h"
#include "buzzer.h"
#include "main.h"
#include <stdint.h>

#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define REST 0

typedef struct
{
    uint16_t frequency;
    uint16_t durationMs;
} Note;

static const Note twinkle[] =
    {
        {NOTE_C4, 400},
        {NOTE_C4, 400},
        {NOTE_G4, 400},
        {NOTE_G4, 400},
        {NOTE_A4, 400},
        {NOTE_A4, 400},
        {NOTE_G4, 800},
        {NOTE_F4, 400},
        {NOTE_F4, 400},
        {NOTE_E4, 400},
        {NOTE_E4, 400},
        {NOTE_D4, 400},
        {NOTE_D4, 400},
        {NOTE_C4, 800},
};

#define NOTE_COUNT (sizeof(twinkle) / sizeof(twinkle[0]))
#define GAP_MS 40

static uint8_t playing;
static uint32_t noteIndex;
static uint32_t noteStart;

static void StartNote(void)
{
    noteStart = HAL_GetTick();
    if (twinkle[noteIndex].frequency == REST)
        Buzzer_Off();
    else
        Buzzer_Tone(twinkle[noteIndex].frequency);
}

void Melody_Play(void)
{
    playing = 1;
    noteIndex = 0;
    StartNote();
}

void Melody_Update(void)
{
    if (!playing)
        return;

    uint32_t elapsed = HAL_GetTick() - noteStart;

    if (elapsed >= twinkle[noteIndex].durationMs - GAP_MS)
        Buzzer_Off();

    if (elapsed >= twinkle[noteIndex].durationMs)
    {
        noteIndex++;
        if (noteIndex >= NOTE_COUNT)
        {
            playing = 0;
            return;
        }
        StartNote();
    }
}
