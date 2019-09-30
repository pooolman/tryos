/*
 * 本文件提供用于操纵gdt的函数实现
 */

#include "gdt.h"
#include "process.h"

static gdt_ptr_t gdt_ptr; //用于写入GDTR寄存器


extern void gdt_flush(uint32_t);


/*
 * 写入一个新的条目到GDT中
 * limit: 段界限，仅低20位有效
 * base: 段基址
 * attr: 段属性，第11到第8位无效（从0开始），其余各位对应于
 * 段描述符的各个属性
 */
static void install_gdt_entry(int32_t index, uint32_t limit, uint32_t base, uint16_t attr)
{
	cpu.GDT[index].limit_low = (uint16_t)limit;
	cpu.GDT[index].base_addr_low = (uint16_t)base;
	cpu.GDT[index].base_addr_middle = (uint8_t)(base >> 16);
	cpu.GDT[index].base_addr_high = (uint8_t)(base >> 24);
	limit = (limit >> 8) & 0xF00;
	attr = attr & 0xF0FF;
	cpu.GDT[index].attributes = ((uint16_t)limit) | attr;
}

/*
 * 初始化cpu.GDT，并且刷新GDTR。
 * 具体的GDT定义参考demand_skeleton_analysis.txt
 */

void init_gdt(void)
{
	/* 待写入GDTR的内容 */
	gdt_ptr.limit = (uint16_t)(sizeof(gdt_entry_t)*GDT_ENTRY_COUNT) - 1;
	gdt_ptr.base = (uint32_t)cpu.GDT;

	/* 安装段描述符到GDT中 */
	install_gdt_entry(SEG_INDEX_NULL, 0, 0, 0); //intel要求第一个GDT条目必须为全0
	install_gdt_entry(SEG_INDEX_KCODE, 0xFFFFF, 0x0, 0xC09A); //内核模式代码段描述符
	install_gdt_entry(SEG_INDEX_KDATA, 0xFFFFF, 0x0, 0xC092); //内核模式数据段描述符
	install_gdt_entry(SEG_INDEX_UCODE, 0xFFFFF, 0x0, 0xC0FA); //用户模式代码段描述符
	install_gdt_entry(SEG_INDEX_UDATA, 0xFFFFF, 0x0, 0xC0F2); //用户模式数据段描述符
	install_gdt_entry(SEG_INDEX_TSS, sizeof(cpu.TSS)-1, (uint32_t)&cpu.TSS, 0x89); //TSS描述符

	/* 刷新GDTR */
	gdt_flush((uint32_t)&gdt_ptr);
}


