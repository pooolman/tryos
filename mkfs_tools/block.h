#ifndef _INCLUDE_BLOCK_H_
#define _INCLUDE_BLOCK_H_

#include <stdint.h>
#include "../tryos/include/fs.h"

uint32_t alloc_block(int fd, super_block_t * sb);

int32_t free_block(int fd, super_block_t * sb, uint32_t bnum);

int32_t zero_block(int fd, super_block_t * sb, uint32_t bnum);

/* 不同于 NAVL_BLK_NUM，这个常量用来鉴别alloc_block的执行结果；在make_fs之后，0号block也被标记为无效的 */
#define _NAVL_BLK_NUM_	0xFFFFFFFF

#endif //_INCLUDE_BLOCK_H_
