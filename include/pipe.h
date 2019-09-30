#ifndef _INCLUDE_PIPE_H_
#define _INCLUDE_PIPE_H_

#include <stdint.h>
#include "parameters.h"
//注释这条，避免交叉引用 :(
//#include "file.h"

/* 管道结构定义 */
typedef struct {
	uint8_t buf[PIPE_BUF_SIZE]; //环形缓冲区，用于保存管道数据
	uint32_t ridx; //读索引，读取从ridx%sizeof(buf)处开始
	uint32_t widx; //写索引，写入从widx%sizeof(buf)处开始
	int32_t ropen; //非0表示仍然有用于读取的文件描述符与该管道关联，否则表示该管道不再可读
	int32_t wopen; //非0表示仍然有用于写入的文件描述符与该管道关联，否则表示该管道不再可写
} pipe_t;


//注释这条，避免交叉引用 :(
//int32_t create_pipe(_file_t ** prfile, _file_t ** pwfile);
void close_pipe(pipe_t * pp, uint32_t port_type);
int32_t read_pipe(pipe_t * pp, void * buf, uint32_t n);
int32_t write_pipe(pipe_t * pp, void * buf, uint32_t n);

#endif //_INCLUDE_PIPE_H_
