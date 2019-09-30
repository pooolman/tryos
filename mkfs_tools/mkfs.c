#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "./extvars.h"
#include "../tryos/include/fs.h"
#include "./lib.h"
#include "./block.h"
#include "./inode.h"
#include "./path.h"
#include "./file.h"

/* n/m的值，如果结果不为整数，则为其向上取整后的值 */
#define UPPER_DIVIDE(n, m) ((n)%(m) ? (n)/(m) + 1 : (n)/(m))

/* 全局的fd变量/超级块结构体 */
int global_fd; //引用当前正在处理的文件
super_block_t global_sb; //当前文件系统中的超级块

/*
 * 在fd的当前偏移处构建bitmap，该bitmap占据blks_bitmap个块，其中有avl_bits个bit被置为0，其余置为1，小端字节序写入。
 * fd必须是可写的。
 * 注意：这里假设avl_bits大于(blks_bitmap-1)*BLOCK_SIZE 且小于等于blks_bitmap*BLOCK_SIZE
 * 成功返回0，失败返回-1。
 */
static int32_t build_bitmap(int fd, uint32_t blks_bitmap, uint32_t avl_bits)
{
	uint8_t sector[BLOCK_SIZE];
	memset(sector, 0, sizeof(sector));

	for(uint32_t i = 0; i < blks_bitmap - 1; i++)
		if(write(fd, sector, sizeof(sector)) != sizeof(sector))
		{
			printf("build_bitmap: write error\n");
			return -1;
		}
	if(avl_bits % BITS_PER_BLOCK != 0)
	{
		uint32_t zero_byte_count = (avl_bits % BITS_PER_BLOCK) / 8;
		for(uint32_t i = 0; i < zero_byte_count; i++)
			sector[i] = 0;
		if((avl_bits % BITS_PER_BLOCK) % 8 == 0)
			for(uint32_t i = zero_byte_count; i < BLOCK_SIZE; i++)
				sector[i] = 0xFF;
		else
		{
			uint8_t mask = 0xFF << ((avl_bits % BITS_PER_BLOCK) % 8); //从低位开始构建，和搜索方法一致
			sector[zero_byte_count] = mask;
			for(uint32_t i = zero_byte_count + 1; i < BLOCK_SIZE; i++)
				sector[i] = 0xFF;
		}
	}
	if(write(fd, sector, sizeof(sector)) != sizeof(sector))
	{
		printf("build_bitmap: write error\n");
		return -1;
	}

	return 0;
}

/*
 * 在指定文件上创建一个空的、符合格式的文件系统。
 * fd必须引用一个为可读可写方式打开的文件，且长度为512字节的倍数
 * 成功返回0，失败返回-1。
 */
int32_t make_fs(int fd)
{
	struct stat st;
	super_block_t sb;

	if(fstat(fd, &st) == -1)
	{
		printf("make_fs: stat error\n");
		return -1;
	}
	if(st.st_size % BLOCK_SIZE != 0 || st.st_size < 67 * BLOCK_SIZE)
	{
		printf("make_fs: file size(bytes) must large than or equal 67*512 and is multiple of 512\n");
		return -1;
	}
	
	/* 计算可用于分配的磁盘i节点总数 */
	sb.inode_number = (uint32_t)((st.st_size*8 - 2*8*BLOCK_SIZE)/(1 + 64 + 8*sizeof(disk_inode_t) + 512*BLOCK_SIZE));
	
	/* 计算inode bitmap占用的块数 */
	sb.blks_ibitmap = UPPER_DIVIDE(sb.inode_number, BITS_PER_BLOCK);
	
	/* 计算block bitmap占用的块数 */
	sb.blks_bbitmap = UPPER_DIVIDE(64*sb.inode_number, BITS_PER_BLOCK);

	/* 计算i节点占用的块数 */
	sb.blks_inode = UPPER_DIVIDE(sb.inode_number, INODES_PER_BLOCK);

	/* 计算可用于分配的数据块数 */
	sb.block_number = (uint32_t)(st.st_size/BLOCK_SIZE - 2 - sb.blks_ibitmap - sb.blks_bbitmap - sb.blks_inode);
	if(sb.block_number > sb.blks_bbitmap * BITS_PER_BLOCK)
	{
		uint32_t count = sb.block_number - sb.blks_bbitmap * BITS_PER_BLOCK;
		count = UPPER_DIVIDE(count, BITS_PER_BLOCK);
		
		sb.block_number -= count;
		sb.blks_bbitmap += count;
	}

	printf("blks_ibitmap: %u, blks_bbitmap: %u, blks_inode: %u, inode_number: %u, block_number: %u\n",
			(unsigned int)sb.blks_ibitmap,
			(unsigned int)sb.blks_bbitmap,
			(unsigned int)sb.blks_inode,
			(unsigned int)sb.inode_number,
			(unsigned int)sb.block_number);

	
	/* 准备写入 */
	uint8_t sector[BLOCK_SIZE];
	memset(sector, 0, sizeof(sector));
	
	/* 写入超级块 */
	memcpy(sector, &sb, sizeof(sb));
	if(raw_write(fd, 1, sector, sizeof(sector)) == -1)
	{
		printf("make_fs: write super-block error\n");
		return -1;
	}
	
	/* 偏移到第一个bitmap起始处 */
	if(lseek(fd, BLOCK_SIZE * 2, SEEK_SET) < 0)
	{
		printf("make_fs: seek to bitmap error\n");
		return -1;
	}
	/* 写入两个bitmap */
	if( ! (build_bitmap(fd, sb.blks_ibitmap, sb.inode_number) == 0 && build_bitmap(fd, sb.blks_bbitmap, sb.block_number) == 0))
	{
		printf("make_fs: built_bitmap error\n");
		return -1;
	}

	/* 0号block保留 */
	/* 第一次分配，成功时必定分配0号block */
	if(alloc_block(fd, &sb) != 0 )
	{
		printf("make_fs: can not set block 0 to reserved\n");
		return -1;
	}

	return 0;

}

