#ifndef _INCLUDE_STAT_H_
#define _INCLUDE_STAT_H_

#include <stdint.h>

/* 可以被用户进程检索的i节点信息 */
typedef struct{
	int32_t dev;	//i节点所在设备
	uint32_t inum; //i节点编号
	uint16_t type; //i节点所表示的资源类型
	uint16_t major; //主/从设备号，当i节点表示资源为设备时有效
	uint16_t minor;
	uint16_t link_number; //指向该inode的目录项数
	uint32_t size; //文件大小，字节单位
} stat_t;

#endif //_INCLUDE_STAT_H_
