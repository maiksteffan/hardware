/**
 * @file LedController.cpp
 * @brief Implementation of the LED Controller for dual addressable LED strips
 */

#include "LedController.h"

// ============================================================================
// LED Position Mappings (A-Y mapped to physical LED indices)
// ============================================================================

static const LedMapping LED_MAPPINGS[NUM_POSITIONS] = {
    { StripId::STRIP1, 153 },  // A
    { StripId::STRIP1, 165 },  // B
    { StripId::STRIP1, 177 },  // C
    { StripId::STRIP2, 177 },  // D
    { StripId::STRIP2, 165 },  // E
    { StripId::STRIP2, 153 },  // F
    { StripId::STRIP1, 130 },  // G
    { StripId::STRIP1, 118 },  // H
    { StripId::STRIP1, 105 },  // I
    { StripId::STRIP1, 92 },   // J
    { StripId::STRIP2, 105 },  // K
    { StripId::STRIP2, 118 },  // L
    { StripId::STRIP2, 130 },  // M
    { StripId::STRIP1, 55 },   // N
    { StripId::STRIP1, 67 },   // O
    { StripId::STRIP1, 79 },   // P
    { StripId::STRIP2, 79 },   // Q
    { StripId::STRIP2, 67 },   // R
    { StripId::STRIP2, 55 },   // S
    { StripId::STRIP1, 34 },   // T
    { StripId::STRIP1, 22 },   // U
    { StripId::STRIP1, 10 },   // V
    { StripId::STRIP2, 10 },   // W
    { StripId::STRIP2, 22 },   // X
    { StripId::STRIP2, 34 }    // Y
};

// Animation constants
static const uint8_t SUCCESS_EXPANSION_RADIUS = 5;
static const uint8_t SEQ_PULSE_COUNT = 2;
static const uint16_t SEQ_PULSE_STEPS = 20;
static const uint16_t SEQ_STEP_DURATION_MS = 10;
static const uint8_t SEQ_PULSE_MAX_BRIGHTNESS = 40;

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
    m_strip1.begin();
    m_strip2.begin();
    m_strip1.setBrightness(LED_BRIGHTNESS);
    m_strip2.setBrightness(LED_BRIGHTNESS);
    m_strip1.clear();
    m_strip2.clear();
    m_strip1.show();
    m_strip2.show();
    
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        m_positions[i].state = PositionState::OFF;
        m_positions[i].animationStep = 0;
        m_positions[i].lastAnimationTime = 0;
        m_positions[i].blinkOn = false;
    }
    
    m_sequenceAnimActive = false;
    m_needsUpdate = false;
}

void LedController::tick() {
    update(millis());
}

void LedController::update(uint32_t nowMillis) {
    // Update animations
    for (uint8_t i = 0; i < NUM_POSITIONS; i++) {
        if (m_positions[i].state == PositionState::ANIMATING) {
            updateAnimation(i, nowMillis);
        } else if (m_positions[i].state == PositionState::CONTRACTING) {
            updateContractAnimation(i, nowMillis);
        }
    }
    
    updateBlinking(nowMillis);
    
    if (m_sequenceAnimActive) {
        updateSequenceCompletedAnimation(nowMillis);
    }
    
    if (m_needsUpdate) {
        m_strip1.show();
        m_strip2.show();
        m_needsUpdate = false;
    }
}

bool LedController::show(uint8_t position) {
    if (position >= NUM_POSITIONS) return false;
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) return false;
    
    if (m_positions[position].state == PositionState::ANIMATING ||
        m_positions[position].state == PositionState::EXPANDED) {
        clearExpandedRegion(position, mapping);
    }
    
    m_positions[position].state = PositionState::SHOWN;
    m_positions[position].animationStep = 0;
    
    setLed(mapping->strip, mapping->index, COLOR_SHOW_R, COLOR_SHOW_G, COLOR_SHOW_B);
    m_needsUpdate = true;
    
    return true;
}

