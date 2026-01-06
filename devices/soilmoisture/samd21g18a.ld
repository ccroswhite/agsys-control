/**
 * Linker Script for ATSAMD21G18A
 * 
 * Memory:
 *   Flash: 256KB (0x00000000 - 0x0003FFFF)
 *   SRAM:  32KB  (0x20000000 - 0x20007FFF)
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 256K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 32K
}

/* Stack size */
_stack_size = 2048;

/* Entry point */
ENTRY(Reset_Handler)

SECTIONS
{
    /* Vector table and code */
    .text :
    {
        . = ALIGN(4);
        _stext = .;
        
        KEEP(*(.isr_vector))    /* Vector table */
        *(.text)                /* Code */
        *(.text*)
        *(.rodata)              /* Read-only data */
        *(.rodata*)
        
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    /* ARM exception handling */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > FLASH

    /* Initialized data - stored in flash, copied to RAM at startup */
    _sidata = LOADADDR(.data);
    
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        
        *(.data)
        *(.data*)
        
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH

    /* Uninitialized data - zeroed at startup */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        
        *(.bss)
        *(.bss*)
        *(COMMON)
        
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } > RAM

    /* Stack */
    .stack :
    {
        . = ALIGN(8);
        _sstack = .;
        . = . + _stack_size;
        . = ALIGN(8);
        _estack = .;
    } > RAM

    /* Heap - remaining RAM after stack */
    .heap :
    {
        . = ALIGN(8);
        _sheap = .;
        end = .;
        _end = .;
        . = ORIGIN(RAM) + LENGTH(RAM) - _stack_size;
        _eheap = .;
    } > RAM

    /* Remove debug info from output */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}

/* Provide symbols for startup code */
PROVIDE(_estack = ORIGIN(RAM) + LENGTH(RAM));
