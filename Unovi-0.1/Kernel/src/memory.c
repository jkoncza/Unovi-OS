#include "memory.h"

#include <limine.h>
#include <stdint.h>
#include <stdbool.h>

#define BITS_PER_WORD 64ULL

static volatile struct limine_memmap_request memory_map_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = 0
};

static volatile struct limine_executable_address_request executable_address_request = {
    .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST_ID,
    .revision = 0,
    .response = 0
};


static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = 0
};

uint64_t memory_kernel_physical_base(void) {
    if (executable_address_request.response == 0)
        return 0;

    return executable_address_request.response->physical_base;
}

uint64_t memory_kernel_virtual_base(void) {
    if (executable_address_request.response == 0)
        return 0;

    return executable_address_request.response->virtual_base;
}

uint64_t memory_virtual_to_physical(uint64_t virtual_address) {
    if (executable_address_request.response == 0)
        return 0;

    uint64_t virtual_base =
        executable_address_request.response->virtual_base;

    uint64_t physical_base =
        executable_address_request.response->physical_base;

    if (virtual_address < virtual_base)
        return 0;

    return physical_base +
           (virtual_address - virtual_base);
}


static uint64_t total_memory = 0;
static uint64_t usable_memory = 0;
static uint64_t free_memory = 0;

static uint64_t highest_address = 0;
static uint64_t page_count = 0;

static uint64_t *bitmap = 0;
static uint64_t bitmap_words = 0;

static uint64_t hhdm_offset = 0;

static bool initialized = false;

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) &
           ~(alignment - 1);
}

static inline void bitmap_set(uint64_t page) {
    bitmap[page / BITS_PER_WORD] |=
        1ULL << (page % BITS_PER_WORD);
}

static inline void bitmap_clear(uint64_t page) {
    bitmap[page / BITS_PER_WORD] &=
        ~(1ULL << (page % BITS_PER_WORD));
}

static inline bool bitmap_test(uint64_t page) {
    return (bitmap[page / BITS_PER_WORD] &
            (1ULL << (page % BITS_PER_WORD))) != 0;
}

uint64_t memory_hhdm_offset(void) {
    return hhdm_offset;
}

void *physical_to_virtual(uint64_t physical) {
    return (void *)(uintptr_t)(physical + hhdm_offset);
}

uint64_t virtual_to_physical(const void *virtual_address) {
    return (uint64_t)(uintptr_t)virtual_address -
           hhdm_offset;
}

bool memory_init(void) {
    if (memory_map_request.response == 0)
        return false;

    if (hhdm_request.response == 0)
        return false;

    if (memory_map_request.response->entry_count == 0)
        return false;

    hhdm_offset =
        hhdm_request.response->offset;

    struct limine_memmap_response *map =
        memory_map_request.response;

    /*
     * Find the highest physical address.
     */
    for (uint64_t i = 0;
         i < map->entry_count;
         i++) {

        struct limine_memmap_entry *entry =
            map->entries[i];

        uint64_t end =
            entry->base + entry->length;

        if (end > highest_address)
            highest_address = end;

        if (entry->type == LIMINE_MEMMAP_USABLE)
            usable_memory += entry->length;
    }

    total_memory = highest_address;

    page_count =
        align_up(highest_address, PAGE_SIZE) /
        PAGE_SIZE;

    bitmap_words =
        (page_count + BITS_PER_WORD - 1) /
        BITS_PER_WORD;

    uint64_t bitmap_size =
        bitmap_words * sizeof(uint64_t);

    uint64_t bitmap_physical = 0;

    /*
     * Find usable RAM large enough to hold the bitmap.
     */
    for (uint64_t i = 0;
         i < map->entry_count;
         i++) {

        struct limine_memmap_entry *entry =
            map->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE)
            continue;

        uint64_t start =
            align_up(entry->base, PAGE_SIZE);

        uint64_t end =
            entry->base + entry->length;

        if (end <= start)
            continue;

        if (end - start >= bitmap_size) {
            bitmap_physical = start;
            break;
        }
    }

    if (bitmap_physical == 0)
        return false;

    /*
     * IMPORTANT:
     *
     * bitmap_physical is a physical address.
     *
     * We access it through Limine's HHDM.
     */
    bitmap =
        (uint64_t *)physical_to_virtual(
            bitmap_physical
        );

    /*
     * Initially mark every physical page allocated.
     */
    for (uint64_t i = 0;
         i < bitmap_words;
         i++) {

        bitmap[i] = UINT64_MAX;
    }

    /*
     * Mark usable RAM as free.
     */
    for (uint64_t i = 0;
         i < map->entry_count;
         i++) {

        struct limine_memmap_entry *entry =
            map->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE)
            continue;

        uint64_t start =
            align_up(entry->base, PAGE_SIZE);

        uint64_t end =
            entry->base + entry->length;

        end &= ~(PAGE_SIZE - 1);

        for (uint64_t address = start;
             address < end;
             address += PAGE_SIZE) {

            uint64_t page =
                address / PAGE_SIZE;

            bitmap_clear(page);
        }
    }

    /*
     * Reserve the pages occupied by the bitmap.
     */
    uint64_t bitmap_start =
        bitmap_physical / PAGE_SIZE;

    uint64_t bitmap_end =
        align_up(
            bitmap_physical + bitmap_size,
            PAGE_SIZE
        ) / PAGE_SIZE;

    for (uint64_t page = bitmap_start;
         page < bitmap_end;
         page++) {

        bitmap_set(page);
    }

    /*
     * Recalculate free memory accurately.
     */
    free_memory = 0;

    for (uint64_t i = 0;
         i < map->entry_count;
         i++) {

        struct limine_memmap_entry *entry =
            map->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE)
            free_memory += entry->length;
    }

    free_memory -=
        (bitmap_end - bitmap_start) * PAGE_SIZE;

    initialized = true;

    return true;
}

uint64_t memory_total(void) {
    return total_memory;
}

uint64_t memory_usable(void) {
    return usable_memory;
}

uint64_t memory_free(void) {
    return free_memory;
}

void *physical_page_alloc(void) {
    if (!initialized)
        return 0;

    for (uint64_t word = 0;
         word < bitmap_words;
         word++) {

        uint64_t bits = bitmap[word];

        if (bits == UINT64_MAX)
            continue;

        for (uint64_t bit = 0;
             bit < BITS_PER_WORD;
             bit++) {

            uint64_t page =
                word * BITS_PER_WORD + bit;

            if (page >= page_count)
                return 0;

            if (!bitmap_test(page)) {

                bitmap_set(page);

                if (free_memory >= PAGE_SIZE)
                    free_memory -= PAGE_SIZE;

                return (void *)physical_to_virtual(
                    page * PAGE_SIZE
                );
            }
        }
    }

    return 0;
}

void physical_page_free(void *address) {
    if (!initialized || address == 0)
        return;

    uint64_t physical =
        virtual_to_physical(address);

    if (physical % PAGE_SIZE != 0)
        return;

    uint64_t page =
        physical / PAGE_SIZE;

    if (page >= page_count)
        return;

    if (!bitmap_test(page))
        return;

    bitmap_clear(page);

    free_memory += PAGE_SIZE;
}
