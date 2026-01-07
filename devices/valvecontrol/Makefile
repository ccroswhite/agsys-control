# Makefile for Valve Controller
# Provides convenient targets for building, cleaning, and managing the project

.PHONY: all build debug release upload monitor clean clean-all help

# Default target
all: build

# Build default environment (debug)
build:
	pio run

# Build debug environment
debug:
	pio run -e debug

# Build release environment
release:
	pio run -e release

# Upload to device
upload:
	pio run -t upload

# Upload debug build
upload-debug:
	pio run -e debug -t upload

# Upload release build
upload-release:
	pio run -e release -t upload

# Open serial monitor
monitor:
	pio device monitor

# Clean PlatformIO build files
clean:
	pio run -t clean
	rm -rf .pio/build

# Clean all generated and intermediate files
clean-all: clean
	rm -rf .pio
	rm -rf .pioenvs
	rm -rf .piolibdeps
	rm -f *.elf *.hex *.bin
	rm -f *.o *.obj *.d
	rm -f *.log *.tmp *.bak
	# Clean Eagle backup files
	rm -f docs/*.b\#* docs/*.s\#* docs/*.l\#*
	rm -f *.b\#* *.s\#* *.l\#*
	# Clean Eagle CAM/Gerber outputs
	rm -f docs/*.gpi docs/*.dri docs/*.pro
	rm -f docs/*.gbl docs/*.gbo docs/*.gbp docs/*.gbs
	rm -f docs/*.gtl docs/*.gto docs/*.gtp docs/*.gts
	rm -f docs/*.gml docs/*.xln docs/*.drl docs/*.ger

# Show help
help:
	@echo "Valve Controller Build Targets:"
	@echo "  make build        - Build default (debug) environment"
	@echo "  make debug        - Build debug environment"
	@echo "  make release      - Build release environment"
	@echo "  make upload       - Upload to device"
	@echo "  make upload-debug - Upload debug build"
	@echo "  make upload-release - Upload release build"
	@echo "  make monitor      - Open serial monitor"
	@echo "  make clean        - Clean PlatformIO build files"
	@echo "  make clean-all    - Clean all generated files (including Eagle)"
	@echo "  make help         - Show this help"
