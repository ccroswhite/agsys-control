/* =============================================================================
 * nrf52840_s140_memory.ld
 * Memory layout for nRF52840 with S140 v7.2.0 SoftDevice
 * 
 * See freertos-common/include/agsys_internal_flash.h for full memory map
 * 
 * Flash Layout:
 *   0x00000 - 0x27000   S140 SoftDevice (156KB)
 *   0x27000 - 0xF1000   Application (808KB)
 *   0xF1000 - 0xF3000   Reserved
 *   0xF3000 - 0xFB000   Bootloader (32KB)
 *   0xFB000 - 0x100000  Bootloader settings + MBR params
 * 
 * RAM Layout:
 *   0x20000000 - 0x20005968  SoftDevice RAM
 *   0x20005968 - 0x20040000  Application RAM (~234KB)
 * ============================================================================= */

MEMORY
{
  FLASH (rx) : ORIGIN = 0x27000, LENGTH = 0xCA000
  RAM (rwx) :  ORIGIN = 0x20005968, LENGTH = 0x3a698
  UICR_APPROTECT (r) : ORIGIN = 0x10001208, LENGTH = 0x4
}
