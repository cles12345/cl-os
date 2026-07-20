#include "vga.h"

static uint8_t vga_colomn = 0;
static uint8_t vga_row = 0;
static volatile uint16_t* const vga = (uint16_t* const)0xB8000;
const uint16_t default_vga_color = (VGA_COLOR8_WHITE << 8) | (VGA_COLOR8_BLACK << 12); 
uint16_t current_vga_color = default_vga_color;

void print(const char* str){
    while (*str){
        switch (*str){
        case '\n': 
            new_line();
            break;
        case '\r': 
            vga_colomn = 0;
            break;
        case '\t':
            for (uint8_t i = 0; i < 4; i++){
                if (vga_colomn == VGA_WIDTH){
                    new_line();
                }
                vga_colomn++;
            }
            break;
        default:
            if (vga_colomn == VGA_WIDTH){
                new_line();
            }
            
            vga[vga_row * VGA_WIDTH + (vga_colomn++)] = *str | current_vga_color;
            break;
        }
        str++;
    }    
}

void scroll_up(void){
    for (uint16_t y = 0; y < VGA_HEIGHT; y++){
        for (uint16_t x = 0; x < VGA_WIDTH; x++){
            vga[(y - 1) * VGA_WIDTH + x] = vga[y * VGA_WIDTH + x];
        }
    }

    for (uint16_t x = 0; x < VGA_HEIGHT; x++){
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = ' ' | current_vga_color;
    }
}

void new_line(void){
    if (vga_row < VGA_HEIGHT - 1){
        vga_colomn = 0;
        vga_row++;   
    }
    else{
        scroll_up();
        vga_colomn = 0;
    }
}

void vga_reset(void){
    vga_colomn = 0;
    vga_row = 0;
    current_vga_color = default_vga_color;

    for (uint16_t y = 0; y < VGA_HEIGHT; y++){
        for (uint16_t x = 0; x < VGA_WIDTH; x++){
            vga[y * VGA_WIDTH + x] = ' ' | current_vga_color;
        }
    }
}