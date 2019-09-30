#ifndef _INCLUDE_LIB_H_
#define _INCLUDE_LIB_H_

#include <stdint.h>
#include "../tryos/include/fs.h"

int32_t read_sb(int fd, super_block_t * sb);

int32_t raw_read(int fd, uint32_t snum, void * buf, size_t size);

int32_t raw_write(int fd, uint32_t snum, void * buf, size_t size);

/* 返回a,b中数值最小的那个，用于连续的读取/写入i节点函数 */
#define min(a, b) ((a) > (b) ? (b) : (a))

#endif
