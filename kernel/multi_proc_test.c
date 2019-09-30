/*
 * 本文件提供一些多进程测试函数
 */
#include "debug.h"
#include "parameters.h"
#include "string.h"
#include "process.h"
#include "gdt.h"
#include "idt.h"
#include "x86.h"
#include "vmm.h"
#include "vm_tools.h"
#include "terminal_io.h"
#include "path.h"


extern cpu_t cpu; //与CPU关联的数据结构
extern pcb_t pcb_table[PROC_MAX_COUNT]; //pcb表

/*
 * 创建测试进程
 * 注意：该函数必须在init_first_proc函数之后调用
 * entry_point: 进程入口点
 * proc_name: 进程名
 */
void create_test_proc(void * entry_point, const char * proc_name)
{
	pcb_t * proc;

	/* 分配PCB并完成一部分信息的初始化 */
	if((proc = alloc_pcb()) == NULL)
		PANIC("create_test_proc: alloc pcb failed");
	
	/* 建立地址空间 */
	proc->pgdir = create_init_kvm();
	/* init_proc_size/init_proc_start在linker.ld中定义，表示测试进程在物理内存中的位置 */
	extern uint8_t init_proc_start[], init_proc_size[];
	init_first_proc_uvm(proc->pgdir, (uint32_t)init_proc_start, (uint32_t)init_proc_size);

	/* 所有trapframe成员清零 */
	memset(proc->tf, 0, sizeof(trapframe_t));

	/* 设置trapframe.ss/esp/eflags/cs/eip成员 */
	proc->tf->ss = CONSTRUCT_SELECTOR(SEG_INDEX_UDATA, TI_GDT, RPL_USER);
	proc->tf->esp = PROC_USER_STACK_ADDR + PROC_USER_STACK_SIZE;
	proc->tf->eflags = FL_RESERVED | FL_IF | FL_IOPL_0;	//打开中断，IOPL为0，即没有IO端口操作权限
	proc->tf->cs = CONSTRUCT_SELECTOR(SEG_INDEX_UCODE, TI_GDT, RPL_USER);
	proc->tf->eip = (uint32_t)entry_point; //设置从entry_point开始执行
	
	/* 设置trapframe.ds/es/fs/gs成员 */
	proc->tf->ds = CONSTRUCT_SELECTOR(SEG_INDEX_UDATA, TI_GDT, RPL_USER);
	proc->tf->es = proc->tf->ds;
	proc->tf->fs = proc->tf->ds;
	proc->tf->gs = proc->tf->ds;

	/* 设置pcb.name成员 */
	strncpy(proc->name, proc_name, PROC_NAME_LENGTH);

	/* 现在可以运行了 */
	proc->state = PROC_STATE_RUNNABLE;
}

/*
 * 分配PCB函数，只在create_kernel_thread_test中使用。
 */
pcb_t * alloc_pcb_test(void * entry_point)
{
	pcb_t * new_pcb;
	uint32_t i;
	
	/* 遍历pcb表，找到一个可用的pcb，没有则返回NULL */
	for(i = 0; i < sizeof(pcb_table)/sizeof(pcb_t); i++)
	{
		if(pcb_table[i].state == PROC_STATE_UNUSED)
		{
			new_pcb = &pcb_table[i];
			new_pcb->pid = i; //此pid是最小可用pid
			break;
		}
	}
	if(i == sizeof(pcb_table)/sizeof(pcb_t))
		return NULL;
	

	/* 设置当前状态为NEWBORN */
	new_pcb->state = PROC_STATE_NEWBORN;
	
	/* 设置eflags_stack/eflags_sp */
	memset(new_pcb->eflags_stack, 0, sizeof(new_pcb->eflags_stack));
	new_pcb->eflags_sp = PROC_PUSHCLI_DEPTH;

	/* 设置等待队列号 */
	new_pcb->channel = NULL;
	
	/* 分配内核栈 */
	if((new_pcb->kstack = alloc_page_noint()) == NULL)
		PANIC("alloc_pcb_test: alloc kernel stack failed");


	/* 设置内核栈内容，使esp指向内核栈顶位置 */
	uint32_t esp = (uint32_t)(new_pcb->kstack) + PROC_KERNEL_STACK_SIZE;

	/* 为trapframe留出空间，使新进程的pcb.tf指向trapframe起始位置 */
	esp -= sizeof(trapframe_t);
	new_pcb->tf = (trapframe_t *)esp;

	/* 在内核栈中写入trapret的入口地址 */
	esp -= 4;
	extern void trapret(void);
	*((uint32_t *)esp) = (uint32_t)trapret;

	/* 在内核栈中构建context结构，使新进程的pcb.context指向context起始位置 */
	esp -= sizeof(context_t);
	new_pcb->context = (context_t *)esp;

	/* 先清空所有成员，然后使其eip指向entry_point，设置eflags */
	memset(new_pcb->context, 0, sizeof(context_t));
	new_pcb->context->eip = (uint32_t)entry_point;
	new_pcb->context->eflags = FL_RESERVED | FL_IF | FL_IOPL_0; //打开中断，以便切换
	
	return  new_pcb;
}


/*
 * 创建内核线程。仅用于创建测试线程。
 */
void create_kernel_thread_test(const char * proc_name, void * entry_point)
{
	pcb_t * test_thread;

	/* 分配PCB，并初始化一部分PCB成员 */
	if((test_thread = alloc_pcb_test(entry_point)) == NULL)
		PANIC("create_kernel_thread_test: alloc pcb failed");
	
	/* 新建一个页目录，具有内核地址空间 */
	test_thread->pgdir = create_init_kvm_noint();
	
	/* 清零所有trapframe成员 */
	memset(test_thread->tf, 0, sizeof(*(test_thread->tf)));
	
	/* 设置线程name */
	strncpy(test_thread->name, proc_name, PROC_NAME_LENGTH);

	/* 设置当前工作目录为根目录 */
	/* 由于pushcli的原因，暂时无法在kernel_init2中设置cwd */
	//test_thread->cwd = resolve_path("/", 0, NULL);
	
	/* 清空打开文件指针列表 */
	memset(test_thread->open_files, 0, sizeof(test_thread->open_files));
	
	/* 此时已经RUNNALBE了 */
	test_thread->state = PROC_STATE_RUNNABLE;

}
