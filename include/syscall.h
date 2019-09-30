#ifndef _INCLUDE_SYSCALL_H_
#define _INCLUDE_SYSCALL_H_

#include <stdint.h>

/* 现有的系统调用号 */
#define SYS_NUM_debug	0
#define SYS_NUM_exec	1
#define SYS_NUM_fork	2
#define SYS_NUM_exit	3
#define SYS_NUM_wait	4
#define SYS_NUM_getpid	5
#define SYS_NUM_dup	6
#define SYS_NUM_read	7
#define SYS_NUM_write	8
#define SYS_NUM_open	9
#define SYS_NUM_close	10
#define SYS_NUM_fstat	11
#define SYS_NUM_link	12
#define SYS_NUM_unlink	13
#define SYS_NUM_mkdir	14
#define SYS_NUM_mknod	15
#define SYS_NUM_chdir	16
#define SYS_NUM_pipe	17

void syscall(void);

int32_t within_nstack(uint32_t addr);

int32_t within_stack(uint32_t addr);

int32_t fetch_int(uint32_t addr, uint32_t * ap);

int32_t check_str(uint32_t addr);

int32_t get_int_arg(uint32_t n, uint32_t * ap);

int32_t get_ptr_arg(uint32_t n, uint32_t * pp, uint32_t size);

int32_t get_str_arg(uint32_t n, uint32_t * pp);
#endif //_INCLUDE_SYSCALL_H_
