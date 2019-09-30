/*
 * 本文件提供与multiboot协议相关的函数实现
 * 
 * 注意：这里的函数实现默认mmap是20字节大小，并且只使用32位的地址和长度。
 */
#include <stdint.h>

#include "multiboot.h"
#include "terminal_io.h"
#include "vmm.h"


/*
 * 该函数用于输出GRUB提供的内存布局信息
 * 注意：该函数必须在完全开启分页模式之后再被调用
 */
void show_memory_map(void)
{
	/* 从Multiboot information struct中获取mmap相关信息 */
	uint32_t mmap_entry_count = glb_mbi->mmap_length / sizeof(mmap_entry_t);
	mmap_entry_t * mmap_entries = (mmap_entry_t *)(glb_mbi->mmap_addr + KERNEL_VIRTUAL_ADDR_OFFSET);  //因为此时glb_mbi中给出地址仍然是尚未开启分页模式之前的地址，在完全开启分页模式之后，低端的映射被撤销，所以需要添加一个偏移量

	for(uint32_t i = 0; i < mmap_entry_count; i++)
	{
		printk("Region %u\thd:ld %X:%X, hl:ll %X:%X, ", i,
		mmap_entries[i].addr_high, mmap_entries[i].addr_low,
		mmap_entries[i].len_high, mmap_entries[i].len_low);
		switch(mmap_entries[i].type)
		{
			case 1: printk("available\n");
				break;
			case 3: printk("acpi reclaimable\n");
				break;
			case 4: printk("reserve on hibernation\n");
				break;
			case 5: printk("bad memory\n");
				break;
			default: printk("reserved\n");
		}
	}

}


/*
 * 检查paddr所在物理页框是否为有效物理内存，成功返回1，失败返回0
 */
int32_t mem_validate(uint32_t paddr)
{
	paddr = PAGE_DOWN_ALIGN(paddr);
	/* 获得mmap结构的起始地址 */
	mmap_entry_t * mpe =  (mmap_entry_t *)K_P2V(glb_mbi->mmap_addr);

	/* 遍历所有内存区域 */
	for(; mpe < (mmap_entry_t *)K_P2V(glb_mbi->mmap_addr + glb_mbi->mmap_length); mpe++)
	{
		if(mpe->type == MULTIBOOT_MEMORY_AVAILABLE)
		{
			if(paddr >= mpe->addr_low && (paddr + PAGE_SIZE) <= (mpe->addr_low + mpe->len_low))
				return 1;
		}
	}
	return 0;
}
