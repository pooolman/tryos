#ifndef _INCLUDE_BUF_CACHE_H_
#define _INCLUDE_BUF_CACHE_H_

#include <stdint.h>

/* 为避免buf_cache.h和ide.h交叉引用，将该常量放在该文件中定义 */
#define IDE_SECTOR_SIZE		512 //ide磁盘上一个扇区字节数
/* 一个buf能容纳的数据大小 */
#define BUF_SIZE		IDE_SECTOR_SIZE

/* 用于buf_t.flags中的标志 */
#define BUF_BUSY	0x1	//该buf当前被某个进程占用
#define BUF_VALID	0x2	//该buf中存在有效数据
#define BUF_DIRTY	0x4	//该buf中的数据被修改过

typedef struct _buf_t {
	int32_t		dev; //设备号，为负数时表示非可用设备，其余表示可用设备
	uint32_t	sector; //扇区号
	uint32_t	flags; //buf标志，参考demand_skeleton_analysis.txt
	uint8_t		data[BUF_SIZE]; //存储对应扇区中的数据
	struct _buf_t * qnext;	//用于设备请求队列
	struct _buf_t * prev;	//prev/next用于buf_cache中构建双向链表
	struct _buf_t * next;
} buf_t;

void init_buf_cache(void);
buf_t * acquire_buf(int32_t dst_dev, uint32_t dst_sector);
void write_buf(buf_t * buf);
void release_buf(buf_t * buf);

/* DEBUG */
void dump_buf(buf_t * buf);
void dump_buf_cache(void);

#endif //_INCLUDE_BUF_CACHE_H_
