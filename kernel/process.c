/*
 * 本文件提供与process相关的函数实现，定义与process管理相关的变量
 */
#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "parameters.h"
#include "string.h"
#include "process.h"
#include "gdt.h"
#include "idt.h"
#include "x86.h"
#include "vmm.h"
#include "terminal_io.h"
#include "vm_tools.h"
#include "inode.h"
#include "path.h"
#include "file.h"


cpu_t cpu; //与CPU关联的数据结构

pcb_t pcb_table[PROC_MAX_COUNT]; //pcb表

pcb_t * uinit_proc; //第一个可调度的进程

extern void dump_proc(pcb_t * proc);
extern void forkret(void);

/*
 * 加载tss选择子
 */
void ltr(uint16_t tss)
{
	asm volatile ( "ltr %0"
			:
			: "r" (tss));
}

/*
 * pushcli/popcli类似于cli/sti，pushcli保存当前eflags到当前进程中，然后关闭中断，
 * popcli则恢复上次调用pushcli之前的eflags值。
 * pushcli/popcli应该成对使用；且scheduler线程不应该使用这些函数。
 * pushcli/popcli函数执行的操作是原子的；同一时间只可能有一个进程执行成功。
 */
void pushcli(void)
{
	uint32_t eflags = read_eflags();
	cli();
	if(cpu.cur_proc == NULL)
		PANIC("pushcli: no running process");
	if(cpu.cur_proc->eflags_sp == 0 || cpu.cur_proc->eflags_sp > PROC_PUSHCLI_DEPTH)
		PANIC("pushcli: eflags_sp error");
	cpu.cur_proc->eflags_sp--;
	cpu.cur_proc->eflags_stack[cpu.cur_proc->eflags_sp] = eflags;
}

void popcli(void)
{
	if((read_eflags() & (FL_IF | FL_RESERVED)) != FL_RESERVED)
		PANIC("popcli: interruptable");
	cpu.cur_proc->eflags_sp++;
	if(cpu.cur_proc->eflags_sp > PROC_PUSHCLI_DEPTH)
		PANIC("popcli: eflags_sp error");
	if((cpu.cur_proc->eflags_stack[cpu.cur_proc->eflags_sp - 1] & FL_IF) == FL_IF)
		sti();
}


/*
 * 休眠和唤醒函数。
 * 进程调用sleep后，将会把CPU的控制权交给scheduler。
 * 调用wakeup后，休眠在channel上的进程就是RUNNABLE的。
 * sleep/wakeup要求调用进程是可调度的，所以不能在scheduler中使用；也因此不能在外部硬件中断处理程序中使用，因为有可能中断发生在scheduler执行过程中，而scheduler不具有pcb结构，不可被调度！
 * 外部硬件中断处理程序唤醒进程时，应该使用wakeup_noint，这样，即使中断发生在scheduler中也是可以的。
 * 
 */
void sleep(void * channel)
{
	pushcli();//保存原来的中断状态，并关中断

	/* DEBUG */
	print_log("%s[%u] sleep at %X\n", cpu.cur_proc->name, cpu.cur_proc->pid, channel);
	
	if(cpu.cur_proc == NULL)
		PANIC("sleep: no process running");
	
	/* 在channel上休眠 */
	cpu.cur_proc->channel = channel;
	cpu.cur_proc->state = PROC_STATE_SLEEPING;

	/* DEBUG */
	//printk("sleep: ");
	//dump_proc(cpu.cur_proc);
	
	/* 切换到scheduler */
	swtch(&cpu.cur_proc->context, cpu.scheduler_context);

	/* 被唤醒后 */
	cpu.cur_proc->channel = NULL;
	
	/* 恢复原来的中断状态 */
	popcli();
}

void wakeup_noint(void * channel)
{
	for(pcb_t * proc = &pcb_table[0]; proc < &pcb_table[PROC_MAX_COUNT]; proc++)
	{
		if(proc->state == PROC_STATE_SLEEPING && proc->channel == channel)
		{
			proc->state = PROC_STATE_RUNNABLE;
			
			/* DEBUG */
			//printk("wakeup: ");
			//dump_proc(cpu.cur_proc);
		}
	}
}

