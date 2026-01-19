/**
 * @file TouchController.h
 * @brief Touch sensor controller for 25 CAP1188 capacitive touch sensors over I2C
 * 
 * Protocol v2: Event-driven architecture
 * - Always polls sensors (not just in EXPECT mode)
 * - Emits TOUCH_DOWN/TOUCH_UP events on state changes
 * - Debounces touch inputs for reliable detection
 * 
 * Events:
 *   TOUCH_DOWN <letter> - Touch went from inactive -> active (debounced)
 *   TOUCH_UP <letter>   - Touch went from active -> inactive (debounced)
 */

#ifndef TOUCH_CONTROLLER_H
#define TOUCH_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

// Forward declaration
class EventQueue;

// ============================================================================
// Touch State Per Sensor
// ============================================================================

struct TouchSensorState {
    bool active;              // Whether sensor responded to init
    bool currentTouched;      // Current raw touch state
    bool debouncedTouched;    // Debounced (stable) touch state
    bool lastReportedTouched; // Last state reported via event
    uint32_t lastChangeTime;  // When the raw state last changed
};

// ============================================================================
// Expectation State (for EXPECT_DOWN/EXPECT_UP)
// ============================================================================

struct ExpectState {
    bool active;              // Expectation is active
    uint32_t commandId;       // Command ID to include in response
};

// ============================================================================
// TouchController Class
// ============================================================================

class TouchController {
public:
    /**
     * @brief Construct a new Touch Controller
     */
    TouchController();

    /**
     * @brief Set the event queue for emitting touch events
     * @param eventQueue Pointer to event queue
     */
    void setEventQueue(EventQueue* eventQueue);

    /**
     * @brief Initialize all 25 CAP1188 sensors
     * @return true if at least one sensor was initialized
     */
    bool begin();

    /**
     * @brief Tick the touch controller (non-blocking)
     * Polls sensors, debounces, and emits events
     * Call this every loop iteration
     */
    void tick();

    /**
     * @brief Recalibrate a specific sensor
     * @param sensorIndex Sensor index (0-24)
     * @return true if successful
     */
    bool recalibrate(uint8_t sensorIndex);

    /**
     * @brief Recalibrate all active sensors
     */
    void recalibrateAll();

    /**
     * @brief Set expectation for touch down at position
     * @param sensorIndex Sensor index (0-24)
     * @param commandId Command ID to include in TOUCHED_DOWN response
     */
    void setExpectDown(uint8_t sensorIndex, uint32_t commandId);

    /**
     * @brief Set expectation for touch up at position
     * @param sensorIndex Sensor index (0-24)
     * @param commandId Command ID to include in TOUCHED_UP response
     */
    void setExpectUp(uint8_t sensorIndex, uint32_t commandId);

    /**
     * @brief Clear expectation for touch down at position
     * @param sensorIndex Sensor index (0-24)
     */
    void clearExpectDown(uint8_t sensorIndex);

    /**
     * @brief Clear expectation for touch up at position
     * @param sensorIndex Sensor index (0-24)
     */
    void clearExpectUp(uint8_t sensorIndex);

    /**
     * @brief Build comma-separated list of active sensor letters
     * @param buffer Output buffer (should be at least 52 chars)
     * @param bufferSize Size of buffer
     */
    void buildActiveSensorList(char* buffer, size_t bufferSize) const;

    /**
     * @brief Check if a sensor is active (initialized successfully)
     * @param sensorIndex Sensor index (0-24)
     * @return true if active
     */
    bool isSensorActive(uint8_t sensorIndex) const;

    /**
     * @brief Get the current debounced touch state of a sensor
     * @param sensorIndex Sensor index (0-24)
     * @return true if touched
     */
    bool isTouched(uint8_t sensorIndex) const;

    /**
     * @brief Get number of active sensors
     * @return Count of successfully initialized sensors
     */
    uint8_t getActiveSensorCount() const;

    // === Utility Methods ===

    /**
     * @brief Convert sensor letter (A-Y) to index (0-24)
     * @param letter Letter A-Y (case-insensitive)
     * @return Index 0-24, or 255 if invalid
     */
    static uint8_t letterToIndex(char letter);

    /**
     * @brief Convert sensor index (0-24) to letter (A-Y)
     * @param index Index 0-24
     * @return Letter A-Y (uppercase), or '?' if invalid
     */
    static char indexToLetter(uint8_t index);

    /**
     * @brief Convert I2C address to sensor index
     * @param address I2C address
     * @return Sensor index 0-24, or 255 if not found
     */
    static uint8_t addressToIndex(uint8_t address);

private:
    // Event queue for emitting events
    EventQueue* m_eventQueue;

    // Per-sensor state
    TouchSensorState m_sensors[NUM_TOUCH_SENSORS];

    // Expectation tracking for EXPECT_DOWN/EXPECT_UP commands
    ExpectState m_expectDown[NUM_TOUCH_SENSORS];
    ExpectState m_expectUp[NUM_TOUCH_SENSORS];

    // Timestamp of last sensor poll
    uint32_t m_lastPollTime;

    // Number of successfully initialized sensors
    uint8_t m_activeSensorCount;

    // === I2C Methods ===

    /**
     * @brief Initialize a single CAP1188 sensor
     * @param address I2C address
     * @return true if successful
     */
    bool initSensor(uint8_t address);

    /**
     * @brief Read a register from a CAP1188 sensor
     * @param address I2C address
     * @param reg Register address
     * @param value Output value
     * @return true if successful
     */
    bool readRegister(uint8_t address, uint8_t reg, uint8_t& value);

    /**
     * @brief Write a register to a CAP1188 sensor
     * @param address I2C address
     * @param reg Register address
     * @param value Value to write
     * @return true if successful
     */
    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);

    /**
     * @brief Read raw touch state from a sensor
     * @param address I2C address
     * @return true if CS1 is touched
     */
    bool readRawTouch(uint8_t address);

    /**
     * @brief Try to recover a stuck I2C bus
     */
    void recoverI2CBus();

    /**
     * @brief Poll all sensors and update raw states
     */
    void pollSensors();

    /**
     * @brief Process debouncing and emit events
     */
    void processDebounce();
};

#endif // TOUCH_CONTROLLER_H
