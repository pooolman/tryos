#ifndef _INCLUDE_PATH_H_
#define _INCLUDE_PATH_H_

#include <stdint.h>
#include "inode.h"

int32_t lookup_dir(m_inode_t * dp, const char * name, m_inode_t ** ipp);

int32_t add_link(m_inode_t * dp, const char * name, uint32_t inum);

char * get_element(const char * path, char * elem, int32_t * last_elem);

int32_t resolve_path(const char * path, m_inode_t ** ipp, int32_t stop_at_parent, char * name);

#endif //_INCLUDE_PATH_H_
