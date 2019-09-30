/*
 * exec函数实现
 */
#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "parameters.h"
#include "fs.h"
#include "path.h"
#include "inode.h"
#include "vmm.h"
#include "vm_tools.h"
#include "elf.h"
#include "string.h"
#include "process.h"

/*
 * 将指定文件中off开始的size大小的内容读入pgdir中addr处。
 * ip: 指定文件，要求已上锁；
 * off: 文件偏移处；
 * size: 内容大小，字节单位；
 * pgdir: 指定分页结构；
 * addr: pgdir中的某个地址，要求addr到addr+size范围已经映射了内存；
 * 成功返回实际读入的字节数，失败返回-1；
 *
 * 注意：
 * 该函数仅适用于写入pgdir的用户地址空间中；
 */
/* 返回a,b中的最小值 */
#define MIN(a, b) ((uint32_t)(a) > (uint32_t)(b) ? (uint32_t)(b) : (uint32_t)(a))
int32_t load_uvm(pde_t * pgdir, void * addr, mem_inode_t * ip, uint32_t off, uint32_t size)
{
	char buf[128];
	int32_t min;
	uint32_t old_off;

	old_off = off;
	for(; size > 0; size = size - (uint32_t)min, off = off + (uint32_t)min, addr = (void *)((uint32_t)addr + (uint32_t)min))
	{
		min = (int32_t)MIN(size, sizeof(buf));
		if((min = read_inode(ip, buf, off, (uint32_t)min)) == -1)
			return -1;
		if(min == 0) //没有数据可读了
			break;
		if(copyout(pgdir, addr, buf, (uint32_t)min) == -1)
			return -1;
	}
	
	return (int32_t)(off - old_off);
}

/*
 * 加载指定程序替换当前进程内存镜像。
 * path: 指定所加载的程序；
 * argv: 新进程所需要的字符串参数列表，如果没有MAX_ARG_NUM个，则应该以NULL指针结尾；
 *
 * 如果加载并设置成功，原进程的分页结构、用户地址空间、进程名将被替换或释放掉，原进程的其余内容保持不变（包括内核栈），函数将不会返回，直接从所加载程序指定的入口开始执行；
 * 如果操作过程中失败，将返回-1且不会对原进程做任何修改。
 */
