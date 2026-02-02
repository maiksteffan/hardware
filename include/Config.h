/**
 * @file Config.h
 * @brief Configuration constants for the LED/Touch controller firmware
 * 
 * Protocol version 2.0 - Event-driven architecture
 * Arduino acts as "dumb" hardware executor + event source
 * All game logic resides on Raspberry Pi
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// Firmware Information
// ============================================================================

#define FIRMWARE_VERSION "2.0.0"
#define PROTOCOL_VERSION "2"

// ============================================================================
// Serial Protocol Configuration
// ============================================================================

// Maximum length of a command line (including null terminator)
// Increased from 32 to support command IDs
constexpr size_t MAX_LINE_LEN = 64;

// Serial baud rate
constexpr uint32_t SERIAL_BAUD_RATE = 115200;

// ============================================================================
// Queue Sizes
// ============================================================================

// Maximum number of commands that can be queued
constexpr uint8_t COMMAND_QUEUE_SIZE = 8;

// Maximum number of outgoing events that can be queued
constexpr uint8_t EVENT_QUEUE_SIZE = 16;

// ============================================================================
// Touch Sensing Configuration
// ============================================================================

// Time between touch sensor polls (ms) - faster polling = better responsiveness
constexpr uint16_t TOUCH_POLL_INTERVAL_MS = 5;  // Poll every 5ms

// Debounce time - sensor must be stable for this duration (ms)
// Lower = more responsive, Higher = more stable
constexpr uint16_t DEBOUNCE_MS = 20;  // Reduced from 30ms for better responsiveness

// Number of touch sensors (A-Y = 25 sensors)
constexpr uint8_t NUM_TOUCH_SENSORS = 25;

// Number of steps ahead to recalibrate sensors before they're needed
// Set to 0 to disable pre-recalibration (recommended if sensors are touched during sequence)
constexpr uint8_t RECALIBRATE_STEPS_AHEAD = 0;  // DISABLED - sensors are touched during sequence

// ============================================================================
// LED Configuration
// ============================================================================

// Number of logical LED positions (A-Y)
constexpr uint8_t NUM_POSITIONS = 25;

// LED strip data pins
constexpr uint8_t STRIP1_PIN = 5;   // D5
constexpr uint8_t STRIP2_PIN = 10;  // D10

// LED counts per strip (can be overridden via build flags)
#ifndef NUM_LEDS_STRIP1
#define NUM_LEDS_STRIP1 190
#endif

#ifndef NUM_LEDS_STRIP2
#define NUM_LEDS_STRIP2 190
#endif

// Overall brightness (0-255)
constexpr uint8_t LED_BRIGHTNESS = 128;

// Animation settings
constexpr uint8_t SUCCESS_EXPANSION_RADIUS = 5;    // Max LEDs on each side (legacy, not used for pulse)
constexpr uint8_t SUCCESS_PULSE_COUNT = 2;         // Number of pulse cycles
constexpr uint16_t SUCCESS_PULSE_STEPS = 20;       // Steps per half-pulse (fade in or out)

// ANIMATION_STEP_MS can be overridden via SUCCESS_ANIMATION_SPEED_MS in main.cpp
#ifdef SUCCESS_ANIMATION_SPEED_MS
constexpr uint16_t ANIMATION_STEP_MS = SUCCESS_ANIMATION_SPEED_MS;
#else
constexpr uint16_t ANIMATION_STEP_MS = 25;         // Time between animation steps (ms)
#endif

// ============================================================================
// Colors (RGB format)
// ============================================================================

// SHOW = Blue
constexpr uint8_t COLOR_SHOW_R = 0;
constexpr uint8_t COLOR_SHOW_G = 0;
constexpr uint8_t COLOR_SHOW_B = 255;

// SUCCESS = Green (indicates correct hold)
constexpr uint8_t COLOR_SUCCESS_R = 0;
constexpr uint8_t COLOR_SUCCESS_G = 255;
constexpr uint8_t COLOR_SUCCESS_B = 0;

// BLINK = Green (indicates let go)
constexpr uint8_t COLOR_BLINK_R = 0;
constexpr uint8_t COLOR_BLINK_G = 100;
constexpr uint8_t COLOR_BLINK_B = 0;

// MISTAKE = Red (indicates wrong hold)
constexpr uint8_t COLOR_MISTAKE_R = 255;
constexpr uint8_t COLOR_MISTAKE_G = 0;
constexpr uint8_t COLOR_MISTAKE_B = 0;

// OFF = Black
constexpr uint8_t COLOR_OFF_R = 0;
constexpr uint8_t COLOR_OFF_G = 0;
constexpr uint8_t COLOR_OFF_B = 0;

// ============================================================================
// I2C Configuration
// ============================================================================

// I2C clock speed (Hz) - use standard speed for reliability
constexpr uint32_t I2C_CLOCK_SPEED = 100000;  // Standard 100kHz

// I2C retry settings
constexpr uint8_t I2C_MAX_RETRIES = 3;       // Number of retries on I2C failure
constexpr uint16_t I2C_RETRY_DELAY_US = 100; // Delay between retries (microseconds)

// CAP1188 Register addresses (from datasheet and Adafruit library)
constexpr uint8_t CAP1188_REG_MAIN_CONTROL = 0x00;
constexpr uint8_t CAP1188_REG_GENERAL_STATUS = 0x02;
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_STATUS = 0x03;
constexpr uint8_t CAP1188_REG_NOISE_FLAG_STATUS = 0x0A;
constexpr uint8_t CAP1188_REG_SENSITIVITY_CONTROL = 0x1F;
constexpr uint8_t CAP1188_REG_CONFIG1 = 0x20;
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_ENABLE = 0x21;
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_CONFIG1 = 0x22;
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_CONFIG2 = 0x23;
constexpr uint8_t CAP1188_REG_AVERAGING_SAMPLING = 0x24;
constexpr uint8_t CAP1188_REG_CALIBRATION_ACTIVE = 0x26;
constexpr uint8_t CAP1188_REG_INTERRUPT_ENABLE = 0x27;
constexpr uint8_t CAP1188_REG_REPEAT_ENABLE = 0x28;
constexpr uint8_t CAP1188_REG_MULTIPLE_TOUCH_CONFIG = 0x2A;  // MTBLK - important!
constexpr uint8_t CAP1188_REG_MULTIPLE_TOUCH_PATTERN = 0x2B;
constexpr uint8_t CAP1188_REG_RECALIBRATION_CONFIG = 0x2F;
constexpr uint8_t CAP1188_REG_SENSOR_THRESHOLD_1 = 0x30;
constexpr uint8_t CAP1188_REG_STANDBY_CONFIG = 0x41;  // STANDBYCFG - Adafruit sets 0x30
constexpr uint8_t CAP1188_REG_STANDBY_SENSITIVITY = 0x42;
constexpr uint8_t CAP1188_REG_STANDBY_THRESHOLD = 0x43;
constexpr uint8_t CAP1188_REG_SENSOR_BASE_COUNT = 0x50;  // Base count for CS1
constexpr uint8_t CAP1188_REG_LED_LINK = 0x72;  // LED linking to sensor
constexpr uint8_t CAP1188_REG_PRODUCT_ID = 0xFD;
constexpr uint8_t CAP1188_REG_MANUFACTURER_ID = 0xFE;
constexpr uint8_t CAP1188_REG_REVISION = 0xFF;

// CS1 bit mask (only using CS1 channel)
constexpr uint8_t CS1_BIT_MASK = 0x01;

// Default sensitivity level (0 = most sensitive, 7 = least sensitive)
// DELTA_SENSE multiplier: 0=128x, 1=64x, 2=32x, 3=16x, 4=8x, 5=4x, 6=2x, 7=1x
// Using 0 (128x) for MAXIMUM sensitivity to detect touches reliably
constexpr uint8_t DEFAULT_SENSITIVITY = 0;

// Touch threshold (lower = more sensitive, range 0x00-0x7F)
// This is the delta threshold - how much the reading must change to register a touch
// 0x10 (16) is very sensitive, 0x20 (32) is sensitive, 0x40 (64) is moderate
constexpr uint8_t DEFAULT_TOUCH_THRESHOLD = 0x10;  // 16 - very sensitive for reliable detection

// Averaging/Sampling config: balance between stability and responsiveness
// Bits 6:4 = avg (0=1, 1=2, 2=4, 3=8, 4=16, 5=32, 6=64, 7=128 samples)
// Bits 3:2 = sample time (0=320us, 1=640us, 2=1.28ms, 3=2.56ms)
// Bits 1:0 = cycle time (0=35ms, 1=70ms, 2=105ms, 3=140ms)
// 0x25 = 4 samples, 640us sample time, 70ms cycle - good balance
constexpr uint8_t DEFAULT_AVG_SAMPLING = 0x25;

// Sensor initialization delay (ms) - time for CAP1188 to stabilize after power-on
// CAP1188 datasheet recommends waiting for internal oscillator to stabilize
constexpr uint16_t SENSOR_INIT_DELAY_MS = 500;  // Increased for reliability

// Post-init recalibration delay (ms) - time after init before recalibrating all
// Longer = more stable baseline, but slower startup
constexpr uint16_t POST_INIT_RECAL_DELAY_MS = 1500;  // 1.5 seconds for stable baseline

// ============================================================================
// I2C Address Mapping for Sensors A-Y
// ============================================================================

constexpr uint8_t SENSOR_I2C_ADDRESSES[NUM_TOUCH_SENSORS] = {
    0x1F,  // A
    0x1E,  // B
    0x1D,  // C
    0x1C,  // D
    0x3F,  // E
    0x1A,  // F
    0x28,  // G
    0x29,  // H
    0x2A,  // I
    0x0E,  // J
    0x0F,  // K
    0x18,  // L
    0x19,  // M
    0x3C,  // N
    0x2F,  // O
    0x38,  // P
    0x0D,  // Q
    0x0C,  // R
    0x0B,  // S
    0x3E,  // T
    0x2C,  // U
    0x3D,  // V
    0x08,  // W
    0x09,  // X
    0x0A   // Y
};

// ============================================================================
// Command IDs
// ============================================================================

// Special value indicating no command ID was provided
constexpr uint32_t NO_COMMAND_ID = 0xFFFFFFFF;

#endif // CONFIG_H
