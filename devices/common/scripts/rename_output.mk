# =============================================================================
# rename_output.mk - Shared post-build renaming for AgSys devices
# =============================================================================
# Include this file in device Makefiles to add output renaming support.
#
# Required variables (set before including):
#   OUTPUT_DIRECTORY - Build output directory (e.g., _build)
#   OUTPUT_NAME      - Target output name (from build_config.mk)
#   TARGET_CHIP      - Target chip name (e.g., nrf52832_xxaa, nrf52840_xxaa)
#
# Usage in device Makefile:
#   TARGET_CHIP := nrf52832_xxaa
#   include $(COMMON_DIR)/scripts/rename_output.mk
#
# Then add to your default target:
#   all: default rename_output
# =============================================================================

.PHONY: rename_output

rename_output:
	@cp $(OUTPUT_DIRECTORY)/$(TARGET_CHIP).hex $(OUTPUT_DIRECTORY)/$(OUTPUT_NAME).hex
	@cp $(OUTPUT_DIRECTORY)/$(TARGET_CHIP).bin $(OUTPUT_DIRECTORY)/$(OUTPUT_NAME).bin
	@echo "Output: $(OUTPUT_DIRECTORY)/$(OUTPUT_NAME).hex"
