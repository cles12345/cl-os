#pragma once
#include "stdint.h"
#include "vga.h"
#include "utill.h"

#define KEY_LSHIFT   0x2A
#define KEY_RSHIFT   0x36
#define KEY_CAPS     0x3A 
#define KEY_SPACE    0x39
#define KEY_ENTER    0x1C
#define KEY_BACKSPACE 0x0E
#define KEY_TAB      0x0F

void keyboard_handler(void);