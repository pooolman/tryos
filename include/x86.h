/*
 * 本文件定义一些与x86架构有关的常量以及一些便于使用特殊x86指令的函数。
 * 注意：
 * 这些函数使用了gnu拓展（asm关键字），所以编译时应使用-std=gnu99
 */
#ifndef _INCLUDE_X86_H_
#define _INCLUDE_X86_H_

#include <stdint.h>

//Eflags register
#define FL_RESERVED	0x00000002 //reserved bit
#define FL_CF           0x00000001      // Carry Flag
#define FL_PF           0x00000004      // Parity Flag
#define FL_AF           0x00000010      // Auxiliary carry Flag
#define FL_ZF           0x00000040      // Zero Flag
#define FL_SF           0x00000080      // Sign Flag
#define FL_TF           0x00000100      // Trap Flag
#define FL_IF           0x00000200      // Interrupt Enable
#define FL_DF           0x00000400      // Direction Flag
#define FL_OF           0x00000800      // Overflow Flag
#define FL_IOPL_MASK    0x00003000      // I/O Privilege Level bitmask
#define FL_IOPL_0       0x00000000      //   IOPL == 0
#define FL_IOPL_1       0x00001000      //   IOPL == 1
#define FL_IOPL_2       0x00002000      //   IOPL == 2
#define FL_IOPL_3       0x00003000      //   IOPL == 3
#define FL_NT           0x00004000      // Nested Task
#define FL_RF           0x00010000      // Resume Flag
#define FL_VM           0x00020000      // Virtual 8086 mode
#define FL_AC           0x00040000      // Alignment Check
#define FL_VIF          0x00080000      // Virtual Interrupt Flag
#define FL_VIP          0x00100000      // Virtual Interrupt Pending
#define FL_ID 0x00200000 // ID flag

static inline uint32_t read_eflags(void)
{
	uint32_t eflags;
	asm volatile ( "pushfl; popl %0"
			: "=r" (eflags)
			:);
	return eflags;
}

static inline void cli(void)
{
	asm volatile ( "cli"
			:
			:);
}

static inline void sti(void)
{
	asm volatile ( "sti"
			:
			:);
}

/*
 * insl and outsl are copied and modified from xv6-v7/x86.h.
 * insl从port中依次读取count个uint32_t值并依次写入到addr中；outsl则与之相反。
 * 注意：与port关联的设备可能没法跟上指令执行的速度！
 */
static inline void insl(uint32_t port, void * addr, uint32_t count)
{
	asm volatile ( "cld; rep insl"
			: "=D" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count)
			: "memory", "cc" );
}

static inline void outsl(uint32_t port, void * addr, uint32_t count)
{
	asm volatile ( "cld; rep outsl"
			: "=S" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count)
			: "cc" );
}

/*
 * 从8位端口读取数据
 */
static inline uint8_t inb( uint16_t port)
{
	uint8_t data;
	asm volatile ( "inb %1, %0"
			: "=a" (data)
			: "d" (port) );
	return data;
}

/*
 * 向8位端口写入数据
 */
static inline void outb( uint16_t port, uint8_t data )
{
	asm volatile ( "outb %0, %1"
			:
			: "a" (data), "d" (port) );
}

/*
 * 向16位端口读取数据
 */
static inline uint16_t inw( uint16_t port )
{
	uint16_t data;
	asm volatile ( "inw %1, %0"
			: "=a" (data)
			: "d" (port) );
	return data;
}

/*
 * 向16位端口写入数据
 */
static inline void outw( uint16_t port, uint16_t data )
{
	asm volatile ( "outw %0, %1"
			:
			: "a" (data), "d" (port) );
}


#endif //_INCLUDE_X86_H_
