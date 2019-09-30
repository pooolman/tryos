/*
 * 本文件提供操纵文本模式显卡的工具实现，包括：
 * 	清空屏幕；
 * 	写入单个字符（分为默认颜色和可设定颜色两个功能）；
 * 	移动光标到指定逻辑位置；
 * 	获取当前光标逻辑位置；
 * 	写入一个以NUL（ASCII码，空字符）字符结尾的字符串；
 */

#include "console.h"
#include "x86.h"

/* 定义光标的逻辑位置 */
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

/* 定义显存地址 */
/* 在开启分页模式之后，需要使用新的虚拟地址访问显存 */
static uint16_t * vga_memory = (uint16_t *)0xB8000;


/*
 * 构造字符的属性
 * 注意：
 * fg 只应该为 vga_color_t 中的前8种颜色，此时字符不会闪烁，若需要字符闪烁，则
 * 需要将 fg 与 0x08 相或
 */
static inline uint8_t construct_attribute( vga_color_t fg, vga_color_t bg)
{
	return (uint8_t)( fg | bg << 4 );
}

/*
 * 构造一个适合写入文本模式显存的条目（字符+相关属性）
 */
static inline uint16_t construct_entry( unsigned char ch, uint8_t attr )
{
	return ( (uint16_t)ch | (uint16_t)attr << 8 );
}

/*
 * 使用光标逻辑位置（cursor_x,cursor_y）更新与光标位置有关的显卡寄存器
 */
static void update_cursor_reg(void)
{
	uint16_t location = cursor_y * VGA_WIDTH + cursor_x; //将光标逻辑位置转换为硬件所能接受的形式
	
	outb( 0x3d4, 0x0e ); //指定待操作的端口号为0x0e
	outb( 0x3d5, (uint8_t)(location >> 8) ); //先向0x0e端口写入location的高8位
	outb( 0x3d4, 0x0f ); //指定待操作端口为0x0f
	outb( 0x3d5, (uint8_t)(location) ); //然后向0x0f端口写入location的低8位
}

/*
 * 对屏幕内容向上滚动一行，消失的行内容不予保存，新增的行使用（空白字符+白色前景+黑色背景+不闪烁）这种条目填充；
 * 光标位置总是随之向上移动一行，除非光标已经位于最顶行，此时光标不再移动。
 */
