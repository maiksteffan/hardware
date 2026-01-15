/**
 * @file SequenceController.cpp
 * @brief Implementation of sequence controller for LED/touch interactions
 */

#include "SequenceController.h"
#include "LedController.h"
#include "TouchController.h"

// ============================================================================
// Constructor
// ============================================================================

SequenceController::SequenceController(LedController& ledController, TouchController& touchController)
    : m_ledController(ledController)
    , m_touchController(touchController)
    , m_sequenceLength(0)
    , m_currentIndex(0)
    , m_state(SequenceState::IDLE)
    , m_stateStartTime(0)
{
    memset(m_sequence, 0, sizeof(m_sequence));
}

// ============================================================================
// Public Methods
// ============================================================================

void SequenceController::begin() {
    m_state = SequenceState::IDLE;
    m_sequenceLength = 0;
    m_currentIndex = 0;
}

void SequenceController::update() {
    // Nothing to do if idle
    if (m_state == SequenceState::IDLE) {
        return;
    }

    // State machine is mostly event-driven via onTouched()
    // This update handles any time-based transitions if needed
}

bool SequenceController::startSequence(const char* sequence) {
    // Stop any existing sequence
    stop();

    // Parse the sequence
    if (!parseSequence(sequence)) {
        Serial.println("ERR invalid_sequence");
        return false;
    }

    if (m_sequenceLength == 0) {
        Serial.println("ERR empty_sequence");
        return false;
    }

    // Print sequence info
    Serial.print("SEQUENCE STARTED: ");
    for (uint8_t i = 0; i < m_sequenceLength; i++) {
        if (i > 0) Serial.print(",");
        Serial.print(m_sequence[i]);
    }
    Serial.println();

    // Start at first position
    m_currentIndex = 0;
    showCurrentAndExpect();

    return true;
}

void SequenceController::stop() {
    if (m_state != SequenceState::IDLE) {
        // Cancel any pending touch expectation
        m_touchController.cancelOperation();
        
        Serial.println("SEQUENCE STOPPED");
    }
    
    m_state = SequenceState::IDLE;
    m_sequenceLength = 0;
    m_currentIndex = 0;
}

bool SequenceController::isRunning() const {
    return m_state != SequenceState::IDLE;
}

void SequenceController::onTouched(char letter) {
    // Only process if we're expecting a touch
    if (m_state != SequenceState::EXPECTING) {
        return;
    }

    // Check if it's the correct letter
    char expected = m_sequence[m_currentIndex];
    
    // Convert both to uppercase for comparison
    if (letter >= 'a' && letter <= 'z') letter -= 32;
    if (expected >= 'a' && expected <= 'z') expected -= 32;

    if (letter == expected) {
        handleSuccess();
    }
    // Wrong letter is ignored (TouchController in EXPECT mode handles this)
}

// ============================================================================
// Private Methods
// ============================================================================

bool SequenceController::parseSequence(const char* sequence) {
    m_sequenceLength = 0;
    memset(m_sequence, 0, sizeof(m_sequence));

    if (sequence == nullptr || *sequence == '\0') {
        return false;
    }

    const char* ptr = sequence;
    
    while (*ptr != '\0' && m_sequenceLength < MAX_SEQUENCE_LENGTH) {
        // Skip whitespace and commas
        while (*ptr == ' ' || *ptr == ',' || *ptr == '\t') {
            ptr++;
        }
        
        if (*ptr == '\0') {
            break;
        }

        // Get the letter
        char c = *ptr;
        
        // Convert to uppercase
        if (c >= 'a' && c <= 'z') {
            c -= 32;
        }

        // Validate it's A-Y
        if (c < 'A' || c > 'Y') {
            return false;  // Invalid character
        }

        m_sequence[m_sequenceLength++] = c;
        ptr++;
    }

    return m_sequenceLength > 0;
}

void SequenceController::showCurrentAndExpect() {
    char letter = m_sequence[m_currentIndex];
    uint8_t position = LedController::charToPosition(letter);

    // Show the LED
    Serial.print("SHOW ");
    Serial.println(letter);
    m_ledController.show(position);

    // Set up touch expectation
    Serial.print("EXPECT ");
    Serial.println(letter);
    m_touchController.expectSensor(letter);

    m_state = SequenceState::EXPECTING;
    m_stateStartTime = millis();
}

void SequenceController::handleSuccess() {
    char letter = m_sequence[m_currentIndex];
    uint8_t position = LedController::charToPosition(letter);

    // Play success animation
    Serial.print("SUCCESS ");
    Serial.println(letter);
    m_ledController.success(position);

    // Hide LED from 2 positions back (if exists)
    if (m_currentIndex >= 2) {
        char hideChar = m_sequence[m_currentIndex - 2];
        uint8_t hidePos = LedController::charToPosition(hideChar);
        
        Serial.print("HIDE ");
        Serial.println(hideChar);
        m_ledController.hide(hidePos);
    }

    // Move to next
    advanceSequence();
}

void SequenceController::advanceSequence() {
    m_currentIndex++;

    if (m_currentIndex >= m_sequenceLength) {
        completeSequence();
    } else {
        showCurrentAndExpect();
    }
}

void SequenceController::completeSequence() {
    // Hide remaining LEDs (last 2 if they exist)
    for (int8_t i = m_sequenceLength - 1; i >= 0 && i >= (int8_t)(m_sequenceLength - 2); i--) {
        char hideChar = m_sequence[i];
        uint8_t hidePos = LedController::charToPosition(hideChar);
        
        Serial.print("HIDE ");
        Serial.println(hideChar);
        m_ledController.hide(hidePos);
    }

    Serial.println("SEQUENCE COMPLETED!!");
    
    m_state = SequenceState::IDLE;
    m_sequenceLength = 0;
    m_currentIndex = 0;
}
