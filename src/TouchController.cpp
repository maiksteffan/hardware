/**
 * @file TouchController.cpp
 * @brief Implementation of event-driven touch sensor controller
 * 
 * Protocol v2: Always polls sensors, emits TOUCH_DOWN/TOUCH_UP events
 * with debouncing for reliable detection.
 */

#include "TouchController.h"
#include "EventQueue.h"

// ============================================================================
// Constructor
// ============================================================================

TouchController::TouchController()
    : m_eventQueue(nullptr)
    , m_lastPollTime(0)
    , m_activeSensorCount(0)
{
    // Initialize all sensor states
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        m_sensors[i].active = false;
        m_sensors[i].currentTouched = false;
        m_sensors[i].debouncedTouched = false;
        m_sensors[i].lastReportedTouched = false;
        m_sensors[i].lastChangeTime = 0;
        
        // Initialize expectation states
        m_expectDown[i].active = false;
        m_expectDown[i].commandId = NO_COMMAND_ID;
        m_expectUp[i].active = false;
        m_expectUp[i].commandId = NO_COMMAND_ID;
    }
}

// ============================================================================
// Public Methods
// ============================================================================

void TouchController::setEventQueue(EventQueue* eventQueue) {
    m_eventQueue = eventQueue;
}

bool TouchController::begin() {
    // Initialize I2C
    Wire.begin();
    Wire.setClock(I2C_CLOCK_SPEED);
    
    // Small delay after I2C init
    delay(100);
    
    // Try to recover I2C bus if stuck
    recoverI2CBus();
    
    m_activeSensorCount = 0;
    
    // Initialize each sensor
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        uint8_t address = SENSOR_I2C_ADDRESSES[i];
        
        if (initSensor(address)) {
            m_sensors[i].active = true;
            m_activeSensorCount++;
        } else {
            m_sensors[i].active = false;
        }
        
        // Reset state
        m_sensors[i].currentTouched = false;
        m_sensors[i].debouncedTouched = false;
        m_sensors[i].lastReportedTouched = false;
        m_sensors[i].lastChangeTime = 0;
    }
    
    return m_activeSensorCount > 0;
}

void TouchController::tick() {
    uint32_t now = millis();
    
    // Rate limit sensor polling
    if (now - m_lastPollTime < TOUCH_POLL_INTERVAL_MS) {
        return;
    }
    m_lastPollTime = now;
    
    // Poll all sensors
    pollSensors();
    
    // Process debouncing and emit events
    processDebounce();
}

bool TouchController::recalibrate(uint8_t sensorIndex) {
    if (sensorIndex >= NUM_TOUCH_SENSORS) {
        return false;
    }
    
    if (!m_sensors[sensorIndex].active) {
        return false;
    }
    
    uint8_t address = SENSOR_I2C_ADDRESSES[sensorIndex];
    
    // Write to trigger recalibration of CS1
    return writeRegister(address, CAP1188_REG_CALIBRATION_ACTIVE, CS1_BIT_MASK);
}

void TouchController::recalibrateAll() {
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        if (m_sensors[i].active) {
            recalibrate(i);
        }
    }
}

void TouchController::setExpectDown(uint8_t sensorIndex, uint32_t commandId) {
    if (sensorIndex >= NUM_TOUCH_SENSORS) {
        return;
    }
    m_expectDown[sensorIndex].active = true;
    m_expectDown[sensorIndex].commandId = commandId;
}

void TouchController::setExpectUp(uint8_t sensorIndex, uint32_t commandId) {
    if (sensorIndex >= NUM_TOUCH_SENSORS) {
        return;
    }
    m_expectUp[sensorIndex].active = true;
    m_expectUp[sensorIndex].commandId = commandId;
}

void TouchController::clearExpectDown(uint8_t sensorIndex) {
    if (sensorIndex >= NUM_TOUCH_SENSORS) {
        return;
    }
    m_expectDown[sensorIndex].active = false;
    m_expectDown[sensorIndex].commandId = NO_COMMAND_ID;
}

