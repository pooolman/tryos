#include <stdint.h>
#include <stddef.h>

#include "sys.h"
#include "ulib.h"
#include "fcntl.h"
#include "string.h"
#include "stat.h"
#include "fs.h"
#include "parameters.h"

/* 一行所能输入的最多字符个数 */
#define LINE_SIZE	128

/*
 * 从终端输入的行中分割出下一个词组
 *
 * pstr: 指向待处理字符串的指针的地址；
 * 词组是连续的非空白字符组成的字符串；
 * 返回词组起始地址，且将*pstr偏移到下一次开始处理的位置；
 * 如果没有词组则返回NULL，*pstr的值不被改变；
 */
char * split_phase(char ** pstr)
{
	char * str;
	char * phase;
	
	str = *pstr;

	/* 跳过开头的空白字符 */
	while(*str == ' ' || *str == '\t' ||
			*str == '\n' || *str == '\0')
	{
		if(*str == '\n' || *str == '\0')
			return NULL;
		str++;
	}
	/* 词组开始位置 */
	phase = str++;
	/* 跳过词组 */
	while(*str != ' ' && *str != '\t' && 
			*str != '\n' && *str != '\0')
		str++;
	
	/* 到行末尾了? */
	if(*str == '\n' || *str == '\0')
	{
		*str = '\0';
		/* 指向下一次处理的位置 */
		*pstr = str;
		return phase;
	}
	
	/* 使词组构成一个字符串 */
	*str++ = '\0';
	/* 指向下一次处理的位置 */
	*pstr = str;
	
	return phase;
}

/*
 * 执行外部命令。
 * argc: argv中词组的个数
 * argv: 待执行的命令序列
 *
 * 支持"<" ">" "|"的组合，这三个符号和其他符号之间必须有空白字符；
 * 对于错误检查不完善(没有kill系统调用)，所以应尽量避免语法出错
 * 成功执行返回0，失败返回-1
 */
#define FL_PIPE_REDIR		0x1
#define FL_STDIN_REDIR		0x2
#define FL_STDOUT_REDIR		0x4
int32_t run_cmd(int32_t argc, char * argv[])
{
	int32_t flags;
	int32_t i;
	int32_t pid;
	int32_t cpid;
	char * stdin_redir_file_pos;
	char * stdout_redir_file_pos;
	char * cmd_argv[MAX_ARG_NUM] = {0};
	int32_t pfd[2] = {0};
	int32_t retval;
	
	flags = 0;
	i = 0;

	/* 解析argv，为执行做准备 */
	while(i < argc && !(flags & FL_PIPE_REDIR))
	{
		if(strncmp(argv[i], "<", 1) == 0)
		{
			flags |= FL_STDIN_REDIR;
			stdin_redir_file_pos = argv[i+1];
			i++; //跳过后面的文件名
		}
		else if(strncmp(argv[i], ">", 1) == 0)
		{
			flags |= FL_STDOUT_REDIR;
			stdout_redir_file_pos = argv[i+1];
			i++; //跳过后面的文件名
		}
		else if(strncmp(argv[i], "|", 1) == 0)
		{
			flags |= FL_PIPE_REDIR;
			pipe(pfd);
			if((cpid = fork()) == 0)
			{
				/* 子进程中 */
				close(FD_STDIN);
				dup(pfd[0]);
				close(pfd[0]);
				close(pfd[1]);
				
				/* 在子进程中继续处理后续的argv */
				/* 不能从调用栈返回，直接结束 */
				if(run_cmd(argc - (i + 1), &argv[i+1]) < 0)
					exit(-1);
				else exit(0);
			}
			else if(cpid < 0)
			{
				printf("run_cmd: fork failed\n");
				return -1;
			}
		}
		else
		{
			/* 准备执行的参数 */
			cmd_argv[i] = argv[i];
		}
		i++;
	}

	/* 执行命令 */
	if((pid = fork()) == 0)
	{
		/* 子进程 */
		if(flags & FL_STDIN_REDIR)
		{
			close(FD_STDIN);
			if(open(stdin_redir_file_pos, O_RDONLY) != FD_STDIN)
			{
				printf("run_cmd: open `%s' on FD_STDIN failed\n", stdin_redir_file_pos);
				return -1;
			}
		}
		if(flags & FL_STDOUT_REDIR)
		{
			close(FD_STDOUT);
			/* 如果不存在则需要创建该文件 */
			if(open(stdout_redir_file_pos, O_CREAT| O_WRONLY) != FD_STDOUT)
			{
				printf("run_cmd: open `%s' on FD_STDOUT failed\n", stdout_redir_file_pos);
				return -1;
			}
		}
		
		/* 目前没有处理"|"和"<"/">"冲突的情况 */
		if(flags & FL_PIPE_REDIR)
		{
			close(FD_STDOUT);
			dup(pfd[1]);
			close(pfd[0]);
			close(pfd[1]);
		}
		exec(cmd_argv[0], cmd_argv);
		printf("run_cmd: exec `%s' failed\n", cmd_argv[0]);
		/* 不能从调用栈返回，应该直接退出 */
		exit(-1);
	}
	else if(pid < 0)
	{
		printf("run_cmd: fork failed\n");
		return -1;
	}
	
	/* 清理资源，等待子进程退出 */
	if(flags & FL_PIPE_REDIR)
	{
		/* 之前创建了管道 */
		close(pfd[0]);
		close(pfd[1]);
	}
	while((pid = _wait(&retval)) > 0)
	{
		printf("run_cmd: proc %d exit with %d\n", pid, retval);
	}
	return 0;
}



int32_t main(void)
{
	int32_t cmd_argc;
	char * cmd_argv[MAX_ARG_NUM];
	char line_buf[LINE_SIZE + 1];
	char * str;
	int32_t pid;
	int32_t retval;


	while(1)
	{
		str = line_buf;

		/* 获取并解析命令 */
		printf("# ");
		if(gets(FD_STDIN, line_buf, LINE_SIZE + 1) <= 0)
		{
			printf("sh: no input data or read failed\n");
			continue;
		}
		if((cmd_argv[0] = split_phase(&str)) == NULL)
		{
			printf("sh: no cmd specified\n");
			continue;
		}
		for(cmd_argc = 1; cmd_argc < MAX_ARG_NUM; cmd_argc++)
			if((cmd_argv[cmd_argc] = split_phase(&str)) == NULL)
				break;
		
		/* 执行命令 */
		if(strncmp(cmd_argv[0], "cd", 2) == 0)
		{
			if(cmd_argc == 1)
			{
				if(chdir("/") < 0)
				{
					printf("sh: change to `/' failed\n");
					goto waiting;
				}
			}
			else
			{
				if(chdir(cmd_argv[1]) < 0)
				{
					printf("sh: change to `%s' failed\n", cmd_argv[1]);
					goto waiting;
				}
			}
		}
		else if(strncmp(cmd_argv[0], "exit", 4) == 0)
		{
			return 0;
		}
		else
		{
			/*
			if((pid = fork()) == 0)
			{
				exec(cmd_argv[0], cmd_argv);
				printf("sh: exec `%s' failed\n", cmd_argv[0]);
				return -1; //子进程退出
			}
			if(pid < 0)
			{
				printf("sh: fork failed\n");
				goto waiting;
			}
			*/
			if(run_cmd(cmd_argc, cmd_argv) < 0)
				printf("sh: run_cmd failed\n");
		}
	waiting:
		while((pid = _wait(&retval)) > 0)
			printf("sh: proc %d exit with %d\n", pid, retval);
	}
	return 0;
}