bool LedController::hide(uint8_t position) {
    if (position >= NUM_POSITIONS) return false;
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) return false;
    
    clearExpandedRegion(position, mapping);
    
    m_positions[position].state = PositionState::OFF;
    m_positions[position].animationStep = 0;
    m_positions[position].blinkOn = false;
    
    m_needsUpdate = true;
    return true;
}

bool LedController::success(uint8_t position) {
    if (position >= NUM_POSITIONS) return false;
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) return false;
    
    if (m_positions[position].state == PositionState::ANIMATING ||
        m_positions[position].state == PositionState::EXPANDED) {
        clearExpandedRegion(position, mapping);
    } else if (m_positions[position].state == PositionState::SHOWN) {
        setLed(mapping->strip, mapping->index, COLOR_OFF_R, COLOR_OFF_G, COLOR_OFF_B);
    }
    
    m_positions[position].state = PositionState::ANIMATING;
    m_positions[position].animationStep = 0;
    m_positions[position].lastAnimationTime = millis();
    
    setLed(mapping->strip, mapping->index, COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
    m_needsUpdate = true;
    
    return true;
}

bool LedController::fail(uint8_t position) {
    if (position >= NUM_POSITIONS) return false;
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) return false;
    
    if (m_positions[position].state == PositionState::ANIMATING ||
        m_positions[position].state == PositionState::EXPANDED) {
        clearExpandedRegion(position, mapping);
    }
    
    m_positions[position].state = PositionState::SHOWN;
    m_positions[position].animationStep = 0;
    
    setLed(mapping->strip, mapping->index, COLOR_FAIL_R, COLOR_FAIL_G, COLOR_FAIL_B);
    m_needsUpdate = true;
    
    return true;
}

bool LedController::contract(uint8_t position) {
    if (position >= NUM_POSITIONS) return false;
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) return false;
    
    // Only contract if expanded or animating (expanding)
    if (m_positions[position].state == PositionState::EXPANDED ||
        m_positions[position].state == PositionState::ANIMATING) {
        // Start from full expansion radius
        m_positions[position].state = PositionState::CONTRACTING;
        m_positions[position].animationStep = SUCCESS_EXPANSION_RADIUS;
        m_positions[position].lastAnimationTime = millis();
    } else {
        // If not expanded, just ensure it's shown as a single green LED
        m_positions[position].state = PositionState::SHOWN;
        setLed(mapping->strip, mapping->index, COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
    }
    
    m_needsUpdate = true;
    return true;
}

bool LedController::blink(uint8_t position) {
    if (position >= NUM_POSITIONS) return false;
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) return false;
    
    if (m_positions[position].state == PositionState::ANIMATING ||
        m_positions[position].state == PositionState::EXPANDED) {
        clearExpandedRegion(position, mapping);
    }
    
    m_positions[position].state = PositionState::BLINKING;
    m_positions[position].animationStep = 0;
    m_positions[position].lastAnimationTime = millis();
    m_positions[position].blinkOn = true;
    
    setLed(mapping->strip, mapping->index, COLOR_BLINK_R, COLOR_BLINK_G, COLOR_BLINK_B);
    m_needsUpdate = true;
    
    return true;
}

bool LedController::stopBlink(uint8_t position) {
    if (position >= NUM_POSITIONS) return false;
    
    if (m_positions[position].state != PositionState::BLINKING) {
        return true;
    }
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) return false;
    
    setLed(mapping->strip, mapping->index, COLOR_OFF_R, COLOR_OFF_G, COLOR_OFF_B);
    
    m_positions[position].state = PositionState::OFF;
    m_positions[position].animationStep = 0;
    m_positions[position].blinkOn = false;
    
    m_needsUpdate = true;
    return true;
}

void LedController::startSequenceCompletedAnimation() {
    m_sequenceAnimActive = true;
    m_sequenceAnimStep = 0;
    m_sequenceAnimLastTime = millis();
    
    m_strip1.clear();
    m_strip2.clear();
    m_needsUpdate = true;
}

bool LedController::isSequenceCompletedAnimationComplete() const {
    return !m_sequenceAnimActive;
}

