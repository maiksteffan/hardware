/**
 * @file TouchController.cpp
 * @brief Implementation of touch sensor controller for 25 CAP1188 sensors
 */

#include "TouchController.h"

// ============================================================================
// Constructor
// ============================================================================

TouchController::TouchController()
    : m_mode(TouchMode::IDLE)
    , m_expectedSensorIndex(255)
    , m_lastScanTime(0)
{
    // Initialize all sensors as inactive
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        m_sensorActive[i] = false;
    }
}

// ============================================================================
// Public Methods
// ============================================================================

bool TouchController::begin() {
    // Initialize I2C (Wire library)
    Wire.begin();
    
    uint8_t successCount = 0;
    
    Serial.println("TouchController: Initializing sensors...");
    
    // Initialize each sensor
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        uint8_t address = SENSOR_I2C_ADDRESSES[i];
        
        if (initSensor(address)) {
            m_sensorActive[i] = true;
            successCount++;
            Serial.print("  Sensor ");
            Serial.print(indexToLetter(i));
            Serial.print(" (0x");
            Serial.print(address, HEX);
            Serial.println("): OK");
        } else {
            m_sensorActive[i] = false;
            Serial.print("  Sensor ");
            Serial.print(indexToLetter(i));
            Serial.print(" (0x");
            Serial.print(address, HEX);
            Serial.println("): FAILED");
        }
    }
    
    Serial.print("TouchController: ");
    Serial.print(successCount);
    Serial.print("/");
    Serial.print(NUM_TOUCH_SENSORS);
    Serial.println(" sensors initialized");
    
    return successCount > 0;
}

void TouchController::update() {
    // Rate limit sensor scanning to prevent excessive I2C traffic
    uint32_t now = millis();
    if (now - m_lastScanTime < TOUCH_SCAN_INTERVAL_MS) {
        return;
    }
    m_lastScanTime = now;
    
    // If idle, no need to scan (optimization)
    if (m_mode == TouchMode::IDLE) {
        return;
    }
    
    // Scan all active sensors for touch
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        // Skip inactive sensors
        if (!m_sensorActive[i]) {
            continue;
        }
        
        uint8_t address = SENSOR_I2C_ADDRESSES[i];
        
        // Check if CS1 is touched
        if (isTouched(address)) {
            char letter = indexToLetter(i);
            
            if (m_mode == TouchMode::EXPECT) {
                // In EXPECT mode, only respond if this is the expected sensor
                if (i == m_expectedSensorIndex) {
                    sendTouched(letter);
                    m_mode = TouchMode::IDLE;
                    m_expectedSensorIndex = 255;
                }
                // Ignore touches on other sensors in EXPECT mode
            } 
            else if (m_mode == TouchMode::RECORD) {
                // In RECORD mode, respond to first touch on any sensor
                sendRecorded(letter);
                m_mode = TouchMode::IDLE;
                return;  // Only record first touch
            }
        }
    }
}

bool TouchController::expectSensor(char letter) {
    uint8_t index = letterToIndex(letter);
    
    if (index >= NUM_TOUCH_SENSORS) {
        return false;  // Invalid letter
    }
    
    if (!m_sensorActive[index]) {
        Serial.print("WARN: Sensor ");
        Serial.print((char)toupper(letter));
        Serial.println(" is not active");
        // Still allow expect mode even if sensor failed init (might work later)
    }
    
    m_expectedSensorIndex = index;
    m_mode = TouchMode::EXPECT;
    
    return true;
}

void TouchController::startRecording() {
    m_mode = TouchMode::RECORD;
    m_expectedSensorIndex = 255;  // Clear any previous expectation
}

void TouchController::cancelOperation() {
    m_mode = TouchMode::IDLE;
    m_expectedSensorIndex = 255;
}

bool TouchController::isIdle() const {
    return m_mode == TouchMode::IDLE;
}

void TouchController::scanAddresses() {
    Serial.println("I2C Address Scan:");
    Serial.println("-----------------");
    
    uint8_t foundCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("  0x");
            if (address < 16) {
                Serial.print("0");
            }
            Serial.print(address, HEX);
            
            // Check if this is one of our sensor addresses
            uint8_t sensorIndex = addressToIndex(address);
            if (sensorIndex < NUM_TOUCH_SENSORS) {
                Serial.print(" <- Sensor ");
                Serial.print(indexToLetter(sensorIndex));
            }
            Serial.println();
            foundCount++;
        }
    }
    
    Serial.print("Found ");
    Serial.print(foundCount);
    Serial.println(" I2C devices");
}

