/*
 * 本文件提供用于debug的工具函数原型/一些简单的宏
 */

#ifndef _INCLUDE_DEBUG_H_
#define _INCLUDE_DEBUG_H_

#include <stdint.h>


void panic_assert(const char * file_name, uint32_t line_number, const char * expression);
void panic(const char * file_name, uint32_t line_number, const char * msg);
void print_log(const char * format, ...);
void iprint_log(const char * format, ...);

/* exp 应该是一个表达式 */
#define ASSERT(exp) ({ \
		if(!(exp)) panic_assert(__FILE__, __LINE__, #exp);})

/* msg 应该是一个字符串指针 */
#define PANIC(msg) panic(__FILE__, __LINE__, (msg))

#endif //_INCLUDE_DEBUG_H_