bool LedController::isAnimationComplete(uint8_t position) const {
    if (position >= NUM_POSITIONS) return true;
    return m_positions[position].state != PositionState::ANIMATING;
}

bool LedController::isContractComplete(uint8_t position) const {
    if (position >= NUM_POSITIONS) return true;
    return m_positions[position].state != PositionState::CONTRACTING;
}

bool LedController::isBlinking(uint8_t position) const {
    if (position >= NUM_POSITIONS) return false;
    return m_positions[position].state == PositionState::BLINKING;
}

uint8_t LedController::charToPosition(char c) {
    if (c >= 'a' && c <= 'y') c = c - 'a' + 'A';
    if (c >= 'A' && c <= 'Y') return c - 'A';
    return 255;
}

char LedController::positionToChar(uint8_t pos) {
    if (pos < NUM_POSITIONS) return 'A' + pos;
    return '?';
}

// ============================================================================
// Private Methods
// ============================================================================

const LedMapping* LedController::getMapping(uint8_t position) const {
    if (position >= NUM_POSITIONS) return nullptr;
    return &LED_MAPPINGS[position];
}

Adafruit_NeoPixel* LedController::getStrip(StripId strip) {
    return (strip == StripId::STRIP1) ? &m_strip1 : &m_strip2;
}

uint16_t LedController::getStripLength(StripId strip) const {
    return (strip == StripId::STRIP1) ? NUM_LEDS_STRIP1 : NUM_LEDS_STRIP2;
}

void LedController::setLed(StripId strip, int16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0) return;
    
    Adafruit_NeoPixel* stripPtr = getStrip(strip);
    uint16_t stripLen = getStripLength(strip);
    
    if (index < (int16_t)stripLen) {
        stripPtr->setPixelColor(index, stripPtr->Color(r, g, b));
    }
}

void LedController::clearExpandedRegion(uint8_t position, const LedMapping* mapping) {
    if (!mapping) return;
    
    uint16_t stripLen = getStripLength(mapping->strip);
    int16_t center = mapping->index;
    
    for (int16_t offset = -SUCCESS_EXPANSION_RADIUS; offset <= SUCCESS_EXPANSION_RADIUS; offset++) {
        int16_t idx = center + offset;
        if (idx >= 0 && idx < (int16_t)stripLen) {
            setLed(mapping->strip, idx, COLOR_OFF_R, COLOR_OFF_G, COLOR_OFF_B);
        }
    }
}

void LedController::updateAnimation(uint8_t position, uint32_t nowMillis) {
    PositionData& data = m_positions[position];
    
    if (nowMillis - data.lastAnimationTime < ANIMATION_STEP_MS) return;
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) return;
    
    data.animationStep++;
    data.lastAnimationTime = nowMillis;
    
    if (data.animationStep >= SUCCESS_EXPANSION_RADIUS) {
        data.animationStep = SUCCESS_EXPANSION_RADIUS;
        data.state = PositionState::EXPANDED;
    }
    
    // Render expanded region
    uint16_t stripLen = getStripLength(mapping->strip);
    int16_t center = mapping->index;
    
    setLed(mapping->strip, center, COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
    
    for (uint8_t r = 1; r <= data.animationStep; r++) {
        int16_t leftIdx = center - r;
        if (leftIdx >= 0) {
            setLed(mapping->strip, leftIdx, COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
        }
        int16_t rightIdx = center + r;
        if (rightIdx < (int16_t)stripLen) {
            setLed(mapping->strip, rightIdx, COLOR_SUCCESS_R, COLOR_SUCCESS_G, COLOR_SUCCESS_B);
        }
    }
    
    m_needsUpdate = true;
}

void LedController::updateContractAnimation(uint8_t position, uint32_t nowMillis) {
    PositionData& data = m_positions[position];
    
    if (nowMillis - data.lastAnimationTime < ANIMATION_STEP_MS) return;
    
    const LedMapping* mapping = getMapping(position);
    if (!mapping) return;
    
    data.lastAnimationTime = nowMillis;
    
    uint16_t stripLen = getStripLength(mapping->strip);
    int16_t center = mapping->index;
    
    // T