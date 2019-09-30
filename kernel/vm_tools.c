/*
 * 用于支持进程内存操作等工具函数的实现
 */
#include <stdint.h>
#include <stddef.h>

#include "debug.h"
#include "parameters.h"
#include "vmm.h"
#include "string.h"
#include "terminal_io.h"
#include "process.h"

/*
 * 获得虚拟地址在指定分页结构中所对应的页表条目虚拟地址。
 * 如果need_create为真，则在需要时会创建中间页表；中间页表对应的页目录条目设置为用户特权级别的。
 * 如果need_create不为真则不会创建中间页表。当不存在中间页表时返回NULL。
 * pd: 指定页目录虚拟地址
 * vaddr: 需要查询的虚拟地址
 * need_create: 为真时，如果需要会创建中间页表；为假时则不会
 *
 * 注意：
 * 如果对当前使用的分页结构进行了修改，则将刷新TLB
 */
pte_t * walk_pgdir(pde_t * pd, void * vaddr, uint32_t need_create)
{
	if(pd == NULL || ((uint32_t)pd % PAGE_SIZE) != 0)
		PANIC("walk_pgdir: unavilable page directory");
	
	pde_t * pde;
	
	pde = &pd[PD_INDEX(vaddr)]; //找出对应页目录条目
	if( ! (*pde & PDE_PRESENT)) //不存在中间页表
	{
		if(need_create) //且需要创建
		{
			/* 分配一个被清零的空闲页用作中间页表 */
			void * pg;
			if((pg = alloc_page()) == NULL)
				PANIC("walk_pgdir: alloc page failed");

			*pde = (pde_t)(PFN(K_V2P(pg)) | PDE_PRESENT | PDE_USER | PDE_RW); //在页目录中建立映射关系

			/* 如果是操纵当前页目录，则刷新TLB */
			if(PFN(rcr3()) == K_V2P(pd))
				flush_TLB();
		}
		else
			return NULL;
	}
	
	pte_t * pt;
	pt = (pte_t *)K_P2V(PFN(*pde));
	
	return (&pt[PT_INDEX(vaddr)]);
}


/*
 * 在指定分页结构中，将一段物理地址空间映射到一段虚拟地址空间上。
 * 如果某个位置已经存在映射则将PANIC。
 * pd: 指定页目录虚拟地址
 * vaddr: 指定虚拟地址空间的起始地址，必须按页对齐
 * size: 指定虚拟地址空间的大小，不足一个页面的按一个页面计算
 * paddr: 指定物理地址空间的起始地址，必须可用，按页对齐且为连续的页面
 * pte_attr: 页表条目的属性
 */
void map_pages(pde_t * pd, void * vaddr, uint32_t size, void * paddr, uint32_t pte_attr)
{
	pte_t * pte;

	size = PAGE_UPPER_ALIGN(size);
	
	for(uint32_t addr = (uint32_t)vaddr; addr < (uint32_t)vaddr + size; addr += PAGE_SIZE, paddr = (void *)((uint32_t)paddr + PAGE_SIZE))
	{
		pte = walk_pgdir(pd, (void *)addr, 1); //需要时创建中间页表，对应页目录条目为用户特权级的
		if(*pte & PTE_PRESENT)
			PANIC("map_pages: remap");
		*pte = (pte_t)(PFN(paddr) | (pte_attr & 0xFFF));
	}
}


/*
 * 创建一个新的分页结构并在其中建立初始的内核地址空间映射，返回新建页目录的虚拟地址，失败则PANIC。
 * 物理地址区域0x0 ~ SUPPORT_MEM_SIZE将被映射到KERNEL_VIRTUAL_ADDR_OFFSET ~
 * KERNEL_VIRTUAL_ADDR_OFFSET + SUPPORT_MEM_SIZE处，对应PDE为用户特权级别，PTE为系统特权级别。
 */
