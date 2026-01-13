/**
 * Linker script for AgSys Bootloader
 * 
 * Places bootloader at 0x72000 (after Recovery Loader)
 * 16KB allocated for bootloader code
 */

/* Memory regions */
MEMORY
{
    /* Bootloader region: 0x72000 - 0x76000 (16KB) */
    FLASH (rx) : ORIGIN = 0x00072000, LENGTH = 0x4000
    
    /* RAM: Use top 4KB of RAM for bootloader stack/data */
    /* Main RAM is 64KB: 0x20000000 - 0x20010000 */
    /* We use 0x2000F000 - 0x20010000 to avoid conflicts */
    RAM (rwx) : ORIGIN = 0x2000F000, LENGTH = 0x1000
}

/* Entry point */
ENTRY(Reset_Handler)

/* Stack configuration */
_stack_size = 0x400;  /* 1KB stack */
_stack_end = ORIGIN(RAM) + LENGTH(RAM);
_stack_start = _stack_end - _stack_size;

SECTIONS
{
    /* Vector table and code */
    .text :
    {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        
        KEEP(*(.init))
        KEEP(*(.fini))
        
        . = ALIGN(4);
        _etext = .;
    } > FLASH
    
    /* Initialized data */
    .data : AT(_etext)
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM
    
    /* Uninitialized data */
    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
    
    /* Stack */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + _stack_size;
        . = ALIGN(8);
    } > RAM
    
    /* Discard */
    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}

/* Provide symbols for startup code */
PROVIDE(_sidata = LOADADDR(.data));
