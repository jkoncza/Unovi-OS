#include "vmm.h"
#include "memory.h"
#include "graphics.h"

#include <stdint.h>
#include <stdbool.h>

static uint64_t kernel_pml4 = 0;
static uint64_t current_pml4 = 0;
static uint64_t allocate_table(void);
static uint64_t vmm_create_address_space(void);

extern char __kernel_start[];
extern char __kernel_end[];

extern char __text_start[];
extern char __text_end[];

extern char __rodata_start[];
extern char __rodata_end[];

extern char __data_start[];
extern char __data_end[];

extern char __bss_start[];
extern char __bss_end[];

static bool map_range(
    uint64_t pml4,
    uint64_t start,
    uint64_t end,
    uint64_t flags
) {
    start &= ~(VMM_PAGE_SIZE - 1);

    end =
        (end + VMM_PAGE_SIZE - 1) &
        ~(VMM_PAGE_SIZE - 1);

    for (uint64_t virtual_address = start;
         virtual_address < end;
         virtual_address += VMM_PAGE_SIZE) {

        uint64_t physical =
            memory_virtual_to_physical(
                virtual_address
            );

        if (physical == 0)
            return false;

        if (!vmm_map(
            pml4,
            virtual_address,
            physical,
            flags
        )) {
            return false;
        }
    }

    return true;
}


bool vmm_map_kernel(void) {
    if (kernel_pml4 == 0)
        return false;

    /*
     * Kernel code:
     * executable, readable, not writable.
     */
    if (!map_range(
        kernel_pml4,
        (uint64_t)(uintptr_t)__text_start,
        (uint64_t)(uintptr_t)__text_end,
        0
    )) {
        return false;
    }

    /*
     * Read-only data:
     * readable, NX.
     */
    if (!map_range(
        kernel_pml4,
        (uint64_t)(uintptr_t)__rodata_start,
        (uint64_t)(uintptr_t)__rodata_end,
        VMM_NX
    )) {
        return false;
    }

    /*
     * Writable data.
     */
    if (!map_range(
        kernel_pml4,
        (uint64_t)(uintptr_t)__data_start,
        (uint64_t)(uintptr_t)__data_end,
        VMM_WRITABLE | VMM_NX
    )) {
        return false;
    }

    /*
     * BSS.
     */
    if (!map_range(
        kernel_pml4,
        (uint64_t)(uintptr_t)__bss_start,
        (uint64_t)(uintptr_t)__bss_end,
        VMM_WRITABLE | VMM_NX
    )) {
        return false;
    }

    return true;
}

bool vmm_verify_kernel(void) {
    uint64_t address;

    /*
     * Verify that our kernel's text is mapped.
     */
    address =
        (uint64_t)(uintptr_t)__text_start;

    if (!vmm_get_physical(
        kernel_pml4,
        address,
        &address
    )) {
        return false;
    }

    /*
     * Verify that the end of the kernel is mapped.
     */
    address =
        (uint64_t)(uintptr_t)__kernel_end;

    address &= ~(VMM_PAGE_SIZE - 1);

    if (!vmm_get_physical(
        kernel_pml4,
        address,
        &address
    )) {
        return false;
    }

    return true;
}

bool vmm_activate_kernel(void) {
    if (kernel_pml4 == 0)
        return false;

    vmm_switch(kernel_pml4);

    return vmm_current() == kernel_pml4;
}

bool vmm_map_hhdm(void) {
    if (kernel_pml4 == 0)
        return false;

    uint64_t hhdm = memory_hhdm_offset();
    uint64_t total = memory_total();

    for (uint64_t physical = 0;
         physical < total;
         physical += VMM_PAGE_SIZE) {

        uint64_t virtual_address =
            hhdm + physical;

        /*
         * Don't wrap around the 64-bit address space.
         */
        if (virtual_address < hhdm)
            return false;

        /*
         * HHDM is kernel-only and writable.
         * NX prevents executing RAM through the HHDM.
         */
        if (!vmm_map(
            kernel_pml4,
            virtual_address,
            physical,
            VMM_WRITABLE | VMM_NX
        )) {
            /*
             * It may already exist.
             */
            uint64_t existing = 0;

            if (!vmm_get_physical(
                kernel_pml4,
                virtual_address,
                &existing
            )) {
                return false;
            }

            if (existing != physical)
                return false;
        }
    }

    return true;
}

