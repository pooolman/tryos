#ifndef _INCLUDE_STRING_H_
#define _INCLUDE_STRING_H_

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

int vsprintf(char * buff, const char * format, va_list ap);


void * memset(void * buffer, int ch, size_t count);
void * memcpy(void * dst, const void * src, size_t count);
void * memmove(void * dst, const void * src, size_t count);
size_t strlen(const char * str);
char * strncpy(char * dst, const char * src, size_t count);
char * sstrncpy(char * dst, const char * src, size_t count);
int32_t strncmp(const char * s1, const char * s2, size_t n);
char * strncat(char * dst, const char * src, size_t n);
char * strchr(const char * s, char c);

#endif //_INCLUDE_STRING_H_
