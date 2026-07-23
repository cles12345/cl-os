#pragma once
#include "stdint.h"
#include "utill.h"

void enable_cursor(uint8_t cursor_start, uint8_t cursor_end);
void update_cursor(int x, int y);