void TouchController::clearExpectUp(uint8_t sensorIndex) {
    if (sensorIndex >= NUM_TOUCH_SENSORS) {
        return;
    }
    m_expectUp[sensorIndex].active = false;
    m_expectUp[sensorIndex].commandId = NO_COMMAND_ID;
}

void TouchController::buildActiveSensorList(char* buffer, size_t bufferSize) const {
    if (bufferSize == 0) {
        return;
    }
    
    buffer[0] = '\0';
    size_t pos = 0;
    bool first = true;
    
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        if (m_sensors[i].active) {
            // Need room for comma (if not first) + letter + null terminator
            size_t needed = first ? 2 : 3;
            if (pos + needed > bufferSize) {
                break;  // Buffer full
            }
            
            if (!first) {
                buffer[pos++] = ',';
            }
            buffer[pos++] = indexToLetter(i);
            buffer[pos] = '\0';
            first = false;
        }
    }
}

bool TouchController::isSensorActive(uint8_t sensorIndex) const {
    if (sensorIndex >= NUM_TOUCH_SENSORS) {
        return false;
    }
    return m_sensors[sensorIndex].active;
}

bool TouchController::isTouched(uint8_t sensorIndex) const {
    if (sensorIndex >= NUM_TOUCH_SENSORS) {
        return false;
    }
    return m_sensors[sensorIndex].debouncedTouched;
}

uint8_t TouchController::getActiveSensorCount() const {
    return m_activeSensorCount;
}

// ============================================================================
// Static Utility Methods
// ============================================================================

uint8_t TouchController::letterToIndex(char letter) {
    // Convert to uppercase
    if (letter >= 'a' && letter <= 'y') {
        letter -= 32;
    }
    
    if (letter >= 'A' && letter <= 'Y') {
        return letter - 'A';
    }
    
    return 255;  // Invalid
}

char TouchController::indexToLetter(uint8_t index) {
    if (index < NUM_TOUCH_SENSORS) {
        return 'A' + index;
    }
    return '?';
}

uint8_t TouchController::addressToIndex(uint8_t address) {
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        if (SENSOR_I2C_ADDRESSES[i] == address) {
            return i;
        }
    }
    return 255;  // Not found
}

// ============================================================================
// Private Methods
// ============================================================================

bool TouchController::initSensor(uint8_t address) {
    // Check if sensor responds
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    
    // Enable only CS1 input (bit 0)
    if (!writeRegister(address, CAP1188_REG_SENSOR_INPUT_ENABLE, CS1_BIT_MASK)) {
        return false;
    }
    
    // Set default sensitivity
    // Sensitivity register 0x1F: bits 6:4 = sensitivity (0-7)
    uint8_t sensitivityValue = 0x20 | (DEFAULT_SENSITIVITY << 4);
    if (!writeRegister(address, CAP1188_REG_SENSITIVITY_CONTROL, sensitivityValue)) {
        return false;
    }
    
    // Clear any pending interrupts
    uint8_t dummy;
    readRegister(address, CAP1188_REG_SENSOR_INPUT_STATUS, dummy);
    
    // Clear interrupt flag in main control register
    uint8_t mainControl;
    if (readRegister(address, CAP1188_REG_MAIN_CONTROL, mainControl)) {
        mainControl &= ~0x01;  // Clear INT bit
        writeRegister(address, CAP1188_REG_MAIN_CONTROL, mainControl);
    }
    
    return true;
}

bool TouchController::readRegister(uint8_t address, uint8_t reg, uint8_t& value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        delayMicroseconds(50);
        return false;
    }
    
    delayMicroseconds(50);
    
    if (Wire.requestFrom(address, (uint8_t)1) != 1) {
        return false;
    }
    
    value = Wire.read();
    return true;
}

bool TouchController::writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    uint8_t result = Wire.endTransmission();
    delayMicroseconds(50);
    return result == 0;
}

