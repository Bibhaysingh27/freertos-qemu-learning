/* Memory layout */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 0x00400000
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 0x00400000
}

ENTRY(Reset_Handler)

SECTIONS
{
    .isr_vector ORIGIN(FLASH) :
    {
        __vector_table_start = .;
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text)
        *(.text*)
    } > FLASH

    .rodata :
    {
        *(.rodata)
        *(.rodata*)
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    _sidata = LOADADDR(.data);

    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

ASSERT((_estack & 7) == 0, "Stack must be 8-byte aligned");