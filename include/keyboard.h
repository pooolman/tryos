#ifndef _INCLUDE_KEYBOARD_H_
#define _INCLUDE_KEYBOARD_H_

#include <stdint.h>

/* 按键状态 */
#define FL_E0ESC	(1 << 0)
#define FL_SHIFT_L	(1 << 1)
#define FL_SHIFT_R	(1 << 2)
#define FL_CAPS_LOCK	(1 << 3)

/* 特殊按键值 */
#define KEY_ESC			(0x80 + 0x0)
#define KEY_BACKSPACE		(0x80 + 0x1)
#define KEY_TAB			(0x80 + 0x2)
#define KEY_ENTER		(0x80 + 0x3)
#define KEY_PAD_ENTER		(0x80 + 0x4)
#define KEY_CTRL_L		(0x80 + 0x5)
#define KEY_CTRL_R		(0x80 + 0x6)
#define KEY_SHIFT_L		(0x80 + 0x7)
#define KEY_SHIFT_R		(0x80 + 0x8)
#define KEY_PAD_SLASH		(0x80 + 0x9)
#define KEY_ALT_L		(0x80 + 0xA)
#define KEY_ALT_R		(0x80 + 0xB)
#define KEY_CAPS_LOCK		(0x80 + 0xC)
#define KEY_F1			(0x80 + 0xD)
#define KEY_F2			(0x80 + 0xE)
#define KEY_F3			(0x80 + 0xF)
#define KEY_F4			(0x80 + 0x10)
#define KEY_F5			(0x80 + 0x11)
#define KEY_F6			(0x80 + 0x12)
#define KEY_F7			(0x80 + 0x13)
#define KEY_F8			(0x80 + 0x14)
#define KEY_F9			(0x80 + 0x15)
#define KEY_F10			(0x80 + 0x16)

#define	KEY_NUM_LOCK		(0x80 + 0x17)
#define	KEY_SCROLL_LOCK         (0x80 + 0x18)
#define	KEY_PAD_HOME            (0x80 + 0x19)
#define	KEY_HOME                (0x80 + 0x1A)
#define	KEY_PAD_UP              (0x80 + 0x1B)
#define	KEY_UP                  (0x80 + 0x1C)
#define	KEY_PAD_PAGEUP          (0x80 + 0x1D)
#define	KEY_PAGEUP              (0x80 + 0x1E)
#define	KEY_PAD_MINUS           (0x80 + 0x1F)
#define	KEY_PAD_LEFT            (0x80 + 0x20)
#define	KEY_LEFT                (0x80 + 0x21)
#define	KEY_PAD_MID             (0x80 + 0x22)
#define	KEY_PAD_RIGHT           (0x80 + 0x23)
#define	KEY_RIGHT               (0x80 + 0x24)
#define	KEY_PAD_PLUS            (0x80 + 0x25)
#define	KEY_PAD_END             (0x80 + 0x26)
#define	KEY_PAD_DOWN            (0x80 + 0x27)
#define	KEY_DOWN                (0x80 + 0x28)
#define	KEY_PAD_PAGEDOWN        (0x80 + 0x29)
#define	KEY_PAGEDOWN            (0x80 + 0x2A)
#define	KEY_PAD_INS             (0x80 + 0x2B)
#define	KEY_INSERT              (0x80 + 0x2C)
#define	KEY_PAD_DOT             (0x80 + 0x2D)
#define	KEY_DELETE		(0x80 + 0x2E)
#define	KEY_F11	                (0x80 + 0x2F)
#define	KEY_F12	                (0x80 + 0x30)
#define	KEY_GUI_L               (0x80 + 0x31)
#define KEY_GUI_R               (0x80 + 0x32)
#define KEY_APPS                (0x80 + 0x33)
#define KEY_END			(0x80 + 0x34)

/* 扫描码映射表，在keymap.c中定义 */
extern uint8_t normal_map[128];
extern uint8_t shift_map[128];
extern uint8_t e0_map[128];

void init_kbd(void);
#endif //_INCLUDE_KEYBOARD_H_
