#include "keyboard.h"

static bool caps_lock = false;
static bool shift_pressed = false;

static const char lower_case_scancode[128] = {
    0,      0,    '1', '2', '3', '4', '5', '6', '7', '8',
    '9',    '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r',
    't',    'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a',    's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'',   '`', 0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n',
    'm',    ',', '.', '/', 0, '*', 0, ' ', 0, 0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0
};

static const char upper_case_scancode[128] = {
    0,      0,    '!', '@', '#', '$', '%', '^', '&', '*',
    '(',    ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R',
    'T',    'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A',    'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"',    '~', 0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N',
    'M',    '<', '>', '?', 0, '*', 0, ' ', 0, 0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0,    0,    0,
    0,      0,   0,    0,    0,    0,    0,    0
};

void keyboard_handler(void){
    uint8_t scancode = inb(0x60);
    outb(0x20, 0x20);

    if (scancode & 0x80){
        uint8_t key = scancode & 0x7F;
        if (key == KEY_LSHIFT || key == KEY_RSHIFT){
            shift_pressed = false;
        }
        return;
    }

    if (scancode == KEY_CAPS){
        caps_lock = !caps_lock;
        return;
    }

    if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT){
        shift_pressed = true;
        return;
    }

    if (scancode == KEY_SPACE){
        print(" ");
        return;
    }
    
    if (scancode == KEY_ENTER){
        print("\n");
        return;
    }
    
    if (scancode == KEY_BACKSPACE){
        vga_delete(1);
        return;
    }

    if (scancode == KEY_TAB){
        print("\t");
        return;
    }

    if (shift_pressed || caps_lock){
        if (upper_case_scancode[scancode]){
            char str[2];
            str[0] = upper_case_scancode[scancode];
            str[1] = '\0';
            print(str);
        }
        return;
    }
    else{
        if (lower_case_scancode[scancode]){
            char str[2];
            str[0] = lower_case_scancode[scancode];
            str[1] = '\0';
            print(str);
        }
        return;
    }    
}