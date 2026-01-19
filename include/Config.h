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

// Time between touch sensor polls (ms)
constexpr uint16_t TOUCH_POLL_INTERVAL_MS = 10;

// Debounce time - sensor must be stable for this duration (ms)
constexpr uint16_t DEBOUNCE_MS = 30;

// Number of touch sensors (A-Y = 25 sensors)
constexpr uint8_t NUM_TOUCH_SENSORS = 25;

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
constexpr uint8_t SUCCESS_EXPANSION_RADIUS = 5;    // Max LEDs on each side
constexpr uint16_t ANIMATION_STEP_MS = 80;         // Time between expansion steps

// ============================================================================
// Colors (RGB format)
// ============================================================================

// SHOW = Blue
constexpr uint8_t COLOR_SHOW_R = 0;
constexpr uint8_t COLOR_SHOW_G = 0;
constexpr uint8_t COLOR_SHOW_B = 255;

// SUCCESS = Green
constexpr uint8_t COLOR_SUCCESS_R = 0;
constexpr uint8_t COLOR_SUCCESS_G = 255;
constexpr uint8_t COLOR_SUCCESS_B = 0;

// BLINK = Orange/Amber (indicates "release me!")
constexpr uint8_t COLOR_BLINK_R = 255;
constexpr uint8_t COLOR_BLINK_G = 100;
constexpr uint8_t COLOR_BLINK_B = 0;

// OFF = Black
constexpr uint8_t COLOR_OFF_R = 0;
constexpr uint8_t COLOR_OFF_G = 0;
constexpr uint8_t COLOR_OFF_B = 0;

// ============================================================================
// I2C Configuration
// ============================================================================

// I2C clock speed (Hz)
constexpr uint32_t I2C_CLOCK_SPEED = 100000;

// CAP1188 Register addresses
constexpr uint8_t CAP1188_REG_MAIN_CONTROL = 0x00;
constexpr uint8_t CAP1188_REG_SENSITIVITY_CONTROL = 0x1F;
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_STATUS = 0x03;
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_ENABLE = 0x21;
constexpr uint8_t CAP1188_REG_CALIBRATION_ACTIVE = 0x26;

// CS1 bit mask (only using CS1 channel)
constexpr uint8_t CS1_BIT_MASK = 0x01;

// Default sensitivity level (0 = most sensitive, 7 = least sensitive)
constexpr uint8_t DEFAULT_SENSITIVITY = 0;

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
// ==================================