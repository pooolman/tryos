#include <stdint.h>
#include <stddef.h>

#include "sys.h"
#include "ulib.h"
#include "fcntl.h"
#include "string.h"
#include "stat.h"
#include "fs.h"
#include "parameters.h"
#include "limits.h"

/* 命令选项 */
#define FL_IGNORE_DIR	0x1

/*
 * 输出name和与之关联的stat结构信息
 */
static void show_info(char * name, stat_t * st)
{
	printf("%d %u\t", st->dev, st->inum);
	switch(st->type)
	{
		case CHR_DEV_INODE:
			printf("chr\t");
			break;
		case FILE_INODE:
			printf("fil\t");
			break;
		case DIR_INODE:
			printf("dir\t");
			break;
		case BLK_DEV_INODE:
			printf("blk\t");
			break;
		default:
			printf("ukn\t");
	}
	printf("(%hu,%hu) %hu %u\t", 
			st->major,
			st->minor,
			st->link_number,
			st->size);
	printf("%s\n", name);
}

/*
 * 输出与指定路径相关的信息
 * path: 指定输出该路径所表示的文件信息；
 * flags: 控制参数；
 * 成功返回0，输出文件信息；失败返回-1，并输出错误信息；
 */
static int32_t show_path(char * path, int32_t flags)
{
	int32_t ret;
	int32_t fd;
	int32_t subent_fd;
	stat_t st;
	dirent_t dirent;
	int32_t size;

	static char buf[PATH_MAX_LEN + 1] = {0};

	if((fd = open(path, O_RDONLY)) < 0)
	{
		printf("show_path: open `%s' failed\n", path);
		return -1;
	}
	if(fstat(fd, &st) < 0)
	{
		printf("show_path: stat `%s' failed\n", path);
		return -1;
	}

	switch(st.type)
	{
		case CHR_DEV_INODE:
		case BLK_DEV_INODE:
		case FILE_INODE:
			show_info(path, &st);
			return 0;
		case DIR_INODE:
			if(flags & FL_IGNORE_DIR)
			{
				show_info(path, &st);
				return 0;
			}
			ret = 0;
			printf("%s:\n", path);
			while((size = read(fd, &dirent, sizeof(dirent))) == sizeof(dirent))
			{
				dirent.name[MAX_DIR_NAME_LEN - 1] =  '\0'; //强制以NUL字符结尾 :(
				if(strlen(path) + strlen(dirent.name) + 1 > PATH_MAX_LEN)
				{
					printf("show_info: `%s' too long\n", dirent.name);
					ret = -1;
					continue;
				}
				strncpy(buf, path, strlen(path));
				strncat(buf, "/", 1);
				strncat(buf, dirent.name, strlen(dirent.name));
				
				if((subent_fd = open(buf, O_RDONLY)) < 0)
				{
					printf("show_info: `%s' open failed\n", buf);
					ret = -1;
					continue;
				}
				if(fstat(subent_fd, &st) < 0)
				{
					printf("show_info: stat `%s' failed\n", buf);
					ret = -1;
					continue;
				}
				
				show_info(dirent.name, &st);
				close(subent_fd);
			}
			if(size != 0)
			{
				printf("show_info: cannot read directory `%s'\n", path);
				ret = -1;
			}
			printf("\n");
			return ret;
		default:
			printf("show_info: unkonwn file type `%s'\n", path);
			return -1;
	}
}


/*
 * 输出指定文件的信息：
 * 	cmd [-d] [path] ...
 * 当没有指定path时，等同于执行"cmd ."；
 * 当指定一个或多个path时，则按照path的顺序依次处理：
 * 如果path是目录，且指定了-d选项，则显示该目录本身的信息，
 * 没有指定则显示该目录下的文件信息，且不递归处理；
 * 如果path不是目录，则显示该path指定文件的信息；
 * 所有的path都将处理完而不管其中的失败情况。
 *
 * 成功执行则以0退出，否则以-1退出；
 */
int32_t main(int argc, char * argv[])
{
	int32_t flags;
	int32_t ret;
	int32_t i;

	flags = 0;
	if(argc == 1)
	{
		/* 没有指定任何参数 */
		return show_path(".", flags);
	}
	else
	{
		/* 第一个参数是不是以-d开头 */
		if(strncmp(argv[1], "-d", 2) == 0)
		{
			flags |= FL_IGNORE_DIR;
			i = 2;
		}
		else
			i = 1;
	}
	
	if(i == argc) //仅指定了-d
		return show_path(".", flags);
	else
	{
		while(i < argc)
		{
			if(show_path(argv[i], flags) == -1)
				ret = -1;
			i++;
		}
		return ret;
	}
}
