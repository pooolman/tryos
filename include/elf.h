/*
 * ELF32 文件格式，这里仅列出一部分内容
 */

#ifndef _INCLUDE_ELF_H_
#define _INCLUDE_ELF_H_

#include <stdint.h>

#define ELF_MAGIC 0x464C457F  // "\x7FELF" in little endian

// elf[12] indentification indexes
#define ELF_EI_CLASS	(4 - 4)

// elf file class or capacity
#define ELF_CLASSNONE	0x0
#define ELF_CLASS32	0x1
#define ELF_CLASS64	0x2


// values for ELF file type
#define ELF_ET_NONE	0x00
#define ELF_ET_REL 	0x01
#define ELF_ET_EXEC	0x02
#define ELF_ET_DYN 	0x03
#define ELF_ET_CORE	0x04
#define ELF_ET_LOPROC	0xff00
#define ELF_ET_HIPROC	0xffff


// File header
typedef struct {
  uint32_t magic;  // must equal ELF_MAGIC
  uint8_t elf[12];
  uint16_t type;
  uint16_t machine;
  uint32_t version; 
  uint32_t entry;
  uint32_t phoff;
  uint32_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elfhdr_t;

// Program section header
typedef struct {
  uint32_t type;
  uint32_t off;
  uint32_t vaddr;
  uint32_t paddr;
  uint32_t filesz;
  uint32_t memsz;
  uint32_t flags;
  uint32_t align;
} proghdr_t;

// Values for Proghdr type
#define ELF_PT_LOAD	1

#endif //_INCLUDE_ELF_H_
