#ifndef _INCLUDE_FILE_H_
#define _INCLUDE_FILE_H_

#include <stdint.h>
#include "fs.h"
#include "stat.h"
#include "pipe.h"

/* 打开文件结构所能表示的资源类型 */
#define FD_TYPE_INODE 1
#define FD_TYPE_PIPE 2

/* 打开文件结构定义 */
typedef struct {
	int32_t type; //所表示资源类型
	uint16_t ref; //有多少个文件描述符引用该结构
	uint32_t mode; //所关联的inode的操作模式（读/写等）
	uint32_t off; //文件偏移量，针对目录和普通文件
	mem_inode_t * ip; //所关联的inode
	pipe_t * pipe; //所关联的pipe对象
} file_t;

file_t * alloc_file(void);

int32_t alloc_fd(file_t * fp);

void close_file(file_t * fp);

file_t * dup_file(file_t * fp);

int32_t read_file(file_t * fp, void * buf, uint32_t n);

int32_t write_file(file_t * fp, void * buf, uint32_t n);

int32_t stat_file(file_t * fp, stat_t * st);

/* DEBUG */
void dump_file(file_t * fp);

#endif //_INCLUDE_FILE_H_
