OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

PHDRS
{
    all PT_LOAD FLAGS(0x7);
}

SECTIONS
{
    . = 0x100000;
    
    .entry : {
        *(.entry)
    } :all

    .text : {
        *(.text*)
    } :all

    .rodata : {
        *(.rodata*)
    } :all

    .data : {
        *(.data*)
    } :all

    .bss : {
        *(.bss*)
        *(COMMON)
    } :all

    __end = .;
}