int32_t exec(const char * path, char * argv[])
{
	elfhdr_t eh;
	proghdr_t ph;
	uint32_t size;
	uint32_t off;
	uint32_t esp;
	uint32_t argc;
	uint32_t stack[2 + MAX_ARG_NUM]; //构造栈时用于保存argc/argv/argv[]这部分用户栈内容
	int32_t ret;
	char name[MAX_DIR_NAME_LEN];
	pde_t * old_pgdir;

	pde_t * new_pgdir = NULL;
	mem_inode_t * ip = NULL;

	/* 解析路径 */
	if((ip = resolve_path(path, 0, NULL)) == NULL)
		return -1;

	/* 锁住这个文件，便于读取 */
	lock_inode(ip);
	if(read_inode(ip, &eh, 0, sizeof(eh)) != sizeof(eh))
		goto bad;

	/* DEBUG */
	iprint_log("exec: has read elf header\n");

	/* 是否为合法的ELF文件 */
	if(eh.magic != ELF_MAGIC ||
			eh.elf[ELF_EI_CLASS] != ELF_CLASS32 ||
			eh.type != ELF_ET_EXEC)
		goto bad;

	/* DEBUG */
	iprint_log("exec: an valid elf exec file\n");
	
	/* 获得一个仅有内核地址空间映射的分页结构 */
	if((new_pgdir = create_init_kvm()) == NULL)
		goto bad;
	
	/* 检查/加载程序 */
	size = PROC_LOAD_ADDR;
	for(off = eh.phoff; off < eh.phoff + eh.phnum * eh.phentsize; off += eh.phentsize)
	{
		if(read_inode(ip, &ph, off, sizeof(ph)) != sizeof(ph))
			goto bad;
		if(ph.type != ELF_PT_LOAD)
			continue;
		if(ph.memsz < ph.filesz)
			goto bad;
		/* 检查通过，为该segment分配内存 */
		/* 不管各个ph.vaddr之间是否存在间隙，都统一分配内存 */
		if((size = (uint32_t)alloc_uvm(new_pgdir, (void *)size, (void *)(ph.vaddr + ph.memsz))) != ph.vaddr + ph.memsz)
			goto bad;
		/* 加载到ph.vaddr处 */
		if((ret = load_uvm(new_pgdir, (void *)ph.vaddr, ip, ph.off, ph.filesz)) != (int32_t)ph.filesz)
			goto bad;

		/* DEBUG */
		iprint_log("exec: segment in file offset %X[%X] has loaded into memory %X[%X]\n", 
		ph.off, ph.filesz,
		ph.vaddr, ph.memsz);
	}
	unlock_inode(ip);
	release_inode(ip);
	ip = NULL; //清除后，在bad处就不会再处理一次

	/* DEBUG */
	iprint_log("exec: all segments has loaded\n");
	
	/* 加载完毕后，还需要分配用户栈 */
	if(alloc_uvm(new_pgdir, (void *)PROC_USER_STACK_ADDR, (void *)(PROC_USER_STACK_ADDR + PROC_USER_STACK_SIZE)) != 
			(void *)(PROC_USER_STACK_ADDR + PROC_USER_STACK_SIZE))
		goto bad;

	/* 准备用户栈内容，为用户程序传递参数 */
	esp = PROC_USER_STACK_ADDR + PROC_USER_STACK_SIZE;
	for(argc = 0; argc < MAX_ARG_NUM && argv[argc] != NULL; argc++)
	{
		/* 为字符串(包括结尾的NUL字符)腾出空间，并向下按4字节对齐 */
		esp = (esp - (strlen(argv[argc]) + 1)) & ~3;
		/* 然后复制字符串和结尾的NUL字符到用户栈 */
		if(copyout(new_pgdir, (void *)esp, (void *)argv[argc], strlen(argv[argc]) + 1) == -1)
			goto bad;
		stack[2 + argc] = esp; //构造argv[]数组元素
	}
	stack[0] = argc; //main函数的argc参数值
	if(argc < MAX_ARG_NUM) //需要在argv[]尾部补充一个NULL指针
	{
		stack[2 + argc] = (uint32_t)NULL;
		argc++;
	}
	stack[1] = esp - argc * 4; //main函数的argv参数值
	esp -= (2 + argc) * 4; //为argc/argv/argv[]腾出空间
	
	if(copyout(new_pgdir, (void *)esp, stack, (2 + argc) * 4) == -1)
		goto bad;

	
	/* DEBUG */
	iprint_log("exec: user stack in pgdir %X is ready\n", new_pgdir);


	/* 修改进程名 */
	if(get_last_element(path, name) == -1)
		PANIC("exec: path error");
	strncpy(cpu.cur_proc->name, name, MIN(PROC_NAME_LENGTH, MAX_DIR_NAME_LEN));

	/* DEBUG */
	iprint_log("exec: has set new proc name: %s\n", cpu.cur_proc->name);

	
	/* 修改当前进程的其他内容 */
	old_pgdir = cpu.cur_proc->pgdir;
	pushcli();
	cpu.cur_proc->size = size;
	cpu.cur_proc->tf->esp = esp;
	cpu.cur_proc->tf->eip = eh.entry;
	cpu.cur_proc->pgdir = new_pgdir;
	lcr3(K_V2P(new_pgdir));
	flush_TLB();
	popcli();

	
	/* DEBUG */
	iprint_log("exec: has commited to current proc\n");
	
	/* 释放原先的分页结构和关联的内存 */
	free_vm(old_pgdir);
	return 0;

bad:
	/* DEBUG */
	iprint_log("exec: exec failed\n");

	if(ip)
	{
		unlock_inode(ip);
		release_inode(ip);
	}
	if(new_pgdir)
		free_vm(new_pgdir);
	return -1;
}
