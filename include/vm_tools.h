#ifndef _INCLUDE_VM_TOOLS_H_
#define _INCLUDE_VM_TOOLS_H_

#include <stdint.h>
#include "vmm.h"

pte_t * walk_pgdir(pde_t * pd, void * vaddr, uint32_t need_create);

void map_pages(pde_t * pd, void * vaddr, uint32_t size, void * paddr, uint32_t pte_attr);

pde_t * create_init_kvm(void);

void * free_uvm(pde_t * pgdir, void * start_addr, void * end_addr);

void * alloc_uvm(pde_t * pgdir, void * start_addr, void * end_addr);

void free_vm(pde_t * pgdir);

void * uva2kva(pde_t * pgdir, void * uvaddr);

int32_t copyout(pde_t * pgdir, void * addr, void * pos, uint32_t len);

pde_t * copy_vm(pde_t * pgdir, uint32_t size);


/* ======================= */


pte_t * walk_pgdir_noint(pde_t * pd, void * vaddr, uint32_t need_create);

void map_pages_noint(pde_t * pd, void * vaddr, uint32_t size, void * paddr, uint32_t pte_attr);

pde_t * create_init_kvm_noint(void);

void init_first_proc_uvm(pde_t * pd, uint32_t init_proc_start_addr, uint32_t init_proc_size);


#endif //_INCLUDE_VM_TOOLS_H_
