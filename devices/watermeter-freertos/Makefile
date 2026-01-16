PROJECT_NAME     := watermeter_freertos
TARGETS          := nrf52840_xxaa
OUTPUT_DIRECTORY := _build

SDK_ROOT := $(HOME)/nordic/nRF5_SDK_17.1.0
PROJ_DIR := .
COMMON_DIR := ../freertos-common
LVGL_DIR := $(PROJ_DIR)/external/lvgl

# Device name for build naming
DEVICE_NAME := watermeter

# Include build configuration
include $(COMMON_DIR)/build_config.mk

# Default target - must be before SDK include to override
.PHONY: all
all: default rename_output

$(OUTPUT_DIRECTORY)/nrf52840_xxaa.out: \
  LINKER_SCRIPT := config/watermeter_gcc_nrf52840.ld

# Source files
SRC_FILES += \
  $(SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52840.S \
  $(SDK_ROOT)/modules/nrfx/mdk/system_nrf52840.c \
  $(SDK_ROOT)/components/boards/boards.c \
  $(SDK_ROOT)/components/libraries/util/app_error.c \
  $(SDK_ROOT)/components/libraries/util/app_error_handler_gcc.c \
  $(SDK_ROOT)/components/libraries/util/app_error_weak.c \
  $(SDK_ROOT)/components/libraries/util/app_util_platform.c \
  $(SDK_ROOT)/components/libraries/util/nrf_assert.c \
  $(SDK_ROOT)/components/libraries/atomic/nrf_atomic.c \
  $(SDK_ROOT)/components/libraries/balloc/nrf_balloc.c \
  $(SDK_ROOT)/components/libraries/memobj/nrf_memobj.c \
  $(SDK_ROOT)/components/libraries/ringbuf/nrf_ringbuf.c \
  $(SDK_ROOT)/components/libraries/strerror/nrf_strerror.c \
  $(SDK_ROOT)/components/libraries/experimental_section_vars/nrf_section_iter.c \
  $(SDK_ROOT)/components/libraries/pwr_mgmt/nrf_pwr_mgmt.c \
  $(SDK_ROOT)/components/libraries/timer/app_timer_freertos.c \
  $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_clock.c \
  $(SDK_ROOT)/integration/nrfx/legacy/nrf_drv_spi.c \
  $(SDK_ROOT)/modules/nrfx/soc/nrfx_atomic.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_clock.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_gpiote.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_spim.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_saadc.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_timer.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/nrfx_pwm.c \
  $(SDK_ROOT)/modules/nrfx/drivers/src/prs/nrfx_prs.c \
  $(SDK_ROOT)/modules/nrfx/hal/nrf_nvmc.c \
  $(SDK_ROOT)/external/freertos/source/croutine.c \
  $(SDK_ROOT)/external/freertos/source/event_groups.c \
  $(SDK_ROOT)/external/freertos/source/portable/MemMang/heap_4.c \
  $(SDK_ROOT)/external/freertos/source/list.c \
  $(SDK_ROOT)/external/freertos/portable/GCC/nrf52/port.c \
  $(SDK_ROOT)/external/freertos/portable/CMSIS/nrf52/port_cmsis.c \
  $(SDK_ROOT)/external/freertos/portable/CMSIS/nrf52/port_cmsis_systick.c \
  $(SDK_ROOT)/external/freertos/source/queue.c \
  $(SDK_ROOT)/external/freertos/source/stream_buffer.c \
  $(SDK_ROOT)/external/freertos/source/tasks.c \
  $(SDK_ROOT)/external/freertos/source/timers.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT.c \
  $(SDK_ROOT)/external/segger_rtt/SEGGER_RTT_printf.c \
  $(SDK_ROOT)/components/softdevice/common/nrf_sdh.c \
  $(SDK_ROOT)/components/softdevice/common/nrf_sdh_ble.c \
  $(SDK_ROOT)/components/softdevice/common/nrf_sdh_soc.c \
  $(SDK_ROOT)/components/ble/common/ble_advdata.c \
  $(SDK_ROOT)/components/ble/common/ble_srv_common.c \
  $(PROJ_DIR)/src/main.c \
  $(PROJ_DIR)/src/ads131m02.c \
  $(PROJ_DIR)/src/st7789.c \
  $(PROJ_DIR)/src/display_new.c \
  $(PROJ_DIR)/src/ui/ui_screen_main.c \
  $(PROJ_DIR)/src/ui/ui_screen_menu.c \
  $(PROJ_DIR)/src/ui/ui_screen_ota.c \
  $(PROJ_DIR)/src/ui/ui_screen_cal.c \
  $(PROJ_DIR)/src/ui/ui_screen_diag.c \
  $(PROJ_DIR)/src/ui/ui_screen_settings.c \
  $(PROJ_DIR)/src/ui/ui_screen_misc.c \
  $(PROJ_DIR)/src/ui/ui_overlay_alarm.c \
  $(PROJ_DIR)/src/ui/ui_overlay_pin.c \
  $(PROJ_DIR)/src/ui/ui_status_bar.c \
  $(PROJ_DIR)/src/lora_task.c \
  $(PROJ_DIR)/src/flow_calc.c \
  $(PROJ_DIR)/src/coil_driver.c \
  $(PROJ_DIR)/src/temp_sensor.c \
  $(PROJ_DIR)/src/lvgl_port.c \
  $(PROJ_DIR)/src/nrf_sdh_freertos_wrapper.c \
  $(PROJ_DIR)/src/app_header.c \
  $(COMMON_DIR)/src/ui/ui_common.c \
  $(COMMON_DIR)/src/ui/ui_display_driver.c \
  $(COMMON_DIR)/src/agsys_app_header.c \
  $(COMMON_DIR)/src/agsys_ble.c \
  $(COMMON_DIR)/src/agsys_ble_auth.c \
  $(COMMON_DIR)/src/agsys_device.c \
  $(COMMON_DIR)/src/agsys_flash.c \
  $(COMMON_DIR)/src/agsys_fram_log.c \
  $(COMMON_DIR)/src/agsys_flash_backup.c \
  $(COMMON_DIR)/src/agsys_fram.c \
  $(COMMON_DIR)/src/agsys_spi.c \
  $(COMMON_DIR)/src/agsys_lora.c \
  $(wildcard $(LVGL_DIR)/src/*.c) \
  $(wildcard $(LVGL_DIR)/src/core/*.c) \
  $(wildcard $(LVGL_DIR)/src/display/*.c) \
  $(wildcard $(LVGL_DIR)/src/draw/*.c) \
  $(wildcard $(LVGL_DIR)/src/draw/sw/*.c) \
  $(wildcard $(LVGL_DIR)/src/draw/sw/blend/*.c) \
  $(wildcard $(LVGL_DIR)/src/font/*.c) \
  $(wildcard $(LVGL_DIR)/src/indev/*.c) \
  $(wildcard $(LVGL_DIR)/src/layouts/*.c) \
  $(wildcard $(LVGL_DIR)/src/layouts/flex/*.c) \
  $(wildcard $(LVGL_DIR)/src/libs/bin_decoder/*.c) \
  $(wildcard $(LVGL_DIR)/src/libs/freetype/*.c) \
  $(wildcard $(LVGL_DIR)/src/misc/*.c) \
  $(wildcard $(LVGL_DIR)/src/misc/cache/*.c) \
  $(wildcard $(LVGL_DIR)/src/osal/*.c) \
  $(wildcard $(LVGL_DIR)/src/stdlib/*.c) \
  $(wildcard $(LVGL_DIR)/src/stdlib/builtin/*.c) \
  $(wildcard $(LVGL_DIR)/src/themes/*.c) \
  $(wildcard $(LVGL_DIR)/src/themes/default/*.c) \
  $(wildcard $(LVGL_DIR)/src/tick/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/arc/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/bar/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/button/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/buttonmatrix/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/image/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/label/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/line/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/menu/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/msgbox/*.c) \
  $(wildcard $(LVGL_DIR)/src/widgets/spinner/*.c) \
  $(COMMON_DIR)/src/agsys_ota.c \
  $(COMMON_DIR)/src/agsys_ble_ota.c \
  $(COMMON_DIR)/src/syscalls.c \

# Include folders
INC_FOLDERS += \
  $(PROJ_DIR)/config \
  $(PROJ_DIR)/src \
  $(PROJ_DIR)/src/ui \
  $(COMMON_DIR)/include \
  $(COMMON_DIR)/include/ui \
  $(SDK_ROOT)/components \
  $(SDK_ROOT)/components/boards \
  $(SDK_ROOT)/components/libraries/util \
  $(SDK_ROOT)/components/libraries/timer \
  $(SDK_ROOT)/components/libraries/balloc \
  $(SDK_ROOT)/components/libraries/ringbuf \
  $(SDK_ROOT)/components/libraries/memobj \
  $(SDK_ROOT)/components/libraries/log \
  $(SDK_ROOT)/components/libraries/log/src \
  $(SDK_ROOT)/components/libraries/strerror \
  $(SDK_ROOT)/components/libraries/atomic \
  $(SDK_ROOT)/components/libraries/delay \
  $(SDK_ROOT)/components/libraries/pwr_mgmt \
  $(SDK_ROOT)/components/libraries/mutex \
  $(SDK_ROOT)/components/libraries/experimental_section_vars \
  $(SDK_ROOT)/components/softdevice/s140/headers \
  $(SDK_ROOT)/components/softdevice/s140/headers/nrf52 \
  $(SDK_ROOT)/components/softdevice/common \
  $(SDK_ROOT)/components/ble/common \
  $(SDK_ROOT)/components/ble/ble_advertising \
  $(SDK_ROOT)/components/toolchain/cmsis/include \
  $(SDK_ROOT)/modules/nrfx \
  $(SDK_ROOT)/modules/nrfx/mdk \
  $(SDK_ROOT)/modules/nrfx/hal \
  $(SDK_ROOT)/modules/nrfx/drivers/include \
  $(SDK_ROOT)/modules/nrfx/drivers/src/prs \
  $(SDK_ROOT)/integration/nrfx \
  $(SDK_ROOT)/integration/nrfx/legacy \
  $(SDK_ROOT)/external/freertos/source/include \
  $(SDK_ROOT)/external/freertos/portable/GCC/nrf52 \
  $(SDK_ROOT)/external/freertos/portable/CMSIS/nrf52 \
  $(SDK_ROOT)/external/segger_rtt \
  $(PROJ_DIR)/external \
  $(LVGL_DIR) \
  $(LVGL_DIR)/src \

# Libraries
LIB_FILES += \

# Optimization flags
OPT = -O3 -g3
# Uncomment the line below to enable link time optimization
#OPT += -flto

# C flags common to all targets
CFLAGS += $(OPT)
CFLAGS += -DLV_CONF_INCLUDE_SIMPLE
CFLAGS += -DBOARD_CUSTOM
CFLAGS += -DCONFIG_GPIO_AS_PINRESET
CFLAGS += -DFLOAT_ABI_HARD
CFLAGS += -DFREERTOS
CFLAGS += -DNRF52840_XXAA
CFLAGS += -DNRF_SD_BLE_API_VERSION=7
CFLAGS += -DS140
CFLAGS += -DSOFTDEVICE_PRESENT
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -Wall -Werror
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums

# C++ flags common to all targets
CXXFLAGS += $(OPT)

# Assembler flags common to all targets
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
ASMFLAGS += -DBOARD_CUSTOM
ASMFLAGS += -DCONFIG_GPIO_AS_PINRESET
ASMFLAGS += -DFLOAT_ABI_HARD
ASMFLAGS += -DFREERTOS
ASMFLAGS += -DNRF52840_XXAA
ASMFLAGS += -DNRF_SD_BLE_API_VERSION=7
ASMFLAGS += -DS140
ASMFLAGS += -DSOFTDEVICE_PRESENT

# Linker flags
LDFLAGS += $(OPT)
LDFLAGS += -mthumb -mabi=aapcs -L$(SDK_ROOT)/modules/nrfx/mdk -L$(COMMON_DIR)/linker -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# let linker dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs

nrf52840_xxaa: CFLAGS += -D__HEAP_SIZE=8192
nrf52840_xxaa: CFLAGS += -D__STACK_SIZE=8192
nrf52840_xxaa: ASMFLAGS += -D__HEAP_SIZE=8192
nrf52840_xxaa: ASMFLAGS += -D__STACK_SIZE=8192

# Add standard libraries at the very end of the linker input, after all objects
# that may need symbols provided by these libraries.
LIB_FILES += -lc -lnosys -lm

.PHONY: default help

# Default target - first one defined
default: nrf52840_xxaa

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo		nrf52840_xxaa
	@echo		flash_softdevice
	@echo		sdk_config - starting external tool for editing sdk_config.h
	@echo		flash      - flashing binary

TEMPLATE_PATH := $(SDK_ROOT)/components/toolchain/gcc

include $(TEMPLATE_PATH)/Makefile.common

$(foreach target, $(TARGETS), $(call define_target, $(target)))

.PHONY: flash flash_softdevice erase

# Flash the program
flash: default
	@echo Flashing: $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex
	nrfjprog -f nrf52 --program $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex --sectorerase
	nrfjprog -f nrf52 --reset

# Flash softdevice
flash_softdevice:
	@echo Flashing: s140_nrf52_7.2.0_softdevice.hex
	nrfjprog -f nrf52 --program $(SDK_ROOT)/components/softdevice/s140/hex/s140_nrf52_7.2.0_softdevice.hex --sectorerase
	nrfjprog -f nrf52 --reset

erase:
	nrfjprog -f nrf52 --eraseall

SDK_CONFIG_FILE := $(PROJ_DIR)/config/sdk_config.h
CMSIS_CONFIG_TOOL := $(SDK_ROOT)/external_tools/cmsisconfig/CMSIS_Configuration_Wizard.jar
sdk_config:
	java -jar $(CMSIS_CONFIG_TOOL) $(SDK_CONFIG_FILE)

# Post-build: rename output files
.PHONY: rename_output
rename_output:
	@cp $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex $(OUTPUT_DIRECTORY)/$(OUTPUT_NAME).hex
	@cp $(OUTPUT_DIRECTORY)/nrf52840_xxaa.bin $(OUTPUT_DIRECTORY)/$(OUTPUT_NAME).bin
	@echo "Output: $(OUTPUT_DIRECTORY)/$(OUTPUT_NAME).hex"

# Build with proper output naming (alias for all)
.PHONY: release
release: all

# Build, sign, and package firmware for release
.PHONY: sign
sign: release
	@echo "Signing firmware..."
	@if [ ! -f $(SIGNING_KEY) ]; then \
		echo "Error: Signing key not found: $(SIGNING_KEY)"; \
		echo "Generate with: python3 $(COMMON_DIR)/scripts/generate_signing_key.py"; \
		exit 1; \
	fi
	@mkdir -p $(SIGNED_OUTPUT_DIR)
	@python3 $(SIGN_SCRIPT) $(OUTPUT_DIRECTORY)/$(OUTPUT_NAME).bin $(SIGNING_KEY) $(SIGNED_OUTPUT_DIR)
	@cp $(OUTPUT_DIRECTORY)/$(OUTPUT_NAME).hex $(SIGNED_OUTPUT_DIR)/
	@echo ""
	@echo "=========================================="
	@echo "Signed release package: $(SIGNED_OUTPUT_DIR)/"
	@echo "=========================================="
