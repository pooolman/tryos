#ifndef _INCLUDE_PATH_H_
#define _INCLUDE_PATH_H_

#include <stdint.h>
#include "fs.h"

mem_inode_t * lookup_dir(mem_inode_t * dp, const char * name, uint32_t * poff);

int32_t add_link(mem_inode_t * dp, const char * name, uint32_t inum);

int32_t get_last_element(const char * path, char * elem);

mem_inode_t * resolve_path(const char * path, int32_t stop_at_parent, char * name);

int32_t is_empty_dir(mem_inode_t * dp);
#endif //_INCLUDE_PATH_H_
