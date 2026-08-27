ENTRY(_start)

SECTIONS
{
    . = 0xffffffff80000000;

    __kernel_start = .;

    .text : ALIGN(4K)
    {
        __text_start = .;

        KEEP(*(.text))
        KEEP(*(.text.*))

        __text_end = .;
    }

    .rodata : ALIGN(4K)
    {
        __rodata_start = .;

        KEEP(*(.rodata))
        KEEP(*(.rodata.*))

        __rodata_end = .;
    }

    .data : ALIGN(4K)
    {
        __data_start = .;

        KEEP(*(.data))
        KEEP(*(.data.*))

        __data_end = .;
    }

    .bss : ALIGN(4K)
    {
        __bss_start = .;

        KEEP(*(.bss))
        KEEP(*(.bss.*))

        __bss_end = .;
    }

    __kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame*)
        *(.note*)
        *(.comment)
    }
}
