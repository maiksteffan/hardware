/**
 * @file TouchController.h
 * @brief Touch sensor controller for 25 CAP1188 capacitive touch sensors over I2C
 * 
 * Manages 25 CAP1188 sensors, each with a unique I2C address using only the CS1 channel.
 * Supports EXPECT mode (wait for specific sensor) and RECORD mode (capture first touch).
 * 
 * Protocol:
 *   Output: "TOUCHED <letter>\n" when expected sensor is touched
 *           "RECORDED <letter>\n" when any sensor touched in record mode
 */

#ifndef TOUCH_CONTROLLER_H
#define TOUCH_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>


// Forward declaration for callback
class SequenceController;
// ============================================================================
// Configuration
// ============================================================================

// Number of touch sensors (A-Y = 25 sensors)
constexpr uint8_t NUM_TOUCH_SENSORS = 25;

// CAP1188 Register addresses
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_STATUS = 0x03;  // Read touch status
constexpr uint8_t CAP1188_REG_SENSITIVITY_CONTROL = 0x01;  // Sensitivity (0-7)
constexpr uint8_t CAP1188_REG_SENSOR_INPUT_ENABLE = 0x21;  // Enable/disable inputs
constexpr uint8_t CAP1188_REG_CALIBRATION_ACTIVE = 0x26;   // Trigger recalibration
constexpr uint8_t CAP1188_REG_MAIN_CONTROL = 0x00;         // Main control register

// CS1 bit mask (only using CS1 channel)
constexpr uint8_t CS1_BIT_MASK = 0x01;

// Default sensitivity level (0 = most sensitive, 7 = least sensitive)
constexpr uint8_t DEFAULT_SENSITIVITY = 0;

// Minimum time between touch reads (ms) to prevent excessive I2C traffic
constexpr uint16_t TOUCH_SCAN_INTERVAL_MS = 20;

// ============================================================================
// I2C Address Mapping for Sensors A-Y
// ============================================================================

/**
 * I2C addresses for each sensor position (A-Y).
 * CAP1188 default address is 0x29, with A0/A1 pins allowing addresses 0x28-0x2B.
 * For 25 sensors, we use addresses 0x28-0x40 (adjust as per your hardware setup).
 * 
 * Modify this array to match your physical wiring!
 */
constexpr uint8_t SENSOR_I2C_ADDRESSES[NUM_TOUCH_SENSORS] = {
    0x1F,  // A  (old P)
    0x1E,  // B  (old O)
    0x1D,  // C  (old N)
    0x1C,  // D  (old M)
    0x3F,  // E  (old L)
    0x1A,  // F  (old K)
    0x28,  // G  (old Q)  <-- assumed fix (Q->G)
    0x29,  // H  (old R)
    0x2A,  // I  (old S)
    0x0E,  // J  (old G)
    0x0F,  // K  (old H)
    0x18,  // L  (old I)
    0x19,  // M  (old J)
    0x3C,  // N  (old W)  <-- assumed
    0x2F,  // O  (old X)  <-- assumed
    0x38,  // P  (old Y)  <-- assumed
    0x0D,  // Q  (old F)
    0x0C,  // R  (old E)
    0x0B,  // S  (old D)
    0x3E,  // T  (old T)
    0x2C,  // U  (old U)
    0x3D,  // V  (old V)
    0x08,  // W  (old A)
    0x09,  // X  (old B)
    0x0A   // Y  (old C)
};


// ============================================================================
// Controller Mode
// ============================================================================

enum class TouchMode : uint8_t {
    IDLE,       // Normal operation, ignore touches
    EXPECT,     // Waiting for a specific sensor touch
    RECORD      // Waiting for any sensor touch (first touch wins)
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
     * @brief Initialize all 25 CAP1188 sensors
     * Enables only CS1 on each sensor and sets default sensitivity.
     * Call this in setup() after Wire.begin()
     * @return true if at least one sensor was initialized successfully
     */
    bool begin();

    /**
     * @brief Update touch state (non-blocking)
     * Scans all sensors for touch input and handles EXPECT/RECORD modes.
     * Call this every loop iteration.
     */
    void update();

    /**
     * @brief Set up EXPECT mode for a specific sensor
     * When the expected sensor is touched, sends "TOUCHED <letter>"
     * @param letter Sensor letter (A-Y, case-insensitive)
     * @return true if valid letter, false otherwise
     */
    bool expectSensor(char letter);

    /**
     * @brief Enable RECORD mode to listen for first touch on any sensor
     * When any sensor is touched, sends "RECORDED <letter>"
     */
    void startRecording();

    /**
     * @brief Cancel any active EXPECT or RECORD operation
     */
    void cancelOperation();

    /**
     * @brief Check if controller is in IDLE mode
     * @return true if idle (not expecting or recording)
     */
    bool isIdle() const;

    /**
     * @brief Scan and print all active I2C addresses to Serial
     * Useful for debugging and verifying hardware connections
     */
    void scanAddresses();

    /**
     * @brief Set sensitivity for a specific sensor
     * @param sensorIndex Sensor index (0-24, corresponding to A-Y)
     * @param level Sensitivity level (0 = most sensitive, 7 = least sensitive)
     * @return true if write succeeded
     */
    bool setSensitivity(uint8_t sensorIndex, uint8_t level);

    /**
     * @brief Recalibrate a specific sensor
     * @param sensorIndex Sensor index (0-24, corresponding to A-Y)
     * @return true if write succeeded
     */
    bool recalibrate(uint8_t sensorIndex);

    /**
     * @brief Recalibrate all 25 sensors
     */
    void recalibrateAll();

    /**
     * @brief Set callback for touch events
     * @param callback Function pointer to call when touch is detected (receives letter A-Y)
     */
    void setTouchCallback(void (*callback)(char));

    // ========================================================================
    // Static Utility Methods
    // ========================================================================

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

    /**
     * @brief Try to recover a stuck I2C bus
     * Toggles SCL to release any slaves holding SDA low
     */
    void recoverI2CBus();

private:
    // Current operating mode
    TouchMode m_mode;

    // Expected sensor index when in EXPECT mode
    uint8_t m_expectedSensorIndex;

    // Track which sensors were successfully initialized
    bool m_sensorActive[NUM_TOUCH_SENSORS];

    // Timestamp of last sensor scan
    uint32_t m_lastScanTime;

    // Callback for touch events
    void (*m_touchCallback)(char);

    /**
     * @brief Initialize a single CAP1188 sensor
     * @param address I2C address of the sensor
     * @return true if initialization succeeded
     */
    bool initSensor(uint8_t address);

    /**
     * @brief Read a register from a CAP1188 sensor
     * @param address I2C address of the sensor
     * @param reg Register address
     * @param value Output value
     * @return true if read succeeded
     */
    bool readRegister(uint8_t address, uint8_t reg, uint8_t& value);

    /**
     * @brief Write a register to a CAP1188 sensor
     * @param address I2C address of the sensor
     * @param reg Register address
     * @param value Value to write
     * @return true if write succeeded
     */
    bool writeRegister(uint8_t address, uint8_t reg, uint8_t value);

    /**
     * @brief Check if CS1 is touched on a sensor
     * Also clears the interrupt flag after reading.
     * @param address I2C address of the sensor
     * @return true if CS1 is touched
     */
    bool isTouched(uint8_t address);

    /**
     * @brief Send TOUCHED response
     * @param letter Sensor letter (A-Y)
     */
    void sendTouched(char letter);

    /**
     * @brief Send RECORDED response
     * @param letter Sensor letter (A-Y)
     */
    void sendRecorded(char letter);
};

#endif // TOUCH_CONTROLLER_H