void wakeup(void * channel)
{
	pushcli(); //保存原来的中断状态，并关中断
	wakeup_noint(channel);
	popcli(); //恢复原来的中断状态
}


/* 在pcb_table中分配PCB，并初始化其中一部分信息。
 * 成功返回PCB的虚拟地址，失败则返回NULL。
 */
pcb_t * alloc_pcb(void)
{
	pcb_t * new_pcb;
	uint32_t i;
	
	/* 遍历pcb表，找到一个可用的pcb，没有则返回NULL */
	pushcli();
	for(i = 0; i < sizeof(pcb_table)/sizeof(pcb_t); i++)
	{
		if(pcb_table[i].state == PROC_STATE_UNUSED)
		{
			pcb_table[i].state = PROC_STATE_NEWBORN;
			popcli();

			new_pcb = &pcb_table[i];
			new_pcb->pid = i; //此pid是最小可用pid
			break;
		}
	}
	if(i == sizeof(pcb_table)/sizeof(pcb_t)) //没有PCB可用了
	{
		popcli();
		return NULL;
	}
	
	/* 找到了一个可用PCB，开始初始化 */

	/* 设置eflags栈以及栈指针 */
	memset(&new_pcb->eflags_stack, 0, sizeof(new_pcb->eflags_stack));
	new_pcb->eflags_sp = PROC_PUSHCLI_DEPTH;

	/* 初始化等待队列号 */
	new_pcb->channel = NULL;

	/* 清零退出状态 */
	new_pcb->retval = 0;

	/* 分配一个清零过的内存页用作内核栈 */
	if((new_pcb->kstack = alloc_page()) == NULL)
		PANIC("alloc_pcb: alloc kernel stack failed");


	/* 设置内核栈内容，使esp指向内核栈顶位置 */
	uint32_t esp = (uint32_t)(new_pcb->kstack) + PROC_KERNEL_STACK_SIZE;

	/* 为trapframe留出空间，使新进程的pcb.tf指向trapframe起始位置 */
	esp -= sizeof(trapframe_t);
	new_pcb->tf = (trapframe_t *)esp;

	/* 在内核栈中写入trapret的入口地址 */
	esp -= 4;
	*((uint32_t *)esp) = (uint32_t)trapret;

	/* 在内核栈中构建context结构，使新进程的pcb.context指向context起始位置 */
	esp -= sizeof(context_t);
	new_pcb->context = (context_t *)esp;

	/* 先清空所有成员，然后使其eip指向forkret的入口地址，并在内核上下文中开启中断 */
	memset(new_pcb->context, 0, sizeof(context_t));
	new_pcb->context->eip = (uint32_t)forkret;
	new_pcb->context->eflags = FL_RESERVED | FL_IF | FL_IOPL_0; //IOPL为0
	
	return  new_pcb;
}


/*
 * 创建一个当前进程的副本作为其子进程
 * 成功将在系统中创建一个当前进程的副本进程为其子进程，
 * 父子进程在用户模式下的执行是连续的，
 * 在父进程中返回子进程的PID，在子进程中返回0；失败则返回-1；
 *
 * 注意：当前设定pid不能超过int32_t所能表示的最大值；
 */
