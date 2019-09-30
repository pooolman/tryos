#ifndef _INCLUDE_MULTIBOOT_H_
#define _INCLUDE_MULTIBOOT_H_

#include <stdint.h>

/* Multiboot information struct. For its usage, please refer Multiboot spec 0.6.96 */
/* 注意：在开启分页模式之后，当使用该结构体中的指针时，要考虑是否需要加上虚拟地址偏移量，
 * 因为这些指针值是GRUB使用未开启分页模式之前的地址设置的*/
typedef struct{
	/* Multiboot info version number */
	uint32_t flags;
	/* Available memory from BIOS */
	uint32_t mem_lower;
	uint32_t mem_upper;

	/* "root" partition */
	uint32_t boot_device;

	/* Kernel command line */
	uint32_t cmdline;

	/* Boot-Module list */
	uint32_t mods_count;
	uint32_t mods_addr;

	/* ELF section header table */
	uint32_t num;
	uint32_t size;
	uint32_t addr;
	uint32_t shndx;
	/* Memory Mapping buffer */
	uint32_t mmap_length;
	uint32_t mmap_addr;

	/* Drive Info buffer */
	uint32_t drives_length;
	uint32_t drives_addr;

	/* ROM configuration table */
	uint32_t config_table;

	/* Boot Loader Name */
	uint32_t boot_loader_name;

	/* APM table */
	uint32_t apm_table;

	/* Video */
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;

	/* There is no framebuffer member, currently is no supported,
	 * although it is specified in Multiboot specification 0.6.96.
	 */

}__attribute__((packed)) multiboot_info_t;

/* All other value for type member indicates reserved memory region.
 * Note: I assume the 'type' member occupies 4 bytes, but this is not specifed in Multiboot spec 0.6.96 */
#define MULTIBOOT_MEMORY_AVAILABLE              1
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE       3
#define MULTIBOOT_MEMORY_PRESERVE_ON_HIBERNATION 4
#define MULTIBOOT_MEMORY_BADRAM                 5

/* Struct definition for memory map information */
typedef struct{
	uint32_t size;
	uint32_t addr_low;
	uint32_t addr_high;
	uint32_t len_low;
	uint32_t len_high;
	uint32_t type;
}__attribute__((packed)) mmap_entry_t;

/* This variable is defined in init.c and is externed to any where this header file is included */
extern multiboot_info_t * glb_mbi;


void show_memory_map(void);
int32_t mem_validate(uint32_t paddr);

#endif  //_INCLUDE_MULTIBOOT_H_
