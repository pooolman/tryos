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
char buf[1024];
int match(char*, char*);

void grep(char *pattern, int fd)
{
	int n, m;
	char *p, *q;
    
	m = 0;
        while((n = read(fd, buf+m, sizeof(buf)-m)) > 0)
	{
		m += n;
	        p = buf;
	 	while((q = strchr(p, '\n')) != NULL)
		{
			*q = 0;
		        if(match(pattern, p))
			{
				 *q = '\n';
				 write(FD_STDOUT, p, q+1 - p);
			}
			p = q+1;
		 }
		if(p == buf)
		m = 0;
		if(m > 0)
		{
			m -= p - buf;
		        memmove(buf, p, m);
		}
	}
}

int main(int argc, char *argv[])
{
	int fd, i;
	char *pattern;
   
	if(argc <= 1)
	{
		printf("usage: grep pattern [file ...]\n");
		return -1;
	}
	pattern = argv[1];

	if(argc <= 2)
	{
	      grep(pattern, FD_STDIN);
	      return 0;
	}

	for(i = 2; i < argc; i++)
	{
		if((fd = open(argv[i], O_RDONLY)) < 0)
		{
			printf("grep: cannot open %s\n", argv[i]);
			return -1;
		}
		grep(pattern, fd);
		close(fd);
	}
	return 0;
}

// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9.

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
	if(re[0] == '^')
		return matchhere(re+1, text);
	do{  // must look at empty string
		if(matchhere(re, text))
			return 1;
	}while(*text++ != '\0');
	return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
	if(re[0] == '\0')
		return 1;
	if(re[1] == '*')
		return matchstar(re[0], re+2, text);
	if(re[0] == '$' && re[1] == '\0')
		return *text == '\0';
	if(*text!='\0' && (re[0]=='.' || re[0]==*text))
		return matchhere(re+1, text+1);
	return 0;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
	do{  // a * matches zero or more instances
		if(matchhere(re, text))
			return 1;
	}while(*text!='\0' && (*text++==c || c=='.'));
	return 0;
}

