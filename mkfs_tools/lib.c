#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../tryos/include/fs.h"
#include "lib.h"

/*
 * 按照sector num从文件fd中读取或者写入size个字节
 * fd必须引用一个为可读可写方式打开的文件。
 * 若成功读取/写入size个字节返回0，失败返回-1
 */
int32_t raw_read(int fd, uint32_t snum, void * buf, size_t size)
{
	if(lseek(fd, snum * BLOCK_SIZE, SEEK_SET) < 0)
		return -1;
	if(read(fd, buf, size) != size)
		return  -1;
	else return 0;
}

int32_t raw_write(int fd, uint32_t snum, void * buf, size_t size)
{
	if(lseek(fd, snum * BLOCK_SIZE, SEEK_SET) < 0)
		return -1;
	if(write(fd, buf, size) != size)
		return -1;
	else return 0;
}


/*
 * 读取super block到sb中
 * fd必须引用一个为可读可写方式打开的文件，且已经通过make_fs调用格式化过。
 * 成功返回0，失败返回-1。
 */
int32_t read_sb(int fd, super_block_t * sb)
{
	return raw_read(fd, 1, sb, sizeof(*sb));
}
