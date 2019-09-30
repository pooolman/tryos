#ifndef _INCLUDE_SYS_H_
#define _INCLUDE_SYS_H_

#include <stdint.h>
#include "stat.h"

extern int32_t debug(char * str);

extern int32_t exec(char * path, char * argv[]);

extern int32_t fork(void);

extern void exit(int32_t retval);

extern int32_t _wait(int32_t * retval);

extern int32_t getpid(void);

extern int32_t dup(int32_t fd);

extern int32_t read(int32_t fd, void * buf, uint32_t n);

extern int32_t write(int32_t fd, void * buf, uint32_t n);

extern int32_t open(const char * path, uint32_t flags);

extern int32_t close(int32_t fd);

extern int32_t fstat(int32_t fd, stat_t * st);

extern int32_t link(const char * oldpath, const char * newpath);

extern int32_t unlink(const char * path);

extern int32_t mkdir(const char * path);

extern int32_t mknod(const char * path, uint32_t type, uint32_t major, uint32_t minor);

extern int32_t chdir(const char * path);

extern int32_t pipe(int32_t pfd[2]);

#endif //_INCLUDE_SYS_H_
