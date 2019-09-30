#ifndef _INCLUDE_INODE_H_
#define _INCLUDE_INODE_H_

#include <stdint.h>
#include "fs.h"
#include "stat.h"
#include "parameters.h"


/* 字符设备表表项结构 */
typedef struct {
	int32_t ( * read)(void * buf, uint32_t n);
	int32_t ( * write)(void * buf, uint32_t n);
} chr_dev_opts_t;

extern chr_dev_opts_t chr_dev_opts_table[CHR_DEV_COUNT];

mem_inode_t * acquire_inode(int32_t dev, uint32_t inum);

mem_inode_t * alloc_inode(int32_t dev);

void lock_inode(mem_inode_t * ip);

void update_inode(mem_inode_t * ip);

void unlock_inode(mem_inode_t * ip);

void trunc_inode(mem_inode_t * ip);

void release_inode(mem_inode_t * ip);

mem_inode_t * dup_inode(mem_inode_t * ip);

int32_t read_inode(mem_inode_t * ip, void * dst, uint32_t off, uint32_t n);

int32_t write_inode(mem_inode_t * ip, void * src, uint32_t off, uint32_t n);

void stat_inode(mem_inode_t * ip, stat_t * st);

/* DEBUG */
void dump_inode(mem_inode_t * ip);
//uint32_t get_inode_map(mem_inode_t * ip, uint32_t n);

#endif //_INCLUDE_INODE_H_