bool TouchController::readRawTouch(uint8_t address) {
    uint8_t status;
    
    // Read sensor input status register
    if (!readRegister(address, CAP1188_REG_SENSOR_INPUT_STATUS, status)) {
        return false;
    }
    
    // Check if CS1 is touched (bit 0)
    bool touched = (status & CS1_BIT_MASK) != 0;
    
    if (touched) {
        // Clear the interrupt flag
        uint8_t mainControl;
        if (readRegister(address, CAP1188_REG_MAIN_CONTROL, mainControl)) {
            mainControl &= ~0x01;
            writeRegister(address, CAP1188_REG_MAIN_CONTROL, mainControl);
        }
    }
    
    return touched;
}

void TouchController::recoverI2CBus() {
    Wire.end();
    
    // On Arduino UNO R4 WiFi, SDA = A4 (pin 18), SCL = A5 (pin 19)
    pinMode(A5, OUTPUT);
    pinMode(A4, INPUT_PULLUP);
    
    // Toggle SCL to release stuck slaves
    for (int i = 0; i < 9; i++) {
        digitalWrite(A5, LOW);
        delayMicroseconds(5);
        digitalWrite(A5, HIGH);
        delayMicroseconds(5);
    }
    
    // Generate STOP condition
    pinMode(A4, OUTPUT);
    digitalWrite(A4, LOW);
    delayMicroseconds(5);
    digitalWrite(A5, HIGH);
    delayMicroseconds(5);
    digitalWrite(A4, HIGH);
    delayMicroseconds(5);
    
    // Reinitialize I2C
    Wire.begin();
    Wire.setClock(I2C_CLOCK_SPEED);
    delay(10);
}

void TouchController::pollSensors() {
    uint32_t now = millis();
    
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        if (!m_sensors[i].active) {
            continue;
        }
        
        uint8_t address = SENSOR_I2C_ADDRESSES[i];
        bool touched = readRawTouch(address);
        
        // Check if raw state changed
        if (touched != m_sensors[i].currentTouched) {
            m_sensors[i].currentTouched = touched;
            m_sensors[i].lastChangeTime = now;
        }
    }
}

void TouchController::processDebounce() {
    uint32_t now = millis();
    
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        if (!m_sensors[i].active) {
            continue;
        }
        
        TouchSensorState& sensor = m_sensors[i];
        
        // Check if state has been stable long enough
        uint32_t elapsed = now - sensor.lastChangeTime;
        
        if (elapsed >= DEBOUNCE_MS) {
            // State is stable - update debounced state
            if (sensor.currentTouched != sensor.debouncedTouched) {
                sensor.debouncedTouched = sensor.currentTouched;
                
                // Emit event if state changed from last reported
                if (sensor.debouncedTouched != sensor.lastReportedTouched) {
                    sensor.lastReportedTouched = sensor.debouncedTouched;
                    
                    if (m_eventQueue) {
                        char letter = indexToLetter(i);
                        
                        if (sensor.debouncedTouched) {
                            // Touch down detected
                            // Check if we have an expectation for this
                            if (m_expectDown[i].active) {
                                // Expected touch - emit TOUCHED_DOWN with command ID
                                m_eventQueue->queueTouchedDown(letter, m_expectDown[i].commandId);
                                // Clear the expectation (one-shot)
                                m_expectDown[i].active = false;
                                m_expectDown[i].commandId = NO_COMMAND_ID;
                            } else {
                                // Spontaneous touch - emit TOUCH_DOWN
                                m_eventQueue->queueTouchDown(letter);
                            }
                        } else {
                            // Touch up detected
                            // Check if we have an expectation for this
                            if (m_expectUp[i].active) {
                                // Expected release - emit TOUCHED_UP with command ID
                                m_eventQueue->queueTouchedUp(letter, m_expectUp[i].commandId);
                                // Clear the expectation (one-shot)
                                m_expectUp[i].active = false;
                                m_expectUp[i].commandId = NO_COMMAND_ID;
                            } else {
                                // Spontaneous release - emit TOUCH_UP
                                m_eventQueue->queueTouchUp(letter);
                            }
                        }
                    }
                }
            }
        }
    }
}