pde_t * create_init_kvm(void)
{
	pde_t * pd;

	/* 分配清零的页目录 */
	if((pd = alloc_page()) == NULL)
		PANIC("create_init_kvm: alloc page failed");
	
	/* 在pd中构建映射关系 */
	map_pages(pd, (void *)KERNEL_VIRTUAL_ADDR_OFFSET, SUPPORT_MEM_SIZE, (void *)0x0, PTE_PRESENT | PTE_RW);

	return pd;
}

/*
 * 释放指定分页结构的用户地址空间中从start_addr到end_addr之间可能被映射的内存，
 * 成功返回start_addr，发生错误则PANIC。
 * pgdir： 指定分页结构
 * start_addr： 起始地址，可以不按页对齐，释放时从PAGE_UPPER_ALIGN(start_addr)开始
 * end_addr： 结束地址，可以不按页对齐，释放时将截至至PAGE_UPPER_ALIGN(end_addr)
 *
 * 注意：
 * 如果end_addr小于等于start_addr，则不会释放内存；
 */
void * free_uvm(pde_t * pgdir, void * start_addr, void * end_addr)
{
	void * addr;
	pte_t * pte;

	if( ! pgdir)
		PANIC("free_uvm: unvalid page dir");
	if((uint32_t)end_addr > KERNEL_VIRTUAL_ADDR_OFFSET)
		PANIC("free_uvm: can't free kernel address space");
	if((uint32_t)end_addr <= (uint32_t)start_addr)
		return start_addr;

	addr = (void *)PAGE_UPPER_ALIGN(start_addr);
	while((uint32_t)addr < (uint32_t)end_addr)
	{
		/* 遍历检查这个地址区间 */

		if((pte = walk_pgdir(pgdir, addr, 0)) == NULL)
		{
			/* 不存在中间页表，该位置没有映射内存，从下一个中间页表映射开始位置继续处理 */
			addr = (void *)((uint32_t)addr + PT_ENTRY_COUNT * PAGE_SIZE);
			addr = (void *)PT_DOWN_ALIGN(addr);
			continue;
		}
		else if(*pte & PTE_PRESENT)
		{
			/* 存在中间页表，且映射了内存 */
			/* 在free_page时将检查该地址的有效性，无效则PANIC */
			free_page((void *)K_P2V(PFN(*pte)));
			*pte = (pte_t)0;
		}
		addr = (void *)((uint32_t)addr + PAGE_SIZE);
	}

	return start_addr;
}

/*
 * 为pgdir的用户地址空间中start_addr到end_addr这段地址空间分配内存，
 * 成功返回end_addr，失败返回NULL且被分配的内存将被释放掉。
 * pgdir: 指定分页结构
 * start_addr: 起始地址，可以不按页对齐，分配时将从PAGE_UPPER_ALIGN(start_addr)开始
 * end_addr: 结束地址，可以不按页对齐，分配时将截至至PAGE_UPPER_ALIGN(end_addr)
 *
 * 注意：
 * 如果end_addr小于等于start_addr，则不会分配内存，返回start_addr。
 */
void * alloc_uvm(pde_t * pgdir, void * start_addr, void * end_addr)
{
	void * addr;
	void * page;

	if( ! pgdir)
		PANIC("alloc_uvm: invalid page dir");
	if((uint32_t)end_addr > KERNEL_VIRTUAL_ADDR_OFFSET)
		return NULL;
	if((uint32_t)end_addr <= (uint32_t)start_addr)
		return start_addr;

	addr = (void *)PAGE_UPPER_ALIGN(start_addr);
	while((uint32_t)addr < (uint32_t)end_addr)
	{
		/* 分配用于addr到PAGE_UPPER_ALIGN(end_addr)的内存 */
		if((page = alloc_page()) == NULL)
		{
			/* 分配失败，回收之前分配的内存 */
			free_uvm(pgdir, start_addr, end_addr);
			return NULL;
		}
		/* 映射到用户地址空间中 */
		map_pages(pgdir, addr, PAGE_SIZE, (void *)K_V2P(page), PTE_PRESENT | PTE_RW | PTE_USER);

		addr = (void *)((uint32_t)addr + PAGE_SIZE);
	}

	return end_addr;
}

