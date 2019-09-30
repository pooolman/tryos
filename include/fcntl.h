/*
 * 用户程序可使用的文件系统接口常量定义
 */
#ifndef _INCLUDE_FCNTL_H_
#define _INCLUDE_FCNTL_H_

/* 文件的操作模式定义，使用mode与MASK相与就能得知其模式 */
#define O_RDONLY	0x1
#define O_WRONLY	0x2
#define O_RDWR		0x3
#define MODE_RW_MASK	0x3

#define O_CREAT		0x4

#endif //_INCLUDE_FCNTL_H_
