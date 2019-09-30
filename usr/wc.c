#include <stdint.h>
#include <stddef.h>

#include "sys.h"
#include "ulib.h"
#include "fcntl.h"
#include "string.h"
#include "stat.h"
#include "fs.h"
#include "parameters.h"

/* 本文件修改自xv6 */
char buf[512];

int wc(int fd, char *name)
{
	int i, n;
	int l, w, c, inword;

	l = w = c = 0;
	inword = 0;
	while((n = read(fd, buf, sizeof(buf))) > 0){
		for(i=0; i<n; i++){
			c++;
			if(buf[i] == '\n')
				l++;
			if(strchr(" \r\t\n\v", buf[i]))
				inword = 0;
			else if(!inword){
				w++;
				inword = 1;
			}
		}
	}
	/*
	if(n < 0){
		return -1;
	}
	*/
	printf("%d %d %d %s\n", l, w, c, name);
	return 0;
}

int main(int argc, char *argv[])
{
	int fd, i;

	if(argc <= 1){
		if(wc(FD_STDIN, "stdin") == 0)
			return 0;
		else
		{
			printf("wc: read from stdin error\n");
			return -1;
		}
	}

	for(i = 1; i < argc; i++){
		if((fd = open(argv[i], O_RDONLY)) < 0){
			printf("wc: cannot open `%s'\n", argv[i]);
			return -1;
		}
		if(wc(fd, argv[i]) != 0)
		{
			close(fd);
			printf("wc: read `%s' error\n", argv[i]);
			return -1;
		}
	}
	return 0;
}