bool TouchController::setSensitivity(uint8_t sensorIndex, uint8_t level) {
    if (sensorIndex >= NUM_TOUCH_SENSORS) {
        return false;
    }
    
    // Clamp level to valid range (0-7)
    if (level > 7) {
        level = 7;
    }
    
    uint8_t address = SENSOR_I2C_ADDRESSES[sensorIndex];
    
    // Sensitivity is stored in bits 6:4 of register 0x1F
    // The remaining bits control delta sense, keep default (0x2F base value)
    uint8_t regValue = 0x20 | (level << 4);
    
    return writeRegister(address, CAP1188_REG_SENSITIVITY_CONTROL, regValue);
}

bool TouchController::recalibrate(uint8_t sensorIndex) {
    if (sensorIndex >= NUM_TOUCH_SENSORS) {
        return false;
    }
    
    uint8_t address = SENSOR_I2C_ADDRESSES[sensorIndex];
    
    // Write 0x01 to trigger recalibration of CS1
    return writeRegister(address, CAP1188_REG_CALIBRATION_ACTIVE, CS1_BIT_MASK);
}

void TouchController::recalibrateAll() {
    Serial.println("Recalibrating all sensors...");
    
    for (uint8_t i = 0; i < NUM_TOUCH_SENSORS; i++) {
        if (m_sensorActive[i]) {
            if (recalibrate(i)) {
                Serial.print("  Sensor ");
                Serial.print(indexToLetter(i));
                Serial.println(": OK");
            } else {
                Serial.print("  Sensor ");
                Serial.print(indexToLetter(i));
                Serial.println(": FAILED");
            }
        }
    }
    
    Serial.println("Recalibration complete");
}

// ============================================================================
// Static Utility Methods
// ============================================================================

uint8_t TouchController::letterToIndex(char letter) {
    // Convert to uppercase
    if (letter >= 'a' && letter <= 'y') {
        letter -= 32;  // Convert to uppercase
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
        return false;  // Sensor not responding
    }
    
    // Enable only CS1 input (bit 0 = CS1)
    if (!writeRegister(address, CAP1188_REG_SENSOR_INPUT_ENABLE, CS1_BIT_MASK)) {
        return false;
    }
    
    // Set default sensitivity
    // Sensitivity register 0x1F: bits 6:4 = sensitivity (0-7)
    // Lower value = more sensitive, default is 0x2F (sensitivity = 2)
    uint8_t sensitivityValue = 0x20 | (DEFAULT_SENSITIVITY << 4);
    if (!writeRegister(address, CAP1188_REG_SENSITIVITY_CONTROL, sensitivityValue)) {
        return false;
    }
    
    // Clear any pending interrupts by reading the status register
    uint8_t dummy;
    readRegister(address, CAP1188_REG_SENSOR_INPUT_STATUS, dummy);
    
    // Clear the interrupt flag in main control register
    // Read current value, clear INT bit (bit 0), write back
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
        return false;
    }
    
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
    return Wire.endTransmission() == 0;
}

bool TouchController::isTouched(uint8_t address) {
    uint8_t status;
    
    // Read sensor input status register
    if (!readRegister(address, CAP1188_REG_SENSOR_INPUT_STATUS, status)) {
        return false;
    }
    
    // Check if CS1 is touched (bit 0)
    bool touched = (status & CS1_BIT_MASK) != 0;
    
    if (touched) {
        // Clear the interrupt flag by writing 0 to bit 0 of main control register
        // This is required to reset the touch detection
        uint8_t mainControl;
        if (readRegister(address, CAP1188_REG_MAIN_CONTROL, mainControl)) {
            mainControl &= ~0x01;  // Clear INT bit
            writeRegister(address, CAP1188_REG_MAIN_CONTROL, mainControl);
        }
    }
    
    return touched;
}

void TouchController::sendTouched(char letter) {
    Serial.print("TOUCHED ");
    Serial.println(letter);
}

void TouchController::sendRecorded(char letter) {
    Serial.print("RECORDED ");
    Serial.println(letter);
}
