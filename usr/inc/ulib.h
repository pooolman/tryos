#ifndef _INCLUDE_ULIB_H_
#define _INCLUDE_ULIB_H_

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#define FD_STDIN	0
#define FD_STDOUT	1
#define FD_STDERR	2

int32_t udebug(const char * format, ...);
int32_t printf(const char * format, ...);
int32_t gets(int32_t fd, void * buf, int32_t n);
int32_t getc(int32_t fd);
#endif //_INCLUDE_ULIB_H_
