#固定的依赖文件
S_DEP_SRCS = lib/sys_ent.asm lib/user_ent.asm
S_DEP_OBJS = $(patsubst %.asm,%.asm.o,$(S_DEP_SRCS))

C_DEP_SRCS = ../libs/string.c ./lib/ulib.c
C_DEP_OBJS = $(patsubst %.c,%.c.o,$(C_DEP_SRCS))


#需要编译的目标源文件，可以有多个，空格分开
C_TGT_SRCS = ./uinit.c ./sh.c ./echo.c ./ls.c ./cat.c ./grep.c ./mkdir.c ./link.c ./unlink.c ./wc.c
C_TGT_OBJS = $(patsubst %.c,%.c.o,$(C_TGT_SRCS))
#指定生成的目标
C_TGTS = $(patsubst %.c,%,$(C_TGT_SRCS))
C_TGTS_DASM = $(patsubst %.c,%.dasm,$(C_TGT_SRCS))

CC = ~/opt/cross/bin/i686-elf-gcc
OBJDUMP = ~/opt/cross/bin/i686-elf-objdump
C_FLAGS = -std=gnu99 -ffreestanding -Wall -Wextra -I ../include -I ./inc
MKFS_TOOL = ../scripts/mkfs.tryos
FS_IMG = ../bochs_disk_64MB.img


#先清除之前生成的目标文件和中间文件，再编译
.PHONY : all
all : clean link dasm

.PHONY : link
link : $(S_DEP_OBJS) $(C_DEP_OBJS) $(C_TGT_OBJS) $(C_TGTS)

.PHONY : clean
clean :
	-rm $(C_DEP_OBJS) $(S_DEP_OBJS) $(C_TGT_OBJS) $(C_TGTS) $(C_TGTS_DASM)

.PHONY : dasm
dasm : $(C_TGTS_DASM)

.PHONY : mkfs
mkfs :
	$(MKFS_TOOL) $(FS_IMG) $(C_TGTS)

$(C_DEP_OBJS) :
	$(CC) -c $(patsubst %.c.o,%.c,$@) $(C_FLAGS) -o $@
$(S_DEP_OBJS) :
	nasm -felf32 $(patsubst %.asm.o,%.asm,$@) -o $@
$(C_TGT_OBJS) :
	$(CC) -c $(patsubst %.c.o,%.c,$@) $(C_FLAGS) -o $@
$(C_TGTS) : 
	$(CC) -T scripts/ulinker.ld -o $@ -ffreestanding -nostdlib $@.c.o $(C_DEP_OBJS) $(S_DEP_OBJS) -lgcc
$(C_TGTS_DASM) :
	$(OBJDUMP) -d -M intel $(patsubst %.dasm,%,$@) > $@