/*
 * 释放分页结构管理的内存以及整个分页结构，
 * 不包括内核地址空间所映射的内存，失败则PANIC。
 */
void free_vm(pde_t * pgdir)
{
	if( ! pgdir)
		PANIC("free_vm: unvalid page dir");
	
	/* 释放从PROC_LOAD_ADDR到KERNEL_VIRTUAL_ADDR_OFFSET之间可能被映射的内存 */
	/* 0x0到PROC_LOAD_ADDR没有被映射 */
	free_uvm(pgdir, (void *)PROC_LOAD_ADDR, (void *)KERNEL_VIRTUAL_ADDR_OFFSET);
	
	/* 释放分页结构 */
	for(uint32_t i = 0; i < PD_ENTRY_COUNT; i++)
		if(pgdir[i] & PDE_PRESENT)
			free_page((void *)K_P2V(PFN(pgdir[i])));
	free_page(pgdir);
}


/*
 * 将指定分页结构中的用户地址空间地址转换为内核地址空间地址，
 * 成功返回映射后结果，失败返回NULL。
 * pgdir： 指定分页结构
 * uvaddr： 指定用户地址空间地址
 */
void * uva2kva(pde_t * pgdir, void * uvaddr)
{
	pte_t * pte;

	if( ! pgdir)
		PANIC("uva2kva: unvalid page dir");

	pte = walk_pgdir(pgdir, uvaddr, 0);
	if(pte == NULL || !(*pte & PTE_PRESENT) || !(*pte & PTE_USER))
		return NULL;

	return (void *)(K_P2V(PFN(*pte)) + PG_OFFSET(uvaddr));
}


/*
 * 从指定位置pos复制len个字节到指定分页结构pgdir中的addr处，
 * 成功返回0，失败返回-1。
 * pgdir： 指定分页结构
 * addr： 目标地址
 * pos： 起始位置
 * len：需要复制的字节数
 * 
 * 注意：
 * 该函数假定从addr到addr+len之间是存在内存映射的；
 * 该函数仅适用于复制数据到具有用户权限属性的地址空间中，即addr到addr+len之间的地址是具有用户权限属性的；
 */
/* 返回a,b中的最小值 */
#define MIN(a, b) ((uint32_t)(a) > (uint32_t)(b) ? (uint32_t)(b) : (uint32_t)(a))
int32_t copyout(pde_t * pgdir, void * addr, void * pos, uint32_t len)
{
	void * kvaddr;
	uint32_t n;

	if( ! pgdir)
		PANIC("copyout: unvalid page dir");
	while(len > 0)
	{
		/* 获取对应的内核地址 */
		if((kvaddr = uva2kva(pgdir, addr)) == NULL)
			/* 说明addr没有被映射到内存或者addr具有内核权限属性 */
			return -1;
		
		/* 确定这次复制的字节数 */
		n = MIN(PAGE_DOWN_ALIGN(kvaddr) + PAGE_SIZE - (uint32_t)kvaddr, len);
		memcpy(kvaddr, pos, n);

		len -= n; //余量减n
		addr = (void *)((uint32_t)addr + n);
		pos = (void *)((uint32_t)pos + n);
	}

	return 0;
}


/*
 * 创建一个指定地址空间的副本。
 * pgdir: 被复制的地址空间；
 * size: 从PROC_LOAD_ADDR开始的有效用户地址空间大小，不包括用户栈；
 * 成功返回所创建副本的页目录地址，失败返回NULL；
 *
 * 注意：
 * 副本中用户地址空间内容是相同但独立的，内核地址空间内容是相同的但并不独立；
 * 如果原地址空间中有hole，则复制后的地址空间中也有hole；
 * 如果复制失败则之前所分配的内存均将被释放掉；
 */
