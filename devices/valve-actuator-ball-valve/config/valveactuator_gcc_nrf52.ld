/* Linker script for nRF52810 with S112 SoftDevice */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

MEMORY
{
  /* S112 SoftDevice uses 0x00000000 - 0x00018000 (96KB) */
  FLASH (rx) : ORIGIN = 0x00019000, LENGTH = 0x17000  /* 92KB for application */
  
  /* S112 SoftDevice uses 0x20000000 - 0x20001668 (~5.6KB) */
  RAM (rwx) :  ORIGIN = 0x20001668, LENGTH = 0x4998   /* ~18.4KB for application */
}

SECTIONS
{
}

INCLUDE "nrf_common.ld"