static void scroll( void )
{
	/* 构造待写入的条目 */
	uint16_t entry = construct_entry( ' ', construct_attribute( VGA_COLOR_WHITE, VGA_COLOR_BLACK ) );
	
	/* 将除去第一行之外的所有行逐行向上移动 */
	for( uint16_t i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++ )
		vga_memory[i] = vga_memory[i + VGA_WIDTH];

	/* 并使用空白字符填充最后一行 */
	for( uint16_t i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
		vga_memory[i] = entry;

	if( cursor_y > 0 )
	{
		/* 如果当前光标没有位于最顶行，那么也将光标向上移动一行 */
		cursor_y--;
		update_cursor_reg();
	}
}


/*
 * 清空屏幕--即使用空白字符和指定属性（白色前景+黑色背景+无闪烁）填充显存；
 * 同时使光标移动到屏幕第一个字符位置处
 */
void console_clear_screen(void)
{
	/* 构造待写入的条目 */
	uint16_t entry = construct_entry( ' ', construct_attribute( VGA_COLOR_WHITE, VGA_COLOR_BLACK ) );
	
	for( uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
		vga_memory[i] = entry;

	/* 移动光标到屏幕第一个字符处 */
	cursor_x = cursor_y = 0;
	update_cursor_reg();
}

/*
 * 将一个给定属性（前景色+背景色+无闪烁）的字符写在光标位置处，同时光标向后移动一个位置，如果必要则会使屏幕内容
 * 滚动（上移一行）；特殊字符将特殊处理，处理方式参考下面实现和相关文档。
 */
void console_write_char_color( const char ch, const vga_color_t fg, const vga_color_t bg )
{
	/* 首先判断要写入的是什么字符？ */
	if( ch == '\n' )
	{
		/* 为LF字符 */
		if( cursor_y + 1 >= VGA_HEIGHT )
		{
			/* 光标已处于屏幕最下面一行 */
			scroll(); //将屏幕向上滚动一行，同时光标也随之向上移动一行，即cursor_y的值也被改变
		}
		/* 将光标移动到下一行起始位置 */
		cursor_x = 0;
		cursor_y++;
		update_cursor_reg(); //保持逻辑位置和实际硬件寄存器值的一致性
	}else if ( ch == '\r' )
	{
		/* 为CR字符 */
		cursor_x = 0; //光标回到当前行的起始位置
		update_cursor_reg();
	}else if ( ch == '\t' )
	{
		/* 为HT字符*/
		uint8_t cursor_next_x = (cursor_x + 4) & 0xFC; //下一个位置应该是哪里？
		if( cursor_next_x >= VGA_WIDTH )
		{
			/* 下一个位置已经超出了屏幕最右边 */

			if( cursor_y + 1 >= VGA_HEIGHT )
				/* 且光标已经处于屏幕最低行，那么就向上滚动一行 */
				scroll();
			/* 处理之后，将光标移动到下一行起始位置 */
			cursor_x = 0;
			cursor_y++;
		}else
			cursor_x = cursor_next_x; //只需要在当前行移动即可
		/* 最后，保持逻辑位置和实际硬件寄存器值的一致性 */
		update_cursor_reg();
	}else if( ch == 0x08 )
	{
		/* 为 BS（Backspce）字符 */
		cursor_x = (cursor_x - 1) > 0 ? (cursor_x - 1) : 0;
		update_cursor_reg();
	}else //if( ch >= ' ' && ch <= '~')
	{
		/* 其余字符都做如下处理 */
		
		/* 首先构造出一个entry */
		uint16_t entry = construct_entry( (unsigned char)ch, construct_attribute( fg, bg ) );
		/* 然后将entry写入显存 */
		vga_memory[cursor_y * VGA_WIDTH + cursor_x] = entry;
		
		if( cursor_x + 1 >= VGA_WIDTH )
		{
			/* 光标的下一个位置超出了屏幕的最右边 */

			if( cursor_y + 1 == VGA_HEIGHT )
				/* 且光标位于屏幕最低行 */
				scroll();

			/* 处理之后，将光标移动到下一行起始位置 */
			cursor_x = 0;
			cursor_y++;
		}else
			/* 没有越界，则只需要向后移动一个位置即可 */
			cursor_x++;

		/* 最后，保持逻辑位置和实际硬件寄存器值的一致性 */
		update_cursor_reg();
	}
	
}


/*
 * 将一个默认属性（亮白色前景+黑色背景+无闪烁）字符写在光标位置处，同时光标向后移动一个位置，如果必要则会使屏幕内容
 * 滚动（上移一行）；特殊字符的处理方式和console_write_char_color一致。
 */
void console_write_char( const char ch )
{
	console_write_char_color( ch, VGA_COLOR_LIGHT_WHITE, VGA_COLOR_BLACK );
}


/*
 * 移动光标到指定位置。所给定的位置参数必须是在屏幕范围之内，即在(0,0)-(VGA_HEIGHT-1,VGA_HEIGHT-1)之内，
 * 否则将被忽略。
 */
void console_move_cursor( const uint8_t row, const uint8_t col )
{
	if( row <= VGA_HEIGHT - 1 && col <= VGA_WIDTH - 1 ) //由于row/col都是uint8_t类型，所以一定大于等于0
	{
		cursor_x = col;
		cursor_y = row;
		update_cursor_reg();
	}
}

/*
 * 获取当前光标位置，结果从参数返回。
 */
void console_get_cursor_pos( uint8_t * row, uint8_t * col )
{
	*row = cursor_y;
	*col = cursor_x;
}


/*
 * 输出一个带有指定属性（前景色+背景色+是否闪烁）且以NUL字符结尾的字符串到屏幕上。
 */
void console_write_string_color( const char * string, const vga_color_t fg, const vga_color_t bg )
{
	while( *string )
	{
		console_write_char_color( *string, fg, bg );
		string++;
	}
}

/*
 * 输出一个带有默认属性（白色前景+黑色背景+不闪烁）且以NUL字符结尾的字符串到屏幕上。
 */
void console_write_string( const char * string )
{
	while( *string )
	{
		console_write_char( *string );
		string++;
	}
}

/*
 * 设置vga buffer的起始地址，默认为0xB8000
 */
void set_vga_memory_addr(uint32_t addr)
{
	vga_memory = (uint16_t *)addr;
}