pde_t * copy_vm(pde_t * pgdir, uint32_t size)
{
	pde_t * new_pgdir;
	uint32_t addr;
	pte_t * pte;
	pte_t * new_pte;

	/* 相同的内核地址空间映射 */
	new_pgdir = create_init_kvm();

	/* 复制非用户栈部分的用户地址空间内容 */
	for(addr = PROC_LOAD_ADDR; addr < PROC_LOAD_ADDR + size; addr += PAGE_SIZE)
	{
		if((pte = walk_pgdir(pgdir, (void *)addr, 0)) == NULL)
		{
			/* 没有中间页表，跳到下一个中间页映射位置继续处理 */
			addr += PT_ENTRY_COUNT * PAGE_SIZE;
			addr = PT_DOWN_ALIGN(addr) - PAGE_SIZE;
			continue;
		}
		if( !(*pte & PTE_PRESENT))
			continue;
		
		/* 存在映射，则在new_pgdir中同样位置处分配内存，然后复制数据 */
		if(alloc_uvm(new_pgdir, (void *)addr, (void *)(addr + PAGE_SIZE)) == NULL)
			goto bad;
		new_pte = walk_pgdir(new_pgdir, (void *)addr, 0);
		memcpy((void *)K_P2V(PFN(*new_pte)), (void *)K_P2V(PFN(*pte)), PAGE_SIZE);
	}
	
	/* 再分配用户栈 */
	if(alloc_uvm(new_pgdir, (void *)PROC_USER_STACK_ADDR, (void *)(PROC_USER_STACK_ADDR + PROC_USER_STACK_SIZE)) == NULL)
		goto bad;
	/* 然后复制用户栈内容 */
	for(addr = PROC_USER_STACK_ADDR; addr < PROC_USER_STACK_ADDR + PROC_USER_STACK_SIZE; addr += PAGE_SIZE)
	{
		pte = walk_pgdir(pgdir, (void *)addr, 0);
		if(pte == NULL || !(*pte & PTE_PRESENT))
			PANIC("copy_vm: broken pgdir");
		new_pte = walk_pgdir(new_pgdir, (void *)addr, 0);
		memcpy((void *)K_P2V(PFN(*new_pte)), (void *)K_P2V(PFN(*pte)), PAGE_SIZE);
	}
	
	/* 已经获得了一个pgdir的副本 */
	return new_pgdir;

bad:
	free_vm(new_pgdir);
	return NULL;
}

/* ======================================== */
/* 以下函数避免了使用pushcli/popcli，在
 * 没有其他进程在使用带锁的内存分配接口时使用。
 */

/*
 * 获得虚拟地址在指定分页结构中所对应的页表条目虚拟地址。该函数执行逻辑等同于
 * walk_pgdir，除了在分配内存时，使用的是不带锁的分配内存接口!
 * 
 * 注意：在使用该接口时，要能够保证没有其他进程在使用带锁的内存分配接口
 */
pte_t * walk_pgdir_noint(pde_t * pd, void * vaddr, uint32_t need_create)
{
	if(pd == NULL || ((uint32_t)pd % PAGE_SIZE) != 0)
		PANIC("walk_pgdir_noint: unavilable page directory");
	
	pde_t * pde;
	
	pde = &pd[PD_INDEX(vaddr)]; //找出对应页目录条目
	if( ! (*pde & PDE_PRESENT)) //不存在中间页表
	{
		if(need_create) //且需要创建
		{
			/* 分配一个被清零的空闲页用作中间页表 */
			void * pg;
			/* 使用不带锁的内存分配接口 */
			if((pg = alloc_page_noint()) == NULL)
				PANIC("walk_pgdir_noint: alloc page failed");

			*pde = (pde_t)(PFN(K_V2P(pg)) | PDE_PRESENT | PDE_USER | PDE_RW); //在页目录中建立映射关系

			/* 如果是操纵当前页目录，则刷新TLB */
			if(PFN(rcr3()) == K_V2P(pd))
				flush_TLB();
		}
		else
			return NULL;
	}
	
	pte_t * pt;
	pt = (pte_t *)K_P2V(PFN(*pde));
	
	return (&pt[PT_INDEX(vaddr)]);
}