bool vmm_init(void) {
    kernel_pml4 = vmm_create_address_space();

    if (kernel_pml4 == 0)
        return false;

    current_pml4 = kernel_pml4;

    return true;
}

uint64_t vmm_kernel_address_space(void) {
    return kernel_pml4;
}

uint64_t vmm_current(void) {
    return current_pml4;
}

void vmm_switch(uint64_t pml4_physical) {
    if (pml4_physical == 0)
        return;

    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(pml4_physical)
        : "memory"
    );

    current_pml4 = pml4_physical;
}


#define PAGE_TABLE_ENTRIES 512

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_NX       (1ULL << 63)

#define PAGE_ADDRESS_MASK 0x000FFFFFFFFFF000ULL

typedef struct {
    uint64_t entries[PAGE_TABLE_ENTRIES];
} PageTable;

static inline PageTable *table_from_physical(uint64_t physical) {
    return (PageTable *)physical_to_virtual(physical);
}

static inline uint64_t pml4_index(uint64_t address) {
    return (address >> 39) & 0x1FF;
}

static inline uint64_t pdpt_index(uint64_t address) {
    return (address >> 30) & 0x1FF;
}

static inline uint64_t pd_index(uint64_t address) {
    return (address >> 21) & 0x1FF;
}

static inline uint64_t pt_index(uint64_t address) {
    return (address >> 12) & 0x1FF;
}

static uint64_t allocate_table(void) {
    void *page = physical_page_alloc();

    if (page == 0)
        return 0;

    /*
     * physical_page_alloc() gives us an HHDM virtual address.
     * Convert it back to the actual physical address.
     */
    uint64_t physical =
        virtual_to_physical(page);

    PageTable *table =
        (PageTable *)page;

    for (uint64_t i = 0;
         i < PAGE_TABLE_ENTRIES;
         i++) {

        table->entries[i] = 0;
    }

    return physical;
}

uint64_t vmm_create_address_space(void) {
    return allocate_table();
}

bool vmm_map(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
) {
    if (pml4_physical == 0)
        return false;

    if ((virtual_address & (VMM_PAGE_SIZE - 1)) != 0)
        return false;

    if ((physical_address & (VMM_PAGE_SIZE - 1)) != 0)
        return false;

    PageTable *pml4 =
        table_from_physical(pml4_physical);

    /*
     * PML4
     */
    uint64_t pml4_i =
        pml4_index(virtual_address);

    if (!(pml4->entries[pml4_i] & PAGE_PRESENT)) {

        uint64_t table =
            allocate_table();

        if (table == 0)
            return false;

        pml4->entries[pml4_i] =
            table |
            PAGE_PRESENT |
            PAGE_WRITABLE |
            ((flags & PAGE_USER) ? PAGE_USER : 0);
    }

    PageTable *pdpt =
        table_from_physical(
            pml4->entries[pml4_i] &
            PAGE_ADDRESS_MASK
        );

    /*
     * PDPT
     */
    uint64_t pdpt_i =
        pdpt_index(virtual_address);

    if (!(pdpt->entries[pdpt_i] & PAGE_PRESENT)) {

        uint64_t table =
            allocate_table();

        if (table == 0)
            return false;

        pdpt->entries[pdpt_i] =
            table |
            PAGE_PRESENT |
            PAGE_WRITABLE |
            ((flags & PAGE_USER) ? PAGE_USER : 0);
    }

    PageTable *pd =
        table_from_physical(
            pdpt->entries[pdpt_i] &
            PAGE_ADDRESS_MASK
        );

    /*
     * Page directory.
     */
    uint64_t pd_i =
        pd_index(virtual_address);

    if (!(pd->entries[pd_i] & PAGE_PRESENT)) {

        uint64_t table =
            allocate_table();

        if (table == 0)
            return false;

        pd->entries[pd_i] =
            table |
            PAGE_PRESENT |
            PAGE_WRITABLE |
            ((flags & PAGE_USER) ? PAGE_USER : 0);
    }

    PageTable *pt =
        table_from_physical(
            pd->entries[pd_i] &
            PAGE_ADDRESS_MASK
        );

    /*
     * Page table.
     */
    uint64_t pt_i =
        pt_index(virtual_address);

    /*
     * Don't silently overwrite an existing mapping.
     */
    if (pt->entries[pt_i] & PAGE_PRESENT)
        return false;

    uint64_t entry =
        (physical_address & PAGE_ADDRESS_MASK) |
        PAGE_PRESENT;

    if (flags & PAGE_WRITABLE)
        entry |= PAGE_WRITABLE;

    if (flags & PAGE_USER)
        entry |= PAGE_USER;

    if (flags & PAGE_NX)
        entry |= PAGE_NX;

    pt->entries[pt_i] = entry;

    return true;
}