int32_t fork(void)
{
	pcb_t * child;
	uint32_t pid;

	/* 分配一个PCB给子进程使用 */
	if((child = alloc_pcb()) == NULL)
		return -1;
	pid = child->pid;
	
	/* 创建当前进程的地址空间副本 */
	if((child->pgdir = copy_vm(cpu.cur_proc->pgdir, cpu.cur_proc->size)) == NULL)
	{
		/* 复制失败，回收资源 */
		free_page(child->kstack);
		
		pushcli();
		memset(child, 0, sizeof(*child));
		child->state = PROC_STATE_UNUSED;
		popcli();

		return -1;
	}

	/* 构建子进程 */

	child->parent = cpu.cur_proc;
	strncpy(child->name, cpu.cur_proc->name, PROC_NAME_LENGTH);
	*(child->tf) = *(cpu.cur_proc->tf);
	child->size = cpu.cur_proc->size;

	child->cwd = dup_inode(cpu.cur_proc->cwd);
	for(int32_t i = 0; i < PROC_OPEN_FD_NUM; i++)
	{
		if(cpu.cur_proc->open_files[i] != NULL)
			child->open_files[i] = dup_file(cpu.cur_proc->open_files[i]);
		else
			child->open_files[i] = NULL;
	}
	
	/* 使fork在子进程中返回0 */
	child->tf->eax = 0;

	/* 修改子进程状态需要关闭中断 */
	pushcli();
	child->state = PROC_STATE_RUNNABLE;
	popcli();
	
	/* fork在父进程中返回子进程的pid */
	return (int32_t)pid;

}


/*
 * 结束当前进程，保存退出状态以供父进程使用。
 * ret: 退出状态值；
 * 成功后释放当前进程所占据的部分资源，并使其进入ZOMBIE状态，不可再次运行，失败则PANIC；
 */
void exit(int32_t retval)
{
	pcb_t * proc;

	/* 保存退出状态以便父进程使用 */
	cpu.cur_proc->retval = retval;
	
	/* 关闭所有对打开文件结构的引用，丢弃对当前工作目录对应i节点的引用 */
	for(int32_t i = 0; i < PROC_OPEN_FD_NUM; i++)
	{
		if(cpu.cur_proc->open_files[i] != NULL)
		{
			close_file(cpu.cur_proc->open_files[i]);
			cpu.cur_proc->open_files[i] = NULL;
		}
	}
	release_inode(cpu.cur_proc->cwd);
	cpu.cur_proc->cwd = NULL;

	/* 释放当前进程的用户地址空间 */
	free_uvm(cpu.cur_proc->pgdir, (void *)PROC_LOAD_ADDR, (void *)KERNEL_VIRTUAL_ADDR_OFFSET);
	cpu.cur_proc->size = 0;

	pushcli();
	/* 唤醒父进程 */
	wakeup_noint(cpu.cur_proc->parent);
	
	/* 将所有子进程交给初始进程，如果有进入ZOMBIE的子进程则还需要唤醒初始进程 */
	for(proc = pcb_table; proc < &pcb_table[PROC_MAX_COUNT]; proc++)
	{
		if(proc->state != PROC_STATE_UNUSED && 
				proc->parent == cpu.cur_proc)
		{
			proc->parent = uinit_proc;
			if(proc->state == PROC_STATE_ZOMBIE)
				wakeup_noint(uinit_proc);
		}
	}

	/* 进入ZOMBIE状态，然后切换到scheduler，不再返回 */
	cpu.cur_proc->state = PROC_STATE_ZOMBIE;
	swtch( & (cpu.cur_proc->context), cpu.scheduler_context);

}


/*
 * 等待子进程结束，并获取其退出状态。
 * ret: 指向保存退出状态值的位置；
 *
 * 如果有子进程结束则释放其所占据的剩余资源，返回其pid，并保存其退出状态到*retval中；
 * 如果所有子进程都尚未结束则睡眠下去直到某个子进程结束执行；
 * 如果没有子进程则返回-1；
 */
int32_t wait(int32_t * retval)
{
	int32_t have_kids;
	pcb_t * proc;
	uint32_t pid;

	pushcli();
	for(;;)
	{
		have_kids = 0;
		for(proc = pcb_table; proc < &pcb_table[PROC_MAX_COUNT]; proc++)
		{
			if(proc->state == PROC_STATE_UNUSED ||
					proc->parent != cpu.cur_proc)
				continue;
			/* 找到一个子进程 */
			have_kids = 1;
			if(proc->state == PROC_STATE_ZOMBIE)
			{
				/* 且该子进程进入ZOMBIE状态 */
				/* 回收剩余被占用的资源 */
				*retval = proc->retval;
				pid = proc->pid;

				free_page(proc->kstack);
				free_vm(proc->pgdir);
				memset(proc, 0, sizeof(*proc));
				proc->state = PROC_STATE_UNUSED;
				
				popcli();
				return pid;
			}
		}
		
		if( ! have_kids) //没有子进程，返回-1
		{
			popcli();
			return -1;
		}

		/* 有子进程但是都没有结束，则等待它们结束 */
		/* 被唤醒后继续检查其子进程的状态 */
		sleep(cpu.cur_proc);
	}
}


