/*
 * 用户程序链接脚本。
 * 系统通过exec加载用户程序为进程。
 */
ENTRY(_ustart)

SECTIONS
{
	/* 起始VMA为PROC_LOAD_ADDR */
	. = 0x1000;
	
	/* 忽略LMA，要求4KB对齐 */
	/* 文本 */
	.text :
	ALIGN(4K)
	{
		*(.text)
	}
	/* 只读数据 */
	.rodata : 
	ALIGN(4K)
	{
		*(.rodata)
	}
	/* 可读写数据 */
	.data : 
	ALIGN(4K)
	{
		*(.data)
	}
	/* 待初始化数据 */
	.bss : 
	ALIGN(4K)
	{
		*(.bss)
	}
}
