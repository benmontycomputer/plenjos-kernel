/* linker-relocatable.lds */
PROVIDE(__ehdr_start = .);
ENTRY(_start)

SECTIONS
{
    . = 0x7fff80000000;
    /* Start at 0, but code is relocatable */
    . = ALIGN(0x1000);
    .text : {
        *(.text.entry)    /* Your _start function */
        *(.text*)         /* All other text */
    }

    . = ALIGN(0x1000);
    .rodata : {
        *(.rodata*)
        *(.constdata)
    }

    . = ALIGN(0x1000);
    .data : {
        *(.data*)
        *(.sdata*)
    }

    . = ALIGN(0x1000);
    .bss : {
        *(.bss*)
        *(.sbss*)
        *(COMMON)
    }
}