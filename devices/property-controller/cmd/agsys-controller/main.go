// AgSys Property Controller
// Main entry point for the property controller service
package main

import (
	"context"
	"encoding/hex"
	"fmt"
	"log"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/spf13/cobra"
	"gopkg.in/yaml.v3"

	"github.com/agsys/property-controller/internal/engine"
)

// Config represents the configuration file structure
type Config struct {
	Property struct {
		UID  string `yaml:"uid"`
		Name string `yaml:"name"`
	} `yaml:"property"`

	Cloud struct {
		GRPCAddr string `yaml:"grpc_addr"`
		APIKey   string `yaml:"api_key"`
		UseTLS   bool   `yaml:"use_tls"`
	} `yaml:"cloud"`

	Controller struct {
		ID string `yaml:"id"`
	} `yaml:"controller"`

	LoRa struct {
		Frequency       uint32 `yaml:"frequency"`
		SpreadingFactor uint8  `yaml:"spreading_factor"`
		Bandwidth       uint32 `yaml:"bandwidth"`
		CodingRate      uint8  `yaml:"coding_rate"`
		TxPower         int8   `yaml:"tx_power"`
		SyncWord        uint8  `yaml:"sync_word"`
		AESKey          string `yaml:"aes_key"`
	} `yaml:"lora"`

	Database struct {
		Path string `yaml:"path"`
	} `yaml:"database"`

	Timing struct {
		SyncInterval     int `yaml:"sync_interval"`
		CommandTimeout   int `yaml:"command_timeout"`
		CommandRetries   int `yaml:"command_retries"`
		TimeSyncInterval int `yaml:"time_sync_interval"`
	} `yaml:"timing"`

	Logging struct {
		Level string `yaml:"level"`
		File  string `yaml:"file"`
	} `yaml:"logging"`
}

var (
	configFile string
	rootCmd    = &cobra.Command{
		Use:   "agsys-controller",
		Short: "AgSys Property Controller",
		Long:  "Property controller for AgSys agricultural IoT system. Manages LoRa devices and cloud communication.",
	}

	runCmd = &cobra.Command{
		Use:   "run",
		Short: "Run the controller service",
		RunE:  runController,
	}

	versionCmd = &cobra.Command{
		Use:   "version",
		Short: "Print version information",
		Run: func(cmd *cobra.Command, args []string) {
			fmt.Println("AgSys Property Controller v0.1.0")
		},
	}
)

func init() {
	rootCmd.PersistentFlags().StringVarP(&configFile, "config", "c", "/etc/agsys/controller.yaml", "Configuration file path")
	rootCmd.AddCommand(runCmd)
	rootCmd.AddCommand(versionCmd)
}

func main() {
	if err := rootCmd.Execute(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func loadConfig(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("failed to read config file: %w", err)
	}

	var cfg Config
	if err := yaml.Unmarshal(data, &cfg); err != nil {
		return nil, fmt.Errorf("failed to parse config file: %w", err)
	}

	return &cfg, nil
}

func runController(cmd *cobra.Command, args []string) error {
	// Load configuration
	cfg, err := loadConfig(configFile)
	if err != nil {
		return fmt.Errorf("failed to load config: %w", err)
	}

	// Validate required fields
	if cfg.Controller.ID == "" {
		return fmt.Errorf("controller.id is required")
	}
	if cfg.Cloud.APIKey == "" {
		return fmt.Errorf("cloud.api_key is required")
	}

	// Parse AES key
	var aesKey []byte
	if cfg.LoRa.AESKey != "" {
		aesKey, err = hex.DecodeString(cfg.LoRa.AESKey)
		if err != nil {
			return fmt.Errorf("invalid AES key: %w", err)
		}
		if len(aesKey) != 16 {
			return fmt.Errorf("AES key must be 16 bytes (32 hex characters)")
		}
	}

	// Build engine config
	engineCfg := engine.DefaultConfig()
	engineCfg.ControllerID = cfg.Controller.ID
	if cfg.Cloud.GRPCAddr != "" {
		engineCfg.GRPCAddr = cfg.Cloud.GRPCAddr
	}
	engineCfg.APIKey = cfg.Cloud.APIKey
	engineCfg.UseTLS = cfg.Cloud.UseTLS
	engineCfg.AESKey = aesKey

	if cfg.Database.Path != "" {
		engineCfg.DatabasePath = cfg.Database.Path
	}
	if cfg.LoRa.Frequency != 0 {
		engineCfg.LoRaFrequency = cfg.LoRa.Frequency
	}
	if cfg.Timing.SyncInterval > 0 {
		engineCfg.SyncInterval = secondsToDuration(cfg.Timing.SyncInterval)
	}
	if cfg.Timing.CommandTimeout > 0 {
		engineCfg.CommandTimeout = secondsToDuration(cfg.Timing.CommandTimeout)
	}
	if cfg.Timing.CommandRetries > 0 {
		engineCfg.CommandRetries = cfg.Timing.CommandRetries
	}
	if cfg.Timing.TimeSyncInterval > 0 {
		engineCfg.TimeSyncInterval = secondsToDuration(cfg.Timing.TimeSyncInterval)
	}

	// Create engine
	eng, err := engine.New(engineCfg)
	if err != nil {
		return fmt.Errorf("failed to create engine: %w", err)
	}

	// Set up signal handling
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// Start engine
	log.Printf("Starting AgSys Property Controller for property %s", cfg.Property.UID)
	if err := eng.Start(ctx); err != nil {
		return fmt.Errorf("failed to start engine: %w", err)
	}

	// Wait for shutdown signal
	sig := <-sigChan
	log.Printf("Received signal %v, shutting down...", sig)

	// Stop engine
	if err := eng.Stop(); err != nil {
		log.Printf("Error during shutdown: %v", err)
	}

	log.Println("Shutdown complete")
	return nil
}

func secondsToDuration(seconds int) time.Duration {
	return time.Duration(seconds) * time.Second
}
