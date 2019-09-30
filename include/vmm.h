#ifndef _INCLUDE_VMM_H_
#define _INCLUDE_VMM_H_

#include <stdint.h>

/* 支持4KB页框 */
#define PAGE_FRAME_SIZE (4*1024)

/* 按页/页框大小向下对齐 */
#define PAGE_DOWN_ALIGN(x) ((uint32_t)(x) & 0xFFFFF000)

/* 按页/页框大小向上对齐 */
#define PAGE_UPPER_ALIGN(x) ((uint32_t)(x) == PAGE_DOWN_ALIGN(x) ? (uint32_t)(x) : PAGE_DOWN_ALIGN(x) + PAGE_FRAME_SIZE)

/* 按中间页表所映射内存大小向下对齐 */
#define PT_DOWN_ALIGN(x) ((uint32_t)(x) & 0xFFC00000)

/* 物理地址中写入页表项或页目录项中的部分(Physical Frame Number) */
#define PFN(x) (((uint32_t)(x)) & 0xFFFFF000)

/* 开启分页后，从0MB到内核结束位置被映射到虚拟地址0xC0000000以上的位置；
 * 该值在linker.ld中也有定义。 */
#define KERNEL_VIRTUAL_ADDR_OFFSET 0xC0000000


/* 页目录项类型(page directory entry type) */
typedef uint32_t pde_t;
/* 页表项类型 (page table entry type) */
typedef uint32_t pte_t;

/* 定义页面大小/页目录条目个数/页表条目个数 */
#define PAGE_SIZE		PAGE_FRAME_SIZE
#define PD_ENTRY_COUNT		(PAGE_SIZE/sizeof(pde_t))
#define PT_ENTRY_COUNT		(PAGE_SIZE/sizeof(pte_t))


/* 定义与页目录项/页表项相关的属性，具体含义参考intel手册*/
/* 所有其他属性当前默认为 0 */
#define PDE_PRESENT	0x1
#define PTE_PRESENT	0x1

#define PDE_RW		0x2
#define PTE_RW		0x2

#define PDE_USER	0x4
#define PTE_USER	0x4


/* 定义用于获取虚拟地址的页目录索引/页表索引/页内偏移量的宏*/
#define PD_INDEX(x)	((uint32_t)(x) >> 22)
#define PT_INDEX(x)	(((uint32_t)(x) >> 12) & 0x3FF)
#define PG_OFFSET(x)	((uint32_t)(x) & 0xFFF)

/* 在虚拟地址（线性地址）和物理地址之间转换，虚拟地址限于KERNEL_VIRTUAL_ADDR_OFFSET ~ KERNEL_VIRTUAL_ADDR_OFFSET + SUPPORT_MEM_SIZE，物理地址限于 0～SUPPORT_MEM_SIZE */
#define K_P2V(x)	((uint32_t)(x) + (uint32_t)KERNEL_VIRTUAL_ADDR_OFFSET)
#define K_V2P(x)	((uint32_t)(x) - (uint32_t)KERNEL_VIRTUAL_ADDR_OFFSET)

void init_vmm(void);

void * alloc_page_noint(void);

void free_page_noint(void * vaddr);

void * alloc_page(void);

void free_page(void * vaddr);

void flush_TLB(void);

void lcr3(uint32_t cr3);
uint32_t rcr3(void);

/* DEBUG */
void dump_free_page_list(void);

#endif //_INCLUDE_VMM_H_