/*
 * 在指定分页结构中，将一段虚拟地址空间映射到一段物理地址空间。
 * 如果某个位置已经存在映射则将PANIC。
 * 函数执行逻辑等同于map_pages，但在获取页表条目虚拟地址时，使用的是walk_pgdir_noint！
 *
 * 注意：在使用该接口时，要能够保证没有其他进程在使用带锁的内存分配接口
 */
void map_pages_noint(pde_t * pd, void * vaddr, uint32_t size, void * paddr, uint32_t pte_attr)
{
	pte_t * pte;

	size = PAGE_UPPER_ALIGN(size);
	
	for(uint32_t addr = (uint32_t)vaddr; addr < (uint32_t)vaddr + size; addr += PAGE_SIZE, paddr = (void *)((uint32_t)paddr + PAGE_SIZE))
	{
		/* 使用walk_pgdir_noint接口 */
		pte = walk_pgdir_noint(pd, (void *)addr, 1); //需要时创建中间页表，对应页目录条目为用户特权级的
		if(*pte & PTE_PRESENT)
			PANIC("map_pages_noint: remap");
		*pte = (pte_t)(PFN(paddr) | (pte_attr & 0xFFF));
	}
}

/*
 * 创建进程的初始内核地址分页结构，并返回新建页目录的虚拟地址。
 * 函数执行逻辑等同于create_init_kvm，但使用的是加*_noint的接口
 *
 * 注意：在使用该接口时，要能够保证没有其他进程在使用带锁的内存分配接口
 */
pde_t * create_init_kvm_noint(void)
{
	pde_t * pd;

	/* 分配清零的页目录 */
	if((pd = alloc_page_noint()) == NULL)
		PANIC("create_init_kvm_noint: alloc page failed");
	
	/* 在pd中构建映射关系 */
	map_pages_noint(pd, (void *)KERNEL_VIRTUAL_ADDR_OFFSET, SUPPORT_MEM_SIZE, (void *)0x0, PTE_PRESENT | PTE_RW);

	return pd;
}

/*
 * 初始化第一个进程的用户地址空间。
 * pd: 第一个进程的页目录虚拟地址
 * init_proc_start_addr: 第一个进程加载在内存中的起始物理地址
 * init_proc_size: 第一个进程在内存中的大小，不能超出PAGE_SIZE
 */
void init_first_proc_uvm(pde_t * pd, uint32_t init_proc_start_addr, uint32_t init_proc_size)
{
	void * pg;

	/* 不能太大 */
	if(init_proc_size > PAGE_SIZE)
		PANIC("init_first_proc_uvm: need more than a page");
	
	/* 分配一页清零过的内存用作保存该进程的代码/数据 */
	if((pg = alloc_page_noint()) == NULL)
		PANIC("init_first_proc_uvm: alloc memory for user address space failed");

	/* 映射到该进程的分页结构中 */
	map_pages_noint(pd, (void *)PROC_LOAD_ADDR, init_proc_size, (void *)K_V2P(pg), PTE_PRESENT | PTE_RW | PTE_USER);
	
	/* 再将代码/数据复制进去 */
	memcpy(pg, (void *)K_P2V(init_proc_start_addr), init_proc_size);

	/* 设置用户栈 */
	for(uint32_t vaddr = PROC_USER_STACK_ADDR; vaddr < PROC_USER_STACK_ADDR + PROC_USER_STACK_SIZE; vaddr += PAGE_SIZE)
	{
		/* 从空闲内存中分配一页清零的内存 */
		if((pg = alloc_page_noint()) == NULL)
			PANIC("init_first_proc_uvm: alloc page for user stack failed");
		map_pages_noint(pd, (void *)vaddr, PAGE_SIZE, (void *)K_V2P(pg), PTE_PRESENT | PTE_RW | PTE_USER);
	}
}
