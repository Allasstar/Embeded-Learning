#pragma once

#include <stdbool.h>
#include <stdint.h>

void BlueButton_SetDebounce(uint32_t ms);
bool BlueButton_WasPressed(void);
