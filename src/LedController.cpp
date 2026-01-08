/**
 * @file LedController.cpp
 * @brief Implementation of the LED Controller for dual addressable LED strips
 * 
 * Uses Adafruit NeoPixel library for better Arduino UNO R4 WiFi compatibility.
 */

#include "LedController.h"

// ============================================================================
// LED Position Mappings
// ============================================================================
// 
// Define the mapping from logical positions (A-Y) to physical LEDs.
// Each entry specifies: { strip (STRIP1 or STRIP2), led_index }
//
// Modify these values to match your actual LED layout!
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
    }
    
    m_needsUpdate = false;
}

void LedController::update(uint32_t nowMillis) {
    // Update all animating positions
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (m_positions[i].state == PositionState::ANIMATING) {
            updateAnimation(i, nowMillis);
        }
    }
    
    // Push updates to LEDs if needed
    if (m_needsUpdate) {
        m_strip1.show();
        m_strip2.show();
        m_needsUpdate = false;
    }
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
    
    m_needsUpdate = true;
    
    return true;
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
        case StripId::STRIP1:
            return NUM_LEDS_STRIP1;
        case StripId::STRIP2:
            return NUM_LEDS_STRIP2;
        default:
            return 0;
    }
}

Adafruit_NeoPixel* LedController::getStrip(StripId strip) {
    switch (strip) {
        case StripId::STRIP1:
            return &m_strip1;
        case StripId::STRIP2:
            return &m_strip2;
        default:
            return nullptr;
    }
}

void LedController::setLed(StripId strip, int16_t index, uint8_t r, uint8_t g, uint8_t b) {
    // Bounds check
    if (index < 0) {
        return;
    }
    
    Adafruit_NeoPixel* stripPtr = getStrip(strip);
    if (!stripPtr) {
        return;
    }
    
    uint16_t stripLen = getStripLength(strip);
    if (index < (int16_t)stripLen) {
        stripPtr->setPixelColor(index, stripPtr->Color(r, g, b));
    }
}

void LedController::clearExpandedRegion(uint8_t position, const LedMapping* mapping) {
    if (!mapping) {
        return;
    }
    
    uint16_t stripLen = getStripLength(mapping->strip);
    int16_t center = mapping->index;
    
    // Clear center LED and expanded region (up to SUCCESS_EXPANSION_RADIUS on each side)
    for (int16_t offset = -SUCCESS_EXPANSION_RADIUS; offset <= SUCCESS_EXPANSION_RADIUS; offset++) {
        int16_t idx = center + offset;
        if (idx >= 0 && idx < (int16_t)stripLen) {
            setLed(mapping->strip, idx, COLOR_OFF_R, COLOR_OFF_G, COLOR_OFF_B);
        }
    }
}

void LedController::renderPosition(uint8_t position) {
    if (position >= NUM_POSITIONS) {
        return;
    }
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) {
        return;
    }
    
    PositionData& data = m_positions[position];
    uint16_t stripLen = getStripLength(mapping->strip);
    int16_t center = mapping->index;
    
    switch (data.state) {
        case PositionState::OFF:
            // Already cleared
            break;
            
        case PositionState::SHOWN:
            setLed(mapping->strip, center, COLOR_SHOW_R, COLOR_SHOW_G, COLOR_SHOW_B);
            break;
            
        case PositionState::ANIMATING:
        case PositionState::EXPANDED: {
            // Render center and expanded LEDs up to current step
            uint8_t radius = data.animationStep;
            
            // Center LED (Green)
            setLed(mapping->strip, center, COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
            
            // Expanded LEDs (symmetric on both sides)
            for (uint8_t r = 1; r <= radius; r++) {
                // Left side
                int16_t leftIdx = center - r;
                if (leftIdx >= 0) {
                    setLed(mapping->strip, leftIdx, COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
                }
                
                // Right side
                int16_t rightIdx = center + r;
                if (rightIdx < (int16_t)stripLen) {
                    setLed(mapping->strip, rightIdx, COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
                }
            }
            break;
        }
    }
}

void LedController::updateAnimation(uint8_t position, uint32_t nowMillis) {
    PositionData& data = m_positions[position];
    
    // Check if enough time has passed for next step
    if (nowMillis - data.lastAnimationTime < ANIMATION_STEP_MS) {
        return;
    }
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) {
        return;
    }
    
    // Advance animation step
    data.animationStep++;
    data.lastAnimationTime = nowMillis;
    
    // Check if animation is complete
    if (data.animationStep >= SUCCESS_EXPANSION_RADIUS) {
        data.animationStep = SUCCESS_EXPANSION_RADIUS;
        data.state = PositionState::EXPANDED;
    }
    
    // Render the current state
    renderPosition(position);
    m_needsUpdate = true;
}
