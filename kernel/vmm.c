/*
 * 本文件提供一些基础的虚拟内存管理相关的函数实现
 */
#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "multiboot.h"
#include "parameters.h"
#include "vmm.h"
#include "string.h"
#include "terminal_io.h"
#include "process.h"


/* 可动态分配的内核内存中，每一个空闲页面头部均具有一个这样的结构 */
typedef struct _free_page_t{
	struct _free_page_t * next; //下一个空闲页的地址
} free_page_t;

/* 标志，用于free_page_list_t.flags */
/* 当前链表正在被某个进程使用 */
#define BUSY_LIST	0x1

/* 链表头，用于描述可动态分配的内核内存区域 */
typedef struct {
	uint32_t flags; //该链表的状态
	uint32_t end_addr; //可动态分配的内存区域结束地址
	free_page_t * head; //第一个空闲页地址
} free_page_list_t;

/* 系统维护的链表结构，用于管理可用于动态分配的内存 */
static free_page_list_t free_page_list;

/*
 * 初始化页分配机制；
 */
void init_vmm(void)
{
	/* 定位到从PAGE_UPPER_ALIGN(kernel_end_addr)往上第一个
	 * 非available内存页位置，作为可动态分配的内存区域结束地址 */
	
	for(free_page_list.end_addr = PAGE_UPPER_ALIGN(kernel_end_addr); free_page_list.end_addr < K_P2V(SUPPORT_MEM_SIZE); free_page_list.end_addr += PAGE_SIZE)
	{
		if( ! mem_validate(K_V2P(free_page_list.end_addr)))
			break;
	}

	if(free_page_list.end_addr == PAGE_UPPER_ALIGN(kernel_end_addr))
		PANIC("init_vmm: memory map problem");
	
	/* 清空标志位 */
	free_page_list.flags = 0;
	
	/* 将所有空闲页链接起来 */
	extern void free_page_noint(void * vaddr);
	for(uint32_t vaddr = PAGE_UPPER_ALIGN(kernel_end_addr); vaddr < free_page_list.end_addr; vaddr += PAGE_SIZE)
		free_page_noint((void *)vaddr);
}

/*
 * 无需锁，分配一个清零过的空闲页，返回该页面起始虚拟地址
 * 如果内存不足则返回NULL
 */
void * alloc_page_noint(void)
{
	free_page_t * page;

	if( ! free_page_list.head)
		return NULL;
	page = free_page_list.head;
	free_page_list.head = free_page_list.head->next;
	memset(page, 0, PAGE_SIZE);
	
	return (void *)page;
}

/*
 * 无需锁，回收PAGE_DOWN_ALIGN(vaddr)指定的空闲页
 */
void free_page_noint(void * vaddr)
{
	free_page_t * page = (free_page_t *)PAGE_DOWN_ALIGN(vaddr);
	
	if((uint32_t)page < PAGE_UPPER_ALIGN(kernel_end_addr) || (uint32_t)page >= free_page_list.end_addr)
		PANIC("free_page: invalid vaddr");
	
	/* 由于速度太慢，暂时注释该行 */
	//memset(page, 1, PAGE_SIZE); //写入1便于检测错误
	page->next = free_page_list.head;
	free_page_list.head = page;
}

/*
 * 锁住链表头
 */
static void lock_free_page_list(void)
{
	pushcli();

	while(free_page_list.flags & BUSY_LIST)
		sleep(&free_page_list);
	free_page_list.flags |= BUSY_LIST;

	popcli();
}

/*
 * 解锁链表头
 */
static void unlock_free_page_list(void)
{
	if( ! (free_page_list.flags & BUSY_LIST))
		PANIC("unlock_free_page_list: list is already unlocked");
	free_page_list.flags &= ~BUSY_LIST;
	wakeup(&free_page_list);
}

/*
 * 需要锁，分配一个清零过的空闲页，返回该页的起始虚拟地址
 */
void * alloc_page(void)
{
	void * vaddr;

	lock_free_page_list();
	vaddr = alloc_page_noint();
	unlock_free_page_list();

	return vaddr;
}

/*
 * 需要锁，回收vaddr指定的空闲页
 */
void free_page(void * vaddr)
{
	lock_free_page_list();
	free_page_noint(vaddr);
	unlock_free_page_list();
}


/*
 * 刷新整个TLB中除具有全局属性的其他条目。
 */
void flush_TLB(void)
{
	uint32_t cr3;
	asm volatile ("mov %%cr3, %0"
			: "=r" (cr3)
			:);
	asm volatile ("mov %0, %%cr3"
			:
			: "r" (cr3));
}


/*
 * 加载新的值到CR3寄存器中，这将更改页目录/更改PCD和PWT属性，
 * 并刷新整个TLB中除具有全局属性的其他条目。
 */
void lcr3(uint32_t cr3)
{
	asm volatile ( "mov %0, %%cr3"
			:
			: "r" (cr3));
}

/*
 * 读取CR3寄存器值
 */
uint32_t rcr3(void)
{
	uint32_t cr3;
	asm volatile ( "mov %%cr3, %0"
			: "=r" (cr3)
			:);
	return cr3;
}

/* DEBUG */
void dump_free_page_list(void)
{
	printk("dump_free_page_list: flags %u end_addr %X head %X\n", free_page_list.flags, free_page_list.end_addr, free_page_list.head);
}
