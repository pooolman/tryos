S_SOURCES = $(shell find . -name "*.asm" ! -path "./usr/*")
S_OBJECTS = $(patsubst %.asm,%.asm.o,$(S_SOURCES))
C_SOURCES = $(shell find . -name "*.c" ! -path "./usr/*")
C_OBJECTS = $(patsubst %.c,%.c.o,$(C_SOURCES))

CC = ~/opt/cross/bin/i686-elf-gcc
OBJDUMP = ~/opt/cross/bin/i686-elf-objdump
C_FLAGS = -std=gnu99 -ffreestanding -Wall -Wextra -I ./include/

OS_IMG = tryos.bin
OS_IMG_DASM = tryos.bin.dasm
DISK = bochs_disk_64MB.img

#这里没有给出目标文件的依赖关系，所以目前，如果修改了某个源文件或者头文件，则必须先make clean后再次make

.PHONY : all
all : clean generate link update_img dasm_img

link : $(C_OBJECTS) $(S_OBJECTS)
	$(CC) -T scripts/linker.ld -o $(OS_IMG) -ffreestanding -O2 -nostdlib $(S_OBJECTS) $(C_OBJECTS) -lgcc



$(C_OBJECTS) :
	$(CC) -c $(patsubst %.c.o,%.c,$@) $(C_FLAGS) -o $@
$(S_OBJECTS) :
	nasm -felf32 $(patsubst %.asm.o,%.asm,$@) -o $@



#在rm前面加上一个‘-’是要求当执行make clean时，如果在rm的过程中出现错误（例如某个文件不存在），仍然继续执行下去
.PHONY : clean
clean :
	-rm $(OS_IMG) $(OS_IMG_DASM) $(S_OBJECTS) $(C_OBJECTS)

#从C头文件中生成一些用于汇编的源文件
#示例：
# input:
# /* comment... */
# #define DF_INT_VECTOR 4   //comment...
# 
# output:
# %define DF_INT_VECTOR 4
.PHONY : generate
generate :
	sed -n '/^#define .*INT_VECTOR/{s/\/.*//;s/^#/%/;p}' ./include/idt.h > ./usr/inc/int_vectors.inc;\
		sed -n '/^#define .*SYS_NUM_/{s/\/.*//;s/^#/%/;p}' ./include/syscall.h > ./usr/inc/syscall_numbers.inc


.PHONY : update_img
update_img :
	sudo mount dd_floppy.img ./mount_point -o loop;\
		cp $(OS_IMG) ./mount_point;\
		sleep 2;\
		sudo umount ./mount_point

.PHONY : mount_img
mount_img :
	sudo mount dd_floppy.img ./mount_point -o loop

.PHONY : umount_img
umount_img :
	sudo umount ./mount_point

.PHONY : dasm_img
dasm_img :
	$(OBJDUMP) -M intel -d $(OS_IMG) > $(OS_IMG_DASM)

.PHONY : disk
disk :
	-rm $(DISK);\
		bximage -hd=64M -imgmode=flat -mode=create -q $(DISK)
