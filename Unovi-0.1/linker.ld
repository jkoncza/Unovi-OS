ENTRY(_start)

SECTIONS
{
    . = 0xffffffff80000000;

    .text : ALIGN(4K)
    {
        KEEP(*(.text))
        KEEP(*(.text.*))
    }

    .rodata : ALIGN(4K)
    {
        KEEP(*(.rodata))
        KEEP(*(.rodata.*))
    }

    .data : ALIGN(4K)
    {
        KEEP(*(.data))
        KEEP(*(.data.*))
    }

    .bss : ALIGN(4K)
    {
        KEEP(*(.bss))
        KEEP(*(.bss.*))

        *(COMMON)
    }

    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.note*)
        *(.comment)
    }
}