#ifndef _INCLUDE_BLOCK_H_

#include <stdint.h>
#include "fs.h"

void read_sb(int32_t dev, super_block_t * sb);
uint32_t alloc_block(int32_t dev);
void free_block(int32_t dev, uint32_t bnum);

#endif //_INCLUDE_BLOCK_H_