/*
 * 调度函数，简单的轮转调度
 */
void scheduler(void)
{
	pcb_t * proc; //待运行的进程
	
	/* 轮流执行每一个RUNNABLE进程 */
	for(;;)
	{
		/* 暂时的开启中断，以便硬件唤醒进程 */
		sti();
		for(uint32_t i = 0; i < 1000; i++)
			;
		cli();

		/* DEBUG */
		//printk("scheduler running\n");
		//dump_proc(&pcb_table[0]);
		//dump_proc(&pcb_table[1]);
		for(proc = &pcb_table[0]; proc < &pcb_table[PROC_MAX_COUNT]; proc++)
		{
			if(proc->state != PROC_STATE_RUNNABLE)
				continue;
			
			lcr3(K_V2P(proc->pgdir)); //切换到待运行进程的分页结构中
			cpu.TSS.esp0 = (uint32_t)proc->kstack + PROC_KERNEL_STACK_SIZE; //准备该进程的内核栈
			proc->state = PROC_STATE_RUNNING;
			cpu.cur_proc = proc; //proc即为当前运行进程

			/* DEBUG */
			/* 即将运行的进程 */
			//printk("scheduler: entering another process\n");
			//dump_proc(proc);
			
			/* 保存scheduler的上下文，从proc的内核上下文开始执行 */
			swtch(&cpu.scheduler_context, proc->context);
			
			/* DEBUG */
			//printk("scheduler: return back to scheduler\n");

			lcr3(K_V2P(cpu.pgdir)); //切换回scheduler线程的分页结构
			cpu.cur_proc = NULL;
		}
	}
}


/*
 * fork成功后，子进程从这里返回trapret中
 */
void forkret(void)
{
	static int32_t is_uinit_proc = 1;
	/* DEBUG */
	/* 从forkret返回进程 */
	//printk("forkret: fork return\n");
	print_log("forkret: fork return\n");
	dump_proc(cpu.cur_proc);

	/*
	uint8_t buf[16];
	int32_t count;
	memset(buf, 0, sizeof(buf));
	while(1)
	{
		iprintk("#");
		count = tty_read(buf, sizeof(buf));
		iprintk("forkret: %d bytes has read\n", count);
		for(int32_t i = 0; i < count ; i++)
			iprintk("%hhX ", buf[i]);
		iprintk("\n");
	}
	*/

	
	if(is_uinit_proc)
	{
		cpu.cur_proc->cwd = resolve_path("/", 0, NULL);
		if( ! (cpu.cur_proc->cwd))
			PANIC("forkret: init working dir for init proc failed");
		is_uinit_proc = 0;
	}

	return;
}

/* =========================================== */
/*
 * 分配一个PCB结构，并初始化其中一部分信息。
 * 成功返回新分配的PCB地址，失败返回NULL。
 * 该函数逻辑上等同于 alloc_pcb，但是没有考虑锁住PCB表！
 *
 * 注意：在使用该函数时，要保证没有其他进程在使用PCB表；
 */
