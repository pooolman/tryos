/*
 * 本文件提供内核使用的一些常量
 */
#ifndef _INCLUDE_PARAMETERS_H_
#define _INCLUDE_PARAMETERS_H_

#include <stdint.h>
#include "vmm.h"

/* 32MB，进入内核后，需要映射0x0 ~ KERNEL_MAP_END_ADDR */
#define KERNEL_MAP_END_ADDR	0x2000000
/* 目前所支持物理内存大小，64MB */
#define SUPPORT_MEM_SIZE	0x4000000
/* 内核页表个数 */
#define KPT_COUNT		((SUPPORT_MEM_SIZE) / 0x400000)


/* kernel_start_addr/kernel_end_addr 在linker.ld中定义，声明为kernel所占用的虚拟地址区间
 * ，由于ld中定义symbol的特性，所以在C语言中使用如下形式利用这些符号
 * ，参考ld手册
 */
extern uint8_t kernel_start_addr[], kernel_end_addr[];


/* kernel heap的起始位置，按页对齐 */
#define KHEAP_START_ADDR ((uint32_t)kernel_end_addr % PAGE_SIZE ? \
		PAGE_DOWN_ALIGN((uint32_t)kernel_end_addr + PAGE_SIZE) : \
		(uint32_t)kernel_end_addr)
/* kernel heap大小 (字节单位)，整数个页*/
#define KHEAP_SIZE	(KERNEL_VIRTUAL_ADDR_OFFSET + KERNEL_MAP_END_ADDR - KHEAP_START_ADDR)


/* 进程用户栈，布局参见文档 */
#define PROC_USER_STACK_SIZE	PAGE_SIZE
#define PROC_USER_STACK_ADDR	(KERNEL_VIRTUAL_ADDR_OFFSET - PROC_USER_STACK_SIZE)
#define PROC_USER_STACK_GUARD	(KERNEL_VIRTUAL_ADDR_OFFSET - PROC_USER_STACK_SIZE - PAGE_SIZE)

/* 进程被加载的起始线性地址，第一个进程也是如此，在linker.ld中也有定义 */
#define PROC_LOAD_ADDR	0x1000

/* 进程内核栈 */
#define PROC_KERNEL_STACK_SIZE	PAGE_SIZE

/* 块缓冲中块的数量 */
#define BUF_COUNT	20

/* inode cache中inode结构个数(保持活动的个数) */
#define CACHE_INODE_NUM	50

/* 内核维护的最多的打开文件结构数量 */
#define OPEN_FILE_NUM	64

/* 每个进程同时使用的最多描述符个数 */
#define PROC_OPEN_FD_NUM	16

/* 用户程序执行时所能接受的最大参数个数 */
#define MAX_ARG_NUM	32

/* tty对应的主设备号 */
#define TTY_MAJOR_NUM	0

/* tty对应的从设备号 */
#define TTY_MINOR_NUM	0

/* tty输入缓冲区大小，字节单位，应设为2的次幂 */
#define TTY_INPUT_BUF_SIZE	128

/* 最大支持字符设备个数 */
#define CHR_DEV_COUNT	1

/* 管道长度，目前将整个管道结构存放在一个页面中，所以结构大小不应超过4KB */
#define PIPE_BUF_SIZE 1024

#endif //_INCLUDE_PARAMETERS_H_
