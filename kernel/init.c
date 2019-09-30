#include <stddef.h>
#include <stdint.h>

#include "parameters.h"
#include "debug.h"
#include "console.h"
#include "terminal_io.h"
#include "gdt.h"
#include "idt.h"
#include "8259A.h"
#include "8253pit.h"
#include "multiboot.h"
#include "vmm.h"
#include "vm_tools.h"
#include "process.h"
#include "ide.h"
#include "buf_cache.h"
#include "block.h"
#include "inode.h"
#include "path.h"
#include "file.h"
#include "fcntl.h"
#include "keyboard.h"
#include "tty.h"

 

/* Check if the compiler thinks we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

/* pointer to Multiboot information struct which is passed by boot.asm */
multiboot_info_t * glb_mbi;
 

/* 在init.data段中声明内核页目录/相关页表 */
/* 用于映射 0MB ~ KERNEL_MAP_END_ADDR */
__attribute__((section(".init.data"), aligned (PAGE_SIZE))) pde_t _kpd[PD_ENTRY_COUNT];
__attribute__((section(".init.data"), aligned (PAGE_SIZE))) pte_t _kpt[KPT_COUNT][PT_ENTRY_COUNT];


extern void kernel_init2(void);
/*
 * 本函数相当于boot1部分，用于建立内核的分页结构，开启分页模式，再转入boot2(kernel_init2)；
 * 因为在进入boot2部分(kernel_init2)后，会丢弃低端位置的映射，所以需要修改栈（esp/ebp）的值/
 * Multiboot information结构指针以及显存访问地址的值 */
__attribute__((section(".init.text"))) void kernel_init(multiboot_info_t * mbi) 
{
	/* 设置页目录 */
	/* 首先清空所有页目录项 */
	for(uint32_t pde_index = 0; pde_index < PD_ENTRY_COUNT; pde_index++)
		_kpd[pde_index] = (pde_t)0;

	for(uint32_t vaddr = 0x0; PD_INDEX(vaddr) < KPT_COUNT; vaddr += PAGE_SIZE * 1024)
	{
		//PDE初始设置为用户特权级的
		_kpd[PD_INDEX(vaddr)] = (pde_t)((uint32_t)_kpt[PD_INDEX(vaddr)] | PDE_PRESENT | PDE_RW | PDE_USER);
		_kpd[PD_INDEX(vaddr) + PD_INDEX(KERNEL_VIRTUAL_ADDR_OFFSET)] = (pde_t)((uint32_t)_kpt[PD_INDEX(vaddr)] | PDE_PRESENT | PDE_RW | PDE_USER);
	}



	/* 设置页表 */
	/* 首先清空所有页表项 */
	pte_t * pte = _kpt[0];
	for(; pte < _kpt[KPT_COUNT]; pte++)
		*pte = (pte_t)0;

	pte = _kpt[0];
	for(uint32_t i = 0; pte < _kpt[KPT_COUNT]; i++, pte++)
		//PTE初始设置为系统特权级的
		*pte = (pte_t)(i << 12 | PTE_PRESENT | PTE_RW);

	
	/* 设置cr3 */
	asm volatile ("mov %0, %%cr3"
			:
			: "r" (_kpd));
	/* 开启分页*/
	uint32_t cr0;
	asm volatile ("mov %%cr0, %0"
			: "=r" (cr0)
			:);
	cr0 |= 0x80000000; //打开PG位
	asm volatile ("mov %0, %%cr0"
			:
			: "r" (cr0));
	/* 设置Multiboot information结构体指针的值 */
	glb_mbi = (multiboot_info_t *)((uint32_t)mbi + KERNEL_VIRTUAL_ADDR_OFFSET);
	/* 修改esp/ebp的值 */
	uint32_t esp, ebp;
	asm volatile ("mov %%esp, %0"
			: "=r" (esp)
			:);
	asm volatile ("mov %%ebp, %0"
			: "=r" (ebp)
			:);
	esp += KERNEL_VIRTUAL_ADDR_OFFSET;
	ebp += KERNEL_VIRTUAL_ADDR_OFFSET;
	asm volatile ("mov %0, %%esp"
			:
			: "r" (esp));
	asm volatile ("mov %0, %%ebp"
			:
			: "r" (ebp));
	
	/* 设置vga buffer的起始位置 */
	set_vga_memory_addr(0xB8000 + KERNEL_VIRTUAL_ADDR_OFFSET);

	/* 进入boot2部分的初始化 */
	kernel_init2();
}

extern void timer_callback(trapframe_t * info);
extern void page_fault_handler(trapframe_t * info);