/*
 * 将path指定文件复制到目录dp下，其文件名与path中给出的文件名相同
 * 成功返回0，失败返回-1。
 * 
 * 注意：
 * path中给出的文件名长度必须不超过MAX_DIR_NAME_LEN，超出部分被忽略
 * dp下不能存在同名文件，否则操作失败。
 */
static int copy_file(m_inode_t * dp, const char * path)
{
	int fd;
	char elem[MAX_DIR_NAME_LEN];
	int32_t last_elem = 0;

	m_inode_t * ip;

	uint32_t off;
	int32_t size;
	char buf[1024];

	/* 只读打开，准备复制 */
	if((fd = open(path, O_RDONLY)) == -1)
	{
		printf("copy_file: open file failed\n");
		return -1;
	}
	
	/* 获取文件名 */
	while((path = get_element(path, elem, &last_elem)) != NULL)
		continue;
	if(last_elem == 0)
	{
		printf("copy_file: get last element of path failed\n");
		return -1;
	}


	/* 创建文件 */
	if((ip = create(dp, FILE_INODE, elem)) == NULL)
	{
		printf("copy_file: create new file failed\n");
		return -1;
	}

	/* 复制文件内容 */
	off = 0;
	while((size = read(fd, buf, sizeof(buf))) > 0)
	{
		if(write_inode(ip, buf, off, (uint32_t)size) != size)
		{
			printf("copy_file: write to new file failed\n");
			return -1;
		}
		off += (uint32_t)size;
	}
	if(size == -1)
	{
		printf("copy_file: read file failed\n");
		return -1;
	}

	/* 操作完成 */
	close(fd);
	free(ip);
	return 0;
}



/*
 * 格式化fs_img为文件系统，默认在其中创建一个根目录，如果提供更多的文件(file1/file2...)，
 * 则将这些文件作为普通文件复制到文件系统的根目录下，其文件名与路径参数中指定的文件名相同。
 *
 * 用法：./cmd fs_img [file1] [file2] ...
 *
 * 注意：
 * fs_img必须已经创建且长度为BLOCK_SIZE的倍数。
 * file1/file2...等文件名最大长度不超过MAX_DIR_NAME_LEN，超过部分被忽略。
 */
int main(int argc, char * argv[])
{
	m_inode_t * rip;

	if(argc < 2)
	{
		printf("main: ./cmd fs_img [file1] [file2] ...\n");
		return -1;
	}

	if((global_fd = open(argv[1], O_RDWR)) == -1)
	{
		printf("main: open %s failed\n", argv[1]);
		return -1;
	}
	
	/* 创建一个空的文件系统，并准备好fd/sb */
	if(make_fs(global_fd) == -1)
	{
		printf("main: make fs failed\n");
		return -1;
	}

	if(read_sb(global_fd, &global_sb) == -1)
	{
		printf("main: read super block failed\n");
		return -1;
	}

	/* 创建根目录 */
	if((rip = alloc_inode()) == NULL || rip->inum != 0)
	{
		printf("main: alloc inode for root dir failed\n");
		return -1;
	}
	rip->type = DIR_INODE;
	rip->link_number = 2;
	rip->size = rip->major = rip->minor = 0;
	memset(rip->addrs, 0, sizeof(rip->addrs));

	if(add_link(rip, ".", rip->inum) == -1 || add_link(rip, "..", rip->inum) == -1)
	{
		printf("main: add ./.. to root dir failed\n");
		return -1;
	}

	if(update_inode(rip) == -1)
	{
		printf("main: write root dir to filesystem failed\n");
		return -1;
	}

	
	/* 如果指定了需要复制的文件，则完成复制 */ 
	for(int i = 2; i < argc; i++)
	{
		if(copy_file(rip, argv[i]) == -1)
		{
			printf("main: copy file %s failed\n", argv[i]);
			return -1;
		}
		printf("main: copy `%s' success\n", argv[i]);
	}


	close(global_fd);
	free(rip);
	printf("main: success\n");
	return 0;
}
