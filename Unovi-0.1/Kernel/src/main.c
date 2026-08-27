#include "graphics.h"
#include "memory.h"
#include "vmm.h"

static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kernel_main(void) {
    if (!graphics_init())
        halt_forever();

    graphics_clear(
        COLOR_RGB(45, 20, 25)
    );

    if (!memory_init())
        halt_forever();

    if (!vmm_init())
        halt_forever();

    if (!vmm_map_kernel())
        halt_forever();

    if (!vmm_map_hhdm())
        halt_forever();

    if (!vmm_verify_kernel())
        halt_forever();
    
    if (!vmm_activate_kernel())
        halt_forever();



    /*
     * We have now built our own kernel mappings,
     * but Limine's CR3 is still active.
     */

    graphics_fill_rect(
        0,
        0,
        graphics_get_info().width,
        graphics_get_info().height,
        COLOR_RGB(45, 20, 25)
    );

    halt_forever();
}
