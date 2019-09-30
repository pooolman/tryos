#ifndef _INCLUDE_FILE_H_
#define _INCLUDE_FILE_H_

#include <stdint.h>
#include "inode.h"

m_inode_t * create(m_inode_t * dp, uint16_t type, const char * name);

#endif //_INCLUDE_FILE_H_
