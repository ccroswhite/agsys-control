# =============================================================================
# AgSys Build Configuration
# =============================================================================
# Usage:
#   make                    - Development build
#   make RELEASE=1          - Release build
#   make PRODUCTION=1       - Production build (release + APPROTECT)
#
# Output naming:
#   Development: <device>-<githash6>-<YYYYMMDD-HHMM>
#   Release:     agsys-<device>-<version>-<YYYYMMDD-HHMM>
#   Production:  agsys-<device>-<version>-<YYYYMMDD-HHMM>-prod
# =============================================================================

# Version (update for each release)
VERSION_MAJOR := 1
VERSION_MINOR := 0
VERSION_PATCH := 0
VERSION := $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

# Get git hash (last 6 characters)
GIT_HASH := $(shell git rev-parse --short=6 HEAD 2>/dev/null || echo "000000")

# Get build timestamp
BUILD_DATE := $(shell date +%Y%m%d-%H%M)

# Device name (set by each device Makefile before including this)
# DEVICE_NAME := soilmoisture

# Build type detection
ifdef PRODUCTION
  RELEASE := 1
  BUILD_TYPE := production
  OUTPUT_NAME := agsys-$(DEVICE_NAME)-$(VERSION)-$(BUILD_DATE)-prod
else ifdef RELEASE
  BUILD_TYPE := release
  OUTPUT_NAME := agsys-$(DEVICE_NAME)-$(VERSION)-$(BUILD_DATE)
else
  BUILD_TYPE := development
  OUTPUT_NAME := $(DEVICE_NAME)-$(GIT_HASH)-$(BUILD_DATE)
endif

# C preprocessor defines for version info
CFLAGS += -DAGSYS_VERSION_MAJOR=$(VERSION_MAJOR)
CFLAGS += -DAGSYS_VERSION_MINOR=$(VERSION_MINOR)
CFLAGS += -DAGSYS_VERSION_PATCH=$(VERSION_PATCH)
CFLAGS += -DAGSYS_VERSION_STRING=\"$(VERSION)\"
CFLAGS += -DAGSYS_GIT_HASH=\"$(GIT_HASH)\"
CFLAGS += -DAGSYS_BUILD_DATE=\"$(BUILD_DATE)\"
CFLAGS += -DAGSYS_DEVICE_NAME=\"$(DEVICE_NAME)\"

# Build type specific flags
ifeq ($(BUILD_TYPE),development)
  CFLAGS += -DDEBUG
  CFLAGS += -DAGSYS_BUILD_DEV=1
  # Keep debug symbols
  CFLAGS += -g3
else
  CFLAGS += -DNDEBUG
  CFLAGS += -DAGSYS_BUILD_RELEASE=1
  # Optimize for size
  CFLAGS += -Os
endif

# Production-specific: Enable APPROTECT
ifeq ($(BUILD_TYPE),production)
  CFLAGS += -DAGSYS_BUILD_PRODUCTION=1
  CFLAGS += -DAGSYS_ENABLE_APPROTECT=1
endif

# Print build info
$(info ========================================)
$(info AgSys Build Configuration)
$(info ----------------------------------------)
$(info Device:     $(DEVICE_NAME))
$(info Version:    $(VERSION))
$(info Git Hash:   $(GIT_HASH))
$(info Build Date: $(BUILD_DATE))
$(info Build Type: $(BUILD_TYPE))
$(info Output:     $(OUTPUT_NAME))
$(info ========================================)

# Output naming is handled by post-build in each device Makefile
# The Nordic SDK hardcodes output names based on TARGETS variable

# =============================================================================
# Firmware Signing Configuration
# =============================================================================
# Signing key location (relative to COMMON_DIR)
SIGNING_KEY := $(COMMON_DIR)/keys/signing_key.pem
SIGN_SCRIPT := $(COMMON_DIR)/scripts/sign_firmware.py

# Signed output directory (created by sign target)
SIGNED_OUTPUT_DIR := $(OUTPUT_DIRECTORY)/$(OUTPUT_NAME)
