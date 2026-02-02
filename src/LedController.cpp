/**
 * @file LedController.cpp
 * @brief Implementation of the LED Controller for dual addressable LED strips
 * 
 * Protocol v2: Non-blocking animations with completion tracking.
 */

#include "LedController.h"

// ============================================================================
// LED Position Mappings
// ============================================================================

static const LedMapping LED_MAPPINGS[NUM_POSITIONS] = {
    // Position A (index 0)
    { StripId::STRIP1, 153 },
    // Position B (index 1)
    { StripId::STRIP1, 165 },
    // Position C (index 2)
    { StripId::STRIP1, 177 },
    // Position D (index 3)
    { StripId::STRIP2, 177 },
    // Position E (index 4)
    { StripId::STRIP2, 165 },
    // Position F (index 5)
    { StripId::STRIP2, 153 },
    // Position G (index 6)
    { StripId::STRIP1, 130 },
    // Position H (index 7)
    { StripId::STRIP1, 118 },
    // Position I (index 8)
    { StripId::STRIP1, 105 },
    // Position J (index 9)
    { StripId::STRIP1, 92 },
    // Position K (index 10)
    { StripId::STRIP2, 105 },
    // Position L (index 11)
    { StripId::STRIP2, 118 },
    // Position M (index 12)
    { StripId::STRIP2, 130 },
    // Position N (index 13)
    { StripId::STRIP1, 55 },
    // Position O (index 14)
    { StripId::STRIP1, 67 },
    // Position P (index 15)
    { StripId::STRIP1, 79 },
    // Position Q (index 16)
    { StripId::STRIP2, 79 },
    // Position R (index 17)
    { StripId::STRIP2, 67 },
    // Position S (index 18)
    { StripId::STRIP2, 55 },
    // Position T (index 19)
    { StripId::STRIP1, 34 },
    // Position U (index 20)
    { StripId::STRIP1, 22 },
    // Position V (index 21)
    { StripId::STRIP1, 10 },
    // Position W (index 22)
    { StripId::STRIP2, 10 },
    // Position X (index 23)
    { StripId::STRIP2, 22 },
    // Position Y (index 24)
    { StripId::STRIP2, 34 }
};

// ============================================================================
// Constructor
// ============================================================================

LedController::LedController()
    : m_strip1(NUM_LEDS_STRIP1, STRIP1_PIN, NEO_GRB + NEO_KHZ800)
    , m_strip2(NUM_LEDS_STRIP2, STRIP2_PIN, NEO_GRB + NEO_KHZ800)
    , m_sequenceAnimActive(false)
    , m_sequenceAnimStep(0)
    , m_sequenceAnimLastTime(0)
    , m_needsUpdate(false)
{
}

// ============================================================================
// Public Methods
// ============================================================================

void LedController::begin() {
    // Initialize NeoPixel strips
    m_strip1.begin();
    m_strip2.begin();
    
    // Set global brightness
    m_strip1.setBrightness(LED_BRIGHTNESS);
    m_strip2.setBrightness(LED_BRIGHTNESS);
    
    // Clear all LEDs
    m_strip1.clear();
    m_strip2.clear();
    m_strip1.show();
    m_strip2.show();
    
    // Initialize position states
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        m_positions[i].state = PositionState::OFF;
        m_positions[i].animationStep = 0;
        m_positions[i].lastAnimationTime = 0;
        m_positions[i].blinkOn = false;
    }
    
    // Initialize SEQUENCE_COMPLETED animation state
    m_sequenceAnimActive = false;
    m_sequenceAnimStep = 0;
    m_sequenceAnimLastTime = 0;
    
    // Initialize SEQUENCE_FAIL animation state
    m_sequenceFailAnimActive = false;
    m_sequenceFailAnimStep = 0;
    m_sequenceFailAnimLastTime = 0;
    
    m_needsUpdate = false;
}

void LedController::update(uint32_t nowMillis) {
    // Update all animating positions
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (m_positions[i].state == PositionState::ANIMATING) {
            updateAnimation(i, nowMillis);
        }
    }
    
    // Update blinking positions
    updateBlinking(nowMillis);
    
    // Update SEQUENCE_COMPLETED animation if active
    if (m_sequenceAnimActive) {
        updateSequenceCompletedAnimation(nowMillis);
    }
    
    // Update SEQUENCE_FAIL animation if active
    if (m_sequenceFailAnimActive) {
        updateSequenceFailAnimation(nowMillis);
    }
    
    // Push updates to LEDs if needed
    if (m_needsUpdate) {
        m_strip1.show();
        m_strip2.show();
        m_needsUpdate = false;
    }
}

void LedController::tick() {
    update(millis());
}

bool LedController::show(uint8_t position) {
    if (position >= NUM_POSITIONS) {
        return false;
    }
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) {
        return false;
    }
    
    // If currently expanded/animating, clear that region first
    if (m_positions[position].state == PositionState::ANIMATING ||
        m_positions[position].state == PositionState::EXPANDED) {
        clearExpandedRegion(position, mapping);
    }
    
    // Set to SHOWN state
    m_positions[position].state = PositionState::SHOWN;
    m_positions[position].animationStep = 0;
    
    // Light the single LED in SHOW color (Blue)
    setLed(mapping->strip, mapping->index, COLOR_SHOW_R, COLOR_SHOW_G, COLOR_SHOW_B);
    m_needsUpdate = true;
    
    return true;
}

