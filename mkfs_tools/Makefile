CC = /usr/bin/gcc
C_FLAGS = -std=c99 -Wall

MKFS_TARGET = mkfs.tryos.2
FSCK_TARGET = fsck.tryos.2

.PHONY : all
all : clean mkfs fsck

mkfs :
	$(CC) $(C_FLAGS) mkfs.c lib.c block.c inode.c path.c file.c -o $(MKFS_TARGET)

fsck :
	$(CC) $(C_FLAGS) fsck.c lib.c block.c inode.c path.c file.c -o $(FSCK_TARGET)


.PHONY : clean_mkfs
clean_mkfs :
	-rm $(MKFS_TARGET)

.PHONY : clean_fsck
clean_fsck :
	-rm $(FSCK_TARGET)

.PHONY : clean
clean :
	-rm $(MKFS_TARGET) $(FSCK_TARGET)
