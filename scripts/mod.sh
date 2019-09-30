# This script is used to dump binary information of input file to screen
#	EXAMPLE:
#	$ mod.sh INPUT_FILE 0 512
#	000000 e9 19 00 48 07 65 07 6c 07 6c 07 6f 07 20 07 42  >...H.e.l.l.o. .B<
#	000010 07 6f 07 63 08 68 07 73 07 21 07 00 e9 fd ff 00  >.o.c.h.s.!......<
#	000020 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  >................<
#	*
#	0001f0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 55 aa  >..............U.<
#	000200



# 用法：./mod.sh 输入文件 起始偏移量 字节数

#!/bin/bash
od  -A x -t xCz $1 -j $2 -N $3