bool vmm_unmap(
    uint64_t pml4_physical,
    uint64_t virtual_address
) {
    if (pml4_physical == 0)
        return false;

    if ((virtual_address & (VMM_PAGE_SIZE - 1)) != 0)
        return false;

    PageTable *pml4 =
        table_from_physical(pml4_physical);

    uint64_t pml4_i =
        pml4_index(virtual_address);

    if (!(pml4->entries[pml4_i] & PAGE_PRESENT))
        return false;

    PageTable *pdpt =
        table_from_physical(
            pml4->entries[pml4_i] &
            PAGE_ADDRESS_MASK
        );

    uint64_t pdpt_i =
        pdpt_index(virtual_address);

    if (!(pdpt->entries[pdpt_i] & PAGE_PRESENT))
        return false;

    PageTable *pd =
        table_from_physical(
            pdpt->entries[pdpt_i] &
            PAGE_ADDRESS_MASK
        );

    uint64_t pd_i =
        pd_index(virtual_address);

    if (!(pd->entries[pd_i] & PAGE_PRESENT))
        return false;

    PageTable *pt =
        table_from_physical(
            pd->entries[pd_i] &
            PAGE_ADDRESS_MASK
        );

    uint64_t pt_i =
        pt_index(virtual_address);

    if (!(pt->entries[pt_i] & PAGE_PRESENT))
        return false;

    pt->entries[pt_i] = 0;

    /*
     * The page-table structures themselves are intentionally
     * retained for now.
     *
     * We'll add page-table cleanup once the address-space
     * lifetime system exists.
     */
    __asm__ volatile (
        "invlpg (%0)"
        :
        : "r"(virtual_address)
        : "memory"
    );

    return true;
}

bool vmm_get_physical(
    uint64_t pml4_physical,
    uint64_t virtual_address,
    uint64_t *physical_address
) {
    if (pml4_physical == 0 ||
        physical_address == 0) {
        return false;
    }

    PageTable *pml4 =
        table_from_physical(pml4_physical);

    uint64_t pml4_i =
        pml4_index(virtual_address);

    if (!(pml4->entries[pml4_i] & PAGE_PRESENT))
        return false;

    PageTable *pdpt =
        table_from_physical(
            pml4->entries[pml4_i] &
            PAGE_ADDRESS_MASK
        );

    uint64_t pdpt_i =
        pdpt_index(virtual_address);

    if (!(pdpt->entries[pdpt_i] & PAGE_PRESENT))
        return false;

    PageTable *pd =
        table_from_physical(
            pdpt->entries[pdpt_i] &
            PAGE_ADDRESS_MASK
        );

    uint64_t pd_i =
        pd_index(virtual_address);

    if (!(pd->entries[pd_i] & PAGE_PRESENT))
        return false;

    PageTable *pt =
        table_from_physical(
            pd->entries[pd_i] &
            PAGE_ADDRESS_MASK
        );

    uint64_t pt_i =
        pt_index(virtual_address);

    if (!(pt->entries[pt_i] & PAGE_PRESENT))
        return false;

    uint64_t page =
        pt->entries[pt_i] &
        PAGE_ADDRESS_MASK;

    *physical_address =
        page +
        (virtual_address & (VMM_PAGE_SIZE - 1));

    return true;
}
