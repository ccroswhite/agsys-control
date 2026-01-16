/* =============================================================================
 * nrf52832_s132_memory.ld
 * Memory layout for nRF52832 with S132 v7.2.0 SoftDevice
 * 
 * See freertos-common/include/agsys_internal_flash.h for full memory map
 * 
 * Flash Layout:
 *   0x00000 - 0x26000  S132 SoftDevice (152KB)
 *   0x26000 - 0x68000  Application (264KB)
 *   0x68000 - 0x6A000  Reserved
 *   0x6A000 - 0x72000  Bootloader (32KB)
 *   0x72000 - 0x80000  Bootloader settings + MBR params
 * 
 * RAM Layout:
 *   0x20000000 - 0x20005968  SoftDevice RAM
 *   0x20005968 - 0x20010000  Application RAM (~42KB)
 * ============================================================================= */

MEMORY
{
  FLASH (rx) : ORIGIN = 0x26000, LENGTH = 0x42000
  RAM (rwx) :  ORIGIN = 0x20005968, LENGTH = 0xa698
  UICR_APPROTECT (r) : ORIGIN = 0x10001208, LENGTH = 0x4
}