void kernel_init2()
{
	/* 先设置scheduler的页表 */
	cpu.pgdir = (pde_t *)(K_P2V(_kpd));
	/* 当前没有进程运行 */
	cpu.cur_proc = NULL;

	/* 撤销掉第一类页表 */
	/* 注意：这里使用_kpd，而_kpd只有在第一类页表没有被撤销时才有效，需要进一步检查 */
	for(uint32_t vaddr = 0x0; PD_INDEX(vaddr) < KPT_COUNT; vaddr += PAGE_SIZE * 1024)
		_kpd[PD_INDEX(vaddr)] = (pde_t)(0x0);
	flush_TLB();


	/* 从现在开始，只使用物理地址0x0 ~ SUPPORT_MEM_SIZE与虚拟地址
	 * KERNEL_VIRTUAL_ADDR_OFFSET ~ (KERNEL_VIRTUAL_ADDR_OFFSET + SUPPORT_MEM_SIZE)
	 * 之间的映射了 */
	console_clear_screen();
	printk("Hello, Tryos. Currently we are in paging mode\n");

	init_gdt();
	init_idt();
	init_8259A();
	init_8253pit(50);
	init_vmm();
	init_ide();
	init_buf_cache();
	init_kbd();
	init_tty();

	
	register_interrupt_handler(PF_INT_VECTOR, page_fault_handler);
	register_interrupt_handler(IRQ0_INT_VECTOR, timer_callback);

	printk("Memory map information provided by GRUB:\n");
	show_memory_map();
	printk("Kernel start address: %X\n", kernel_start_addr);
	printk("Kernel end address: %X\n", kernel_end_addr);
	printk("Kernel size: %X\n", kernel_end_addr - kernel_start_addr);
	printk("----------------------------\n");

	/* 初步测试多进程之间的切换 */
	extern uint8_t init_proc_start[];
	extern uint8_t init_proc_size[];
	printk("init_proc_start: %X init_proc_size: %X\n", init_proc_start, init_proc_size);

	/* 设置TSS */
	/*
	memset(&cpu.TSS, 0, sizeof(cpu.TSS));
	cpu.TSS.ss0 = CONSTRUCT_SELECTOR(SEG_INDEX_KDATA, TI_GDT, RPL_KERNEL);
	ltr(CONSTRUCT_SELECTOR(SEG_INDEX_TSS, TI_GDT, RPL_KERNEL));
	*/
	

	/* 创建测试内核线程 */
	/*
	extern void create_kernel_thread_test(const char * proc_name, void * entry_point);
	extern void proc_AA(void);
	extern void proc_BB(void);
	extern void proc_CC(void);
	create_kernel_thread_test("proc_AA", proc_AA);
	//create_kernel_thread_test("proc_BB", proc_BB);
	//create_kernel_thread_test("proc_CC", proc_CC);

	printk("proc_AA: %X proc_BB: %X proc_CC: %X\n", proc_AA, proc_BB, proc_CC);
	*/

	create_first_proc();

	scheduler();

	PANIC("kernel_init2: unknown error");
	//while(1); //不返回到kernel_init中了
}

uint32_t delay(uint32_t count)
{
	uint32_t a;
	for(uint32_t i = 0; i < count; i++)
	{
		for(uint32_t j = 0; j < count; j++)
		{
			for(uint32_t k = 0; k < count; k++)
				a = k + i + j;
		}
	}

	return a;
}

/* 测试用的内核线程执行体 */
void proc_AA(void)
{
	pde_t * pgdir;
	void * start_addr = (void *)0x1500;
	void * end_addr = (void *)0x8000;

	pgdir = create_init_kvm();
	iprint_log("proc_AA: has created a new pgdir %X\n", (uint32_t)pgdir);
	
	if(alloc_uvm(pgdir, start_addr, end_addr) != end_addr)
		PANIC("proc_AA: alloc uvm failed");
	iprint_log("proc_AA: has alloc uvm from %X to %X\n", (uint32_t)start_addr, (uint32_t)end_addr);

	char buf[64];
	memset(buf, 0x9, sizeof(buf));
	for(int32_t i = 0; i < 12; i++)
		if(copyout(pgdir, (void *)(0x2e00 + i * sizeof(buf)), buf, sizeof(buf)) == -1)
			PANIC("proc_AA: copyout failed");
	
	iprint_log("proc_AA: write to pgdir success\n");
	
	
	while(1);
	asm volatile ("cli; hlt"
			:
			:);
}
void proc_BB(void)
{
	void * end_addr = (void *)(PROC_LOAD_ADDR + 0x200);
	void * start_addr = (void *)PROC_LOAD_ADDR;
	
	iprint_log("proc_BB: pgdir %X\n", (uint32_t)cpu.cur_proc->pgdir);
	iprint_log("proc_BB: alloc uvm from %X to %X\n", (uint32_t)start_addr, (uint32_t)end_addr);
	if(alloc_uvm(cpu.cur_proc->pgdir, start_addr, end_addr) != end_addr)
		PANIC("proc_BB: aloc uvm failed");
	iprint_log("proc_BB: alloc uvm success\n");

	while(1);
	asm volatile ("cli; hlt"
			:
			:);
}
void proc_CC(void)
{

	while(1);
	asm volatile ("cli; hlt"
			:
			:);
}

void timer_callback(trapframe_t * info)
{
	static uint32_t ticks = 0;
	//printk("timer_callback: tick %u\n", ticks);
	ticks++;

	/* DEBUG */
	extern uint32_t delay(uint32_t);
	delay(0x10);
	//printk(">>>timer_callback: [%u]%s kernel thread runing\n", cpu.cur_proc->pid, cpu.cur_proc->name);
	if(cpu.cur_proc)
	{
		cpu.cur_proc->state = PROC_STATE_RUNNABLE;
		swtch(&(cpu.cur_proc->context), cpu.scheduler_context);
	}
	//printk("<<<timer_callback: [%u]%s kernel thread continue runing\n", cpu.cur_proc->pid, cpu.cur_proc->name);
	//while(1);
}