bool LedController::hide(uint8_t position) {
    if (position >= NUM_POSITIONS) {
        return false;
    }
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) {
        return false;
    }
    
    // Clear expanded region (covers both single LED and expanded area)
    clearExpandedRegion(position, mapping);
    
    // Reset state
    m_positions[position].state = PositionState::OFF;
    m_positions[position].animationStep = 0;
    m_positions[position].blinkOn = false;
    
    m_needsUpdate = true;
    
    return true;
}

bool LedController::blink(uint8_t position) {
    if (position >= NUM_POSITIONS) {
        return false;
    }
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) {
        return false;
    }
    
    // If currently expanded/animating, clear that region first
    if (m_positions[position].state == PositionState::ANIMATING ||
        m_positions[position].state == PositionState::EXPANDED) {
        clearExpandedRegion(position, mapping);
    }
    
    // Set to BLINKING state
    m_positions[position].state = PositionState::BLINKING;
    m_positions[position].animationStep = 0;
    m_positions[position].lastAnimationTime = millis();
    m_positions[position].blinkOn = true;  // Start with LED on
    
    // Light the LED in BLINK color (orange) - signals "release me!"
    setLed(mapping->strip, mapping->index, COLOR_BLINK_R, COLOR_BLINK_G, COLOR_BLINK_B);
    m_needsUpdate = true;
    
    return true;
}

bool LedController::stopBlink(uint8_t position) {
    if (position >= NUM_POSITIONS) {
        return false;
    }
    
    // Only act if actually blinking
    if (m_positions[position].state != PositionState::BLINKING) {
        return true; // Not an error, just no-op
    }
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) {
        return false;
    }
    
    // Turn off the LED
    setLed(mapping->strip, mapping->index, COLOR_OFF_R, COLOR_OFF_G, COLOR_OFF_B);
    
    // Reset state
    m_positions[position].state = PositionState::OFF;
    m_positions[position].animationStep = 0;
    m_positions[position].blinkOn = false;
    
    m_needsUpdate = true;
    
    return true;
}

bool LedController::mistake(uint8_t position) {
    if (position >= NUM_POSITIONS) {
        return false;
    }
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) {
        return false;
    }
    
    // If currently expanded/animating, clear that region first
    if (m_positions[position].state == PositionState::ANIMATING ||
        m_positions[position].state == PositionState::EXPANDED) {
        clearExpandedRegion(position, mapping);
    }
    
    // Set to MISTAKE state
    m_positions[position].state = PositionState::MISTAKE;
    m_positions[position].animationStep = 0;
    m_positions[position].blinkOn = false;
    
    // Light the single LED in MISTAKE color (Red)
    setLed(mapping->strip, mapping->index, COLOR_MISTAKE_R, COLOR_MISTAKE_G, COLOR_MISTAKE_B);
    m_needsUpdate = true;
    
    return true;
}

bool LedController::isBlinking(uint8_t position) const {
    if (position >= NUM_POSITIONS) {
        return false;
    }
    return m_positions[position].state == PositionState::BLINKING;
}

bool LedController::success(uint8_t position) {
    if (position >= NUM_POSITIONS) {
        return false;
    }
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) {
        return false;
    }
    
    // If currently expanded/animating, clear that region first
    if (m_positions[position].state == PositionState::ANIMATING ||
        m_positions[position].state == PositionState::EXPANDED) {
        clearExpandedRegion(position, mapping);
    } else if (m_positions[position].state == PositionState::SHOWN) {
        // Clear just the single LED if it was shown
        setLed(mapping->strip, mapping->index, COLOR_OFF_R, COLOR_OFF_G, COLOR_OFF_B);
    }
    
    // Start animation from center
    m_positions[position].state = PositionState::ANIMATING;
    m_positions[position].animationStep = 0;
    m_positions[position].lastAnimationTime = millis();
    
    // Light the center LED immediately (Green)
    setLed(mapping->strip, mapping->index, COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
    m_needsUpdate = true;
    
    return true;
}

bool LedController::isAnimationComplete(uint8_t position) const {
    if (position >= NUM_POSITIONS) {
        return true;  // Invalid position, treat as complete
    }
    
    // Animation is complete if not animating (OFF, SHOWN, or EXPANDED)
    return m_positions[position].state != PositionState::ANIMATING;
}

bool LedController::hasActiveAnimations() const {
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (m_positions[i].state == PositionState::ANIMATING) {
            return true;
        }
    }
    return false;
}

uint8_t LedController::charToPosition(char c) {
    // Convert to uppercase
    if (c >= 'a' && c <= 'y') {
        c = c - 'a' + 'A';
    }
    
    // Validate range A-Y
    if (c >= 'A' && c <= 'Y') {
        return c - 'A';
    }
    
    return 255;  // Invalid
}

char LedController::positionToChar(uint8_t pos) {
    if (pos < NUM_POSITIONS) {
        return 'A' + pos;
    }
    return '?';
}

// ============================================================================
// Private Methods
// ============================================================================

const LedMapping* LedController::getMapping(uint8_t position) const {
    if (position >= NUM_POSITIONS) {
        return nullptr;
    }
    return &LED_MAPPINGS[position];
}

uint16_t LedController::getStripLength(StripId strip) const {
    switch (strip) {
        case StripId: