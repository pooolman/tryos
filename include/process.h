#ifndef _INCLUDE_PROCESS_H_
#define _INCLUDE_PROCESS_H_

#include "gdt.h"
#include "idt.h"
#include "vmm.h"
#include "fs.h"
#include "file.h"
#include "parameters.h"

#define PROC_NAME_LENGTH	15 //进程的名字长度
#define PROC_MAX_COUNT		20 //进程最大个数
#define PROC_PUSHCLI_DEPTH	10 //允许该进程pushcli的次数

// Intel TSS结构，所有的padding必须设置为0
typedef struct {
	uint16_t link; // Old ts selector
	uint16_t padding0;
	uint32_t esp0; // Stack pointers and segment selectors
	uint16_t ss0; // after an increase in privilege level
	uint16_t padding1;
	uint32_t esp1;
	uint16_t ss1;
	uint16_t padding2;
	uint32_t esp2;
	uint16_t ss2;
	uint16_t padding3;
	uint32_t cr3; // Page directory base
	uint32_t eip; // Saved state from last task switch
	uint32_t eflags;
	uint32_t eax; // More saved state (registers)
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint16_t es; // Even more saved state (segment selectors)
	uint16_t padding4;
	uint16_t cs;
	uint16_t padding5;
	uint16_t ss;
	uint16_t padding6;
	uint16_t ds;
	uint16_t padding7;
	uint16_t fs;
	uint16_t padding8;
	uint16_t gs;
	uint16_t padding9;
	uint16_t ldt;
	uint16_t padding10;
	uint16_t t; // Trap on task switch, only lowest bit is used
	uint16_t iomb; // I/O map base address
} tss_t;

/* 内核上下文；
 * 根据cdecl，eax/ecx/edx由调用者保存；
 * esp由指向该结构体变量的指针隐式的保存；
 * cs/ds/ss/es/fs/gs则为共用的；
 */
typedef struct {
	uint32_t ebx;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t eflags;
	uint32_t eip;
} context_t;


// PCB.state值，除UNUSED外，其余表示进程状态
typedef enum {PROC_STATE_UNUSED,
	PROC_STATE_NEWBORN,
	PROC_STATE_RUNNABLE,
	PROC_STATE_RUNNING,
	PROC_STATE_SLEEPING,
	PROC_STATE_ZOMBIE} proc_state_t;

/* PCB定义；除了scheduler，每一个进程都有一个PCB与之对应
 */
typedef struct _pcb_t {
	uint32_t	pid; //每个进程pid唯一
	char		name[PROC_NAME_LENGTH + 1];  //进程名，用作debug
	proc_state_t	state;
	pde_t *		pgdir;
	void *		kstack;
	trapframe_t *	tf;
	context_t *	context;
	uint32_t 	eflags_stack[PROC_PUSHCLI_DEPTH];
	uint32_t	eflags_sp; //上一次压入的eflags在eflags_stack中的位置
	void *		channel; //如果不是NULL，则为等待队列号，是内核地址空间中一个数据结构的地址
	mem_inode_t *	cwd; //当前工作目录
	file_t *	open_files[PROC_OPEN_FD_NUM]; //打开文件指针列表
	uint32_t	size; //从PROC_LOAD_ADDR开始的有效用户地址空间大小，不包括用户栈
	struct _pcb_t * parent; //父进程对应PCB的指针
	int32_t		retval; //进程退出时的状态值
} pcb_t;


/* 与CPU相关的数据结构；
 * scheduler_context指向该CPU的调度器线程内核上下文
 */
typedef struct {
	gdt_entry_t GDT[GDT_ENTRY_COUNT]; //整个系统共有的GDT/IDT/TSS
	idt_entry_t IDT[IDT_ENTRY_COUNT];
	tss_t TSS;
	pde_t * pgdir; //调度器线程的页目录虚拟地址
	context_t * scheduler_context; //调度器线程的内核上下文，实际保存在其内核栈中
	pcb_t * cur_proc; //当前运行/即将运行在该CPU上的进程，不包括scheduler
} cpu_t;


extern cpu_t cpu;

/* 在process.asm中定义 */
/* 切换内核线程的上下文 */
extern void swtch(context_t ** from, context_t * to);

void ltr(uint16_t tss);
pcb_t * alloc_pcb(void);
void scheduler(void);

void pushcli(void);
void popcli(void);
void sleep(void * channel);
void wakeup(void * channel);
void wakeup_noint(void * channel);

int32_t fork(void);
void exit(int32_t retval);
int32_t wait(int32_t * retval);

/* ========================================= */
pcb_t * alloc_pcb_noint(void);
void create_first_proc(void);

/* ========================================= */
void dump_proc(pcb_t * proc);

#endif //_INCLUDE_PROCESS_H_