pcb_t * alloc_pcb_noint(void)
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
	

	/* 设置当前状态/eflags栈以及栈指针 */
	new_pcb->state = PROC_STATE_NEWBORN;
	memset(&new_pcb->eflags_stack, 0, sizeof(new_pcb->eflags_stack));
	new_pcb->eflags_sp = PROC_PUSHCLI_DEPTH;

	/* 初始化等待队列号 */
	new_pcb->channel = NULL;

	/* 清零退出状态 */
	new_pcb->retval = 0;

	/* 分配一个清零过的内存页用作内核栈 */
	if((new_pcb->kstack = alloc_page_noint()) == NULL)
		PANIC("alloc_pcb_noint: alloc kernel stack failed");


	/* 设置内核栈内容，使esp指向内核栈顶位置 */
	uint32_t esp = (uint32_t)(new_pcb->kstack) + PROC_KERNEL_STACK_SIZE;

	/* 为trapframe留出空间，使新进程的pcb.tf指向trapframe起始位置 */
	esp -= sizeof(trapframe_t);
	new_pcb->tf = (trapframe_t *)esp;

	/* 在内核栈中写入trapret的入口地址 */
	esp -= 4;
	*((uint32_t *)esp) = (uint32_t)trapret;

	/* 在内核栈中构建context结构，使新进程的pcb.context指向context起始位置 */
	esp -= sizeof(context_t);
	new_pcb->context = (context_t *)esp;

	/* 先清空所有成员，然后使其eip指向forkret的入口地址，并在内核上下文中开启中断 */
	memset(new_pcb->context, 0, sizeof(context_t));
	new_pcb->context->eip = (uint32_t)forkret;
	new_pcb->context->eflags = FL_RESERVED | FL_IF | FL_IOPL_0; //IOPL为0
	
	return  new_pcb;
}


/*
 * 构造第一个RUNNABLE的进程。
 */
void create_first_proc(void)
{
	pcb_t * proc;

	/* 分配PCB并完成一部分信息的初始化 */
	if((proc = alloc_pcb_noint()) == NULL)
		PANIC("create_first_proc: alloc pcb failed");
	
	/* 这就是初始进程 */
	uinit_proc = proc;
	
	/* 建立地址空间 */
	proc->pgdir = create_init_kvm_noint();
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
	proc->tf->eip = PROC_LOAD_ADDR; //暂设为从加载处开始执行
	
	/* 设置trapframe.ds/es/fs/gs成员 */
	proc->tf->ds = CONSTRUCT_SELECTOR(SEG_INDEX_UDATA, TI_GDT, RPL_USER);
	proc->tf->es = proc->tf->ds;
	proc->tf->fs = proc->tf->ds;
	proc->tf->gs = proc->tf->ds;

	/* 设置pcb.name成员 */
	strncpy(proc->name, "initcode", PROC_NAME_LENGTH);

	/* 清零打开文件指针表 */
	memset(proc->open_files, 0, sizeof(proc->open_files));
	
	/* 设置有效用户地址空间大小 */
	proc->size = (uint32_t)init_proc_size;

	/* 第一个进程无父进程 */
	proc->parent = NULL;
	
	/* 设置TSS，使其使用当前进程的内核栈，加载tr */
	memset(&cpu.TSS, 0, sizeof(cpu.TSS));
	cpu.TSS.ss0 = CONSTRUCT_SELECTOR(SEG_INDEX_KDATA, TI_GDT, RPL_KERNEL);
	cpu.TSS.esp0 = (uint32_t)(proc->kstack) + PROC_KERNEL_STACK_SIZE;

	ltr(CONSTRUCT_SELECTOR(SEG_INDEX_TSS, TI_GDT, RPL_KERNEL));

	/* 现在可以运行了 */
	proc->state = PROC_STATE_RUNNABLE;
}
/* =========================================== */

/*
 * 查看进程各项数据。
 */
void dump_proc(pcb_t * proc)
{
	print_log("[%u]%s ", proc->pid, proc->name);
	
	switch(proc->state)
	{
		case PROC_STATE_NEWBORN: print_log("%s: ", "NEWBORN"); break;
		case PROC_STATE_RUNNABLE: print_log("%s: ", "RUNABLE"); break;
		case PROC_STATE_RUNNING: print_log("%s: ", "RUNNING"); break;
		case PROC_STATE_SLEEPING: print_log("%s: ", "SLEEPING"); break;
		default: print_log("%s: ", "UNKNOWN");
	}
	
	print_log("\n");
	print_log("\tpgdir: %X kstack: %X tf: %X context: %X\n", proc->pgdir, proc->kstack, proc->tf, proc->context);
}

