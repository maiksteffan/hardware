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

constexpr size_t MAX_LINE_LEN = 64;
constexpr uint32_t SERIAL_BAUD_RATE = 115200;

// ============================================================================
// Queue Sizes
// ============================================================================

constexpr uint8_t COMMAND_QUEUE_SIZE = 8;
constexpr uint8_t EVENT_QUEUE_SIZE = 16;

// ============================================================================
// Touch Sensing Configuration
// ============================================================================

constexpr uint16_t TOUCH_POLL_INTERVAL_MS = 5;
constexpr uint16_t DEBOUNCE_MS = 20;
constexpr uint8_t NUM_TOUCH_SENSORS = 25;

// ============================================================================
// LED Configuration
// ============================================================================

constexpr uint8_t NUM_POSITIONS = 25;

constexpr uint8_t STRIP1_PIN = 5;
constexpr uint8_t STRIP2_PIN = 10;

#ifndef NUM_LEDS_STRIP1
#define NUM_LEDS_STRIP1 190
#endif

#ifndef NUM_LEDS_STRIP2
#define NUM_LEDS_STRIP2 190
#endif

constexpr uint8_t LED_BRIGHTNESS = 128;

// Animation settings
constexpr uint8_t SUCCESS_PULSE_COUNT = 2;
constexpr uint16_t SUCCESS_PULSE_STEPS = 20;
constexpr uint16_t ANIMATION_STEP_MS = 25;
constexpr uint16_t BLINK_INTERVAL_MS = 150;

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

// BLINK = Orange (fast blink for "release me")
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

constexpr uint32_t I2C_CLOCK_SPEED = 100000;
constexpr uint8_t I2C_MAX_RETRIES = 3;
constexpr uint16_t I2C_RETRY_DELAY_US = 100;

// CAP1188 Register addresses
constexpr uint8_t CAP1188_REG_MAIN_CONTROL = 0x00;
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_STATUS = 0x03;
constexpr uint8_t CAP1188_REG_SENSITIVITY_CONTROL = 0x1F;
constexpr uint8_t CAP1188_REG_CONFIG1 = 0x20;
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_ENABLE = 0x21;
constexpr uint8_t CAP1188_REG_AVERAGING_SAMPLING = 0x24;
constexpr uint8_t CAP1188_REG_CALIBRATION_ACTIVE = 0x26;
constexpr uint8_t CAP1188_REG_INTERRUPT_ENABLE = 0x27;
constexpr uint8_t CAP1188_REG_REPEAT_ENABLE = 0x28;
constexpr uint8_t CAP1188_REG_MULTIPLE_TOUCH_CONFIG = 0x2A;
constexpr uint8_t CAP1188_REG_SENSOR_THRESHOLD_1 = 0x30;
constexpr uint8_t CAP1188_REG_STANDBY_CONFIG = 0x41;
constexpr uint8_t CAP1188_REG_LED_LINK = 0x72;
constexpr uint8_t CAP1188_REG_PRODUCT_ID = 0xFD;
constexpr uint8_t CAP1188_REG_MANUFACTURER_ID = 0xFE;
constexpr uint8_t CAP1188_REG_REVISION = 0xFF;

constexpr uint8_t CS1_BIT_MASK = 0x01;
constexpr uint8_t DEFAULT_SENSITIVITY = 0;
constexpr uint8_t DEFAULT_TOUCH_THRESHOLD = 0x10;
constexpr uint8_t DEFAULT_AVG_SAMPLING = 0x25;

constexpr uint16_t SENSOR_INIT_DELAY_MS = 500;
constexpr uint16_t POST_INIT_RECAL_DELAY_MS = 1500;

// ============================================================================
// I2C Address Mapping for Sensors A-Y
// ============================================================================

constexpr uint8_t SENSOR_I2C_ADDRESSES[NUM_TOUCH_SENSORS] = {
    0x1F, 0x1E, 0x1D, 0x1C, 0x3F,  // A-E
    0x1A, 0x28, 0x29, 0x2A, 0x0E,  // F-J
    0x0F, 0x18, 0x19, 0x3C, 0x2F,  // K-O
    0x38, 0x0D, 0x0C, 0x0B, 0x3E,  // P-T
    0x2C, 0x3D, 0x08, 0x09, 0x0A   // U-Y
};

// ============================================================================
// Command IDs
// ============================================================================

constexpr uint32_t NO_COMMAND_ID = 0xFFFFFFFF;

#endif // CONFIG_H
