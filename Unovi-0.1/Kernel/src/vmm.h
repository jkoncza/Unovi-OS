#ifndef UNOVI_VMM_H
#define UNOVI_VMM_H

#include <stdint.h>
#include <stdbool.h>

#define VMM_PAGE_SIZE 4096ULL

#define VMM_PRESENT  (1ULL << 0)
#define VMM_WRITABLE (1ULL << 1)
#define VMM_USER     (1ULL << 2)
#define VMM_NX       (1ULL << 63)

bool vmm_init(void);
bool vmm_map_hhdm(void);
bool vmm_map_kernel(void);
bool vmm_verify_kernel(void);
bool vmm_activate_kernel(void);

uint64_t vmm_kernel_address_space(void);

bool vmm_map(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
);

bool vmm_unmap(
    uint64_t pml4_physical,
    uint64_t virtual_address
);

bool vmm_get_physical(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t *physical_address
);

void vmm_switch(uint64_t pml4_physical);

uint64_t vmm_current(void);

#endif
