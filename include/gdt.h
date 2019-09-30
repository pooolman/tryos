#ifndef _INCLUDE_GDT_H_
#define _INCLUDE_GDT_H_

#include <stdint.h>

#define GDT_ENTRY_COUNT		6 //GDT条目个数
#define SEG_INDEX_NULL		0 //空描述符在GDT中的索引
#define SEG_INDEX_KCODE		1 //内核模式代码段描述符在GDT中的索引
#define SEG_INDEX_KDATA		2 //内核模式数据段描述符在GDT中的索引
#define SEG_INDEX_UCODE		3 //用户模式代码段描述符在GDT中的索引
#define SEG_INDEX_UDATA		4 //用户模式数据段描述符在GDT中的索引
#define SEG_INDEX_TSS		5 //TSS在GDT中的索引


#define TI_GDT			0
#define TI_LDT			1
#define RPL_KERNEL		0
#define RPL_USER		3

/* 构建selector， a为GDT索引，b为TI位，c为RPL */
#define CONSTRUCT_SELECTOR(a, b, c) (((uint16_t)(a) << 3) | (((uint8_t)(b) << 2) & 0x4) | ((uint8_t)(c) & 0x3))


typedef struct gdt_entry_struct{
	uint16_t limit_low; //15-0位的段界限
	uint16_t base_addr_low; //15-0位的段基址
	uint8_t base_addr_middle; //23-16位的段基址
	uint16_t attributes; //段描述符的属性以及19-16位的段界限
	uint8_t base_addr_high; //31-24位的段基址
}__attribute__((packed)) gdt_entry_t; //gdt中的段描述符类型


typedef struct gdt_ptr_struct{
	uint16_t limit; //gdt的界限
	uint32_t base; //gdt的线性基地址
}__attribute__((packed)) gdt_ptr_t; //用于写入gdtr寄存器



void init_gdt(void);

#endif  //_INCLUDE_GDT_H_
