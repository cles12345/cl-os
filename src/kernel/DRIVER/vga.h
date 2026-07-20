#pragma once
#include "stdint.h"

#define VGA_COLOR8_BLACK 0
#define VGA_COLOR8_WHITE 15

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void print(const char* str);
void scroll_up(void);
void new_line(void);
void vga_reset(void);