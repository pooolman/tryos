#include <stdint.h>
#include <stddef.h>

#include "sys.h"
#include "ulib.h"
#include "fcntl.h"
#include "string.h"
#include "stat.h"
#include "fs.h"
#include "parameters.h"

static char * tty_path = "/tty";
static char * sh_path = "/sh";

int32_t main(void)
{
	int32_t sh_pid;
	int32_t retval;
	int32_t pid;
	char * sh_argv[] = {sh_path, NULL};

	if(open(tty_path, O_RDWR) < 0) //0 作为stdin
	{
		mknod(tty_path, CHR_DEV_INODE, TTY_MAJOR_NUM, TTY_MINOR_NUM);
		open(tty_path, O_RDWR);
	}

	dup(0); //1 作为stdout
	dup(0); //2 作为stderr

	if((sh_pid = fork()) < 0)
	{
		printf("uinit: fork failed\n");
		goto bad;
	}
	if(sh_pid == 0)
	{
		//子进程
		exec(sh_path, sh_argv);
		printf("uinit child: exec shell failed\n");
		goto bad;
	}
	
	/* 等待所有子进程退出 */
	while((pid = _wait(&retval)) > 0)
	{
		if(pid != sh_pid)
			printf("uinit: zombie %d exit with %d\n", pid, retval);
		else
			printf("uinit: shell exit with %d\n", retval);
	}

bad:
	while(1);
}
