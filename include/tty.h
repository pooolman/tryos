#ifndef _INCLUDE_TTY_H_
#define _INCLUDE_TTY_H_

#include <stdint.h>

uint8_t map_sc(uint8_t sc);

void tty_handler(uint8_t sc);

int32_t tty_read(void * dst, uint32_t n);

int32_t tty_write(void * src, uint32_t n);

void init_tty(void);


#endif //_INCLUDE_TTY_H_
