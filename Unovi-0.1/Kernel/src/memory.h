#ifndef UNOVI_MEMORY_H
#define UNOVI_MEMORY_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096ULL

bool memory_init(void);

uint64_t memory_total(void);
uint64_t memory_usable(void);
uint64_t memory_free(void);
uint64_t memory_kernel_physical_base(void);
uint64_t memory_kernel_virtual_base(void);
uint64_t memory_virtual_to_physical(uint64_t virtual_address);

uint64_t memory_hhdm_offset(void);

void *physical_to_virtual(uint64_t physical);
uint64_t virtual_to_physical(const void *virtual_address);

void *physical_page_alloc(void);
void physical_page_free(void *address);

#endif
