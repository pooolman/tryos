#ifndef _INCLUDE_TERMINAL_IO_H_
#define _INCLUDE_TERMINAL_IO_H_

#include <stdint.h>
#include "console.h"
#include "string.h"


void printk(const char * format, ...);

void printk_color(vga_color_t fg, vga_color_t bg, const char * format, ...);

void iprintk(const char * format, ...);

void iprintk_color(vga_color_t fg, vga_color_t bg, const char * format, ...);

#endif //_INCLUDE_TERMINAL_IO_H_
