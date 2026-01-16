# AgSys Bootloader Makefile
#
# Builds bare-metal bootloader for nRF52832
# No Nordic SDK, no FreeRTOS dependencies

# Toolchain
PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)as
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
SIZE = $(PREFIX)size

# Target
TARGET = agsys_bootloader

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
LINKER_DIR = linker

# Include paths (reuse shared headers from freertos-common)
FREERTOS_COMMON = ../freertos-common/include

INCLUDES = \
    -I$(INC_DIR) \
    -I$(FREERTOS_COMMON)

# Source files
SRCS = \
    $(SRC_DIR)/startup.c \
    $(SRC_DIR)/main.c \
    $(SRC_DIR)/bl_hal_nrf52.c \
    $(SRC_DIR)/bl_crc32.c \
    $(SRC_DIR)/bl_flash.c \
    $(SRC_DIR)/bl_log.c \
    $(SRC_DIR)/bl_ed25519.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Compiler flags
CFLAGS = \
    -mcpu=cortex-m4 \
    -mthumb \
    -mfloat-abi=hard \
    -mfpu=fpv4-sp-d16 \
    -Os \
    -g3 \
    -Wall \
    -Wextra \
    -Werror \
    -ffunction-sections \
    -fdata-sections \
    -fno-strict-aliasing \
    -fno-builtin \
    -fshort-enums \
    -DNRF52832_XXAA \
    $(INCLUDES)

# Linker flags
LDFLAGS = \
    -mcpu=cortex-m4 \
    -mthumb \
    -mfloat-abi=hard \
    -mfpu=fpv4-sp-d16 \
    -T$(LINKER_DIR)/bootloader.ld \
    -Wl,--gc-sections \
    -Wl,-Map=$(BUILD_DIR)/$(TARGET).map \
    --specs=nano.specs \
    --specs=nosys.specs \
    -lc \
    -lm \
    -lnosys

# Adafruit Feather nRF52832 pin overrides
# Uncomment to build for Feather instead of custom AgSys boards
#CFLAGS += -DBL_PIN_SPI_SCK=12
#CFLAGS += -DBL_PIN_SPI_MOSI=13
#CFLAGS += -DBL_PIN_SPI_MISO=14
#CFLAGS += -DBL_PIN_FRAM_CS=15
#CFLAGS += -DBL_PIN_FLASH_CS=16
#CFLAGS += -DBL_PIN_LED=17

# Development mode: skip signature enforcement (warn but don't fail)
# Usage: make DEV=1
ifdef DEV
  CFLAGS += -DBL_DEV_MODE=1
  TARGET = agsys_bootloader_dev
endif

.PHONY: all clean flash dev

all: $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin
	@echo "Build complete!"
	@$(SIZE) $(BUILD_DIR)/$(TARGET).elf

# Convenience target for dev build
dev:
	$(MAKE) DEV=1

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
	$(OBJDUMP) -h -S $@ > $(BUILD_DIR)/$(TARGET).lst

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

# Flash using nrfjprog (requires Nordic tools)
flash: $(BUILD_DIR)/$(TARGET).hex
	nrfjprog --program $< --sectorerase --verify
	nrfjprog --reset

# Flash using OpenOCD
flash-openocd: $(BUILD_DIR)/$(TARGET).hex
	openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg \
		-c "program $< verify reset exit"

# Print size information
size: $(BUILD_DIR)/$(TARGET).elf
	$(SIZE) -A -x $<
