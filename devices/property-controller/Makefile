# Makefile for AgSys Property Controller

.PHONY: all build clean test install deps lint fmt help

# Build output directory
BIN_DIR := bin

# Binary names
CONTROLLER := agsys-controller
DB_CLI := agsys-db

# Default target
all: build

# Download dependencies
deps:
	go mod tidy
	go mod download

# Build all binaries
build: deps
	@mkdir -p $(BIN_DIR)
	go build -o $(BIN_DIR)/$(CONTROLLER) ./cmd/agsys-controller
	go build -o $(BIN_DIR)/$(DB_CLI) ./cmd/agsys-db

# Build for Raspberry Pi 5 (ARM64)
build-pi5: deps
	@mkdir -p $(BIN_DIR)
	GOOS=linux GOARCH=arm64 go build -o $(BIN_DIR)/$(CONTROLLER)-arm64 ./cmd/agsys-controller
	GOOS=linux GOARCH=arm64 go build -o $(BIN_DIR)/$(DB_CLI)-arm64 ./cmd/agsys-db

# Build for Raspberry Pi 3/4 32-bit (ARM)
build-pi: deps
	@mkdir -p $(BIN_DIR)
	GOOS=linux GOARCH=arm GOARM=7 go build -o $(BIN_DIR)/$(CONTROLLER)-arm ./cmd/agsys-controller
	GOOS=linux GOARCH=arm GOARM=7 go build -o $(BIN_DIR)/$(DB_CLI)-arm ./cmd/agsys-db

# Run tests
test:
	go test -v ./...

# Format code
fmt:
	gofmt -w .
	@echo "Code formatted"

# Run linter
lint: fmt
	go vet ./...
	@which golangci-lint > /dev/null && golangci-lint run || echo "golangci-lint not installed"

# Install binaries to system
install: build
	sudo cp $(BIN_DIR)/$(CONTROLLER) /usr/local/bin/
	sudo cp $(BIN_DIR)/$(DB_CLI) /usr/local/bin/
	sudo mkdir -p /etc/agsys /var/lib/agsys /var/log/agsys
	@if [ ! -f /etc/agsys/controller.yaml ]; then \
		sudo cp configs/config.yaml /etc/agsys/controller.yaml; \
	fi

# Install systemd service
install-service: install
	sudo cp configs/agsys-controller.service /etc/systemd/system/
	sudo systemctl daemon-reload
	@echo "Service installed. Edit /etc/agsys/controller.yaml then run:"
	@echo "  sudo systemctl enable agsys-controller"
	@echo "  sudo systemctl start agsys-controller"

# Clean build artifacts
clean:
	rm -rf $(BIN_DIR)
	go clean

# Clean all including dependencies
clean-all: clean
	go clean -modcache

# Run controller locally (for development)
run: build
	./$(BIN_DIR)/$(CONTROLLER) run --config configs/config.yaml

# Show help
help:
	@echo "AgSys Property Controller Build Targets:"
	@echo ""
	@echo "  make build       - Build binaries for current platform"
	@echo "  make build-pi5   - Cross-compile for Raspberry Pi 5 (ARM64)"
	@echo "  make build-pi    - Cross-compile for Raspberry Pi 3/4 (ARM)"
	@echo "  make deps        - Download dependencies"
	@echo "  make test        - Run tests"
	@echo "  make fmt         - Format code with gofmt"
	@echo "  make lint        - Format and run linter"
	@echo "  make install     - Install binaries to /usr/local/bin"
	@echo "  make install-service - Install systemd service"
	@echo "  make clean       - Clean build artifacts"
	@echo "  make clean-all   - Clean including module cache"
	@echo "  make run         - Run controller locally"
	@echo "  make help        - Show this help"
