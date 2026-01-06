# Makefile for Soil Moisture Sensor Firmware
# Target: SAMD21 (ARM Cortex-M0+)

# Toolchain
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE = arm-none-eabi-size

# Project name
PROJECT = soilmoisture

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Include paths
INCLUDES = -I$(INC_DIR)

# MCU flags
MCU = -mcpu=cortex-m0plus -mthumb

# Compiler flags
CFLAGS = $(MCU)
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Os
CFLAGS += -g3
CFLAGS += -std=c11
CFLAGS += $(INCLUDES)
CFLAGS += -D__SAMD21G18A__

# Linker flags
LDFLAGS = $(MCU)
LDFLAGS += -T samd21g18a.ld
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map
LDFLAGS += --specs=nano.specs
LDFLAGS += --specs=nosys.specs
LDFLAGS += -lc -lm

# Default target
all: $(BUILD_DIR) $(BUILD_DIR)/$(PROJECT).elf $(BUILD_DIR)/$(PROJECT).hex $(BUILD_DIR)/$(PROJECT).bin size

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link
$(BUILD_DIR)/$(PROJECT).elf: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

# Create hex file
$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O ihex $< $@

# Create binary file
$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJCOPY) -O binary $< $@

# Display size
size: $(BUILD_DIR)/$(PROJECT).elf
	$(SIZE) $<

# Disassembly
disasm: $(BUILD_DIR)/$(PROJECT).elf
	$(OBJDUMP) -d -S $< > $(BUILD_DIR)/$(PROJECT).lss

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Flash using OpenOCD
flash: $(BUILD_DIR)/$(PROJECT).bin
	openocd -f interface/cmsis-dap.cfg -f target/at91samdXX.cfg \
		-c "program $(BUILD_DIR)/$(PROJECT).bin verify reset exit"

# Flash using JLink
flash-jlink: $(BUILD_DIR)/$(PROJECT).bin
	JLinkExe -device ATSAMD21G18 -if SWD -speed 4000 \
		-CommanderScript flash.jlink

# Debug with GDB
debug: $(BUILD_DIR)/$(PROJECT).elf
	arm-none-eabi-gdb -x gdbinit $<

.PHONY: all clean flash flash-jlink debug size disasm
