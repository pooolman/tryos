/*
 * 本文件提供与文本模式显卡操作有关的工具函数原型和必要的
 * 数据类型定义
 */

#ifndef _INCLUDE_CONSOLE_H_
#define _INCLUDE_CONSOLE_H_

#include <stdint.h>

/* 定义用于文本模式显卡的颜色表 */
typedef enum vga_color{
	VGA_COLOR_BLACK	= 0,
	VGA_COLOR_BLUE	= 1,
	VGA_COLOR_GREEN	= 2,
	VGA_COLOR_CYAN	= 3,
	VGA_COLOR_RED	= 4,
	VGA_COLOR_MAGENTA	= 5,
	VGA_COLOR_BROWN		= 6,
	VGA_COLOR_WHITE		= 7,
	VGA_COLOR_LIGHT_BLACK	= 8,
	VGA_COLOR_LIGHT_BLUE	= 9,
	VGA_COLOR_LIGHT_GREEN	= 10,
	VGA_COLOR_LIGHT_CYAN	= 11,
	VGA_COLOR_LIGHT_RED	= 12,
	VGA_COLOR_LIGHT_MAGENTA	= 13,
	VGA_COLOR_LIGHT_BROWN	= 14,
	VGA_COLOR_LIGHT_WHITE	= 15
} vga_color_t;

/* 在文本模式下，屏幕显示区域为：高25个字符，宽80个字符 */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void set_vga_memory_addr(uint32_t addr);

void console_clear_screen(void);

void console_write_char_color( const char ch, const vga_color_t fg, const vga_color_t bg );

void console_write_char( const char ch );

void console_move_cursor( const uint8_t row, const uint8_t col );

void console_get_cursor_pos( uint8_t * row, uint8_t * col );

void console_write_string_color( const char * string, const vga_color_t fg, const vga_color_t bg );

void console_write_string( const char * string );
#endif
