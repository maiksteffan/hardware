/**
 * @file LedController.h
 * @brief LED Controller for dual addressable LED strips
 * 
 * Manages 25 logical LED positions (A-Y) mapped to two physical LED strips.
 * Supports SHOW, HIDE, SUCCESS, BLINK, STOP_BLINK, and SEQUENCE_COMPLETED.
 */

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

// ============================================================================
// Types
// ============================================================================

enum class StripId : uint8_t { STRIP1 = 0, STRIP2 = 1 };

struct LedMapping {
    StripId strip;
    uint8_t index;
};

enum class PositionState : uint8_t {
    OFF,
    SHOWN,
    ANIMATING,
    EXPANDED,
    CONTRACTING,
    BLINKING
};

struct PositionData {
    PositionState state;
    uint8_t animationStep;
    uint32_t lastAnimationTime;
    bool blinkOn;
    uint8_t expansionRadius;  // Current expansion radius for EXPAND_STEP/CONTRACT_STEP
};

// ============================================================================
// LedController Class
// ============================================================================

class LedController {
public:
    LedController();
    
    void begin();
    void tick();
    
    // LED commands
    bool show(uint8_t position);
    bool hide(uint8_t position);
    void hideAll();
    bool success(uint8_t position);
    bool fail(uint8_t position);
    bool contract(uint8_t position);
    bool blink(uint8_t position);
    bool stopBlink(uint8_t position);
    bool expandStep(uint8_t position);
    bool contractStep(uint8_t position);
    
    // Sequence animation
    void startSequenceCompletedAnimation();
    bool isSequenceCompletedAnimationComplete() const;
    
    // Menu change animation
    void startMenuChangeAnimation(uint8_t r, uint8_t g, uint8_t b, uint8_t range);
    bool isMenuChangeAnimationComplete() const;
    
    // State queries
    bool isAnimationComplete(uint8_t position) const;
    bool isContractComplete(uint8_t position) const;
    bool isBlinking(uint8_t position) const;
    
    // Utilities
    static uint8_t charToPosition(char c);
    static char positionToChar(uint8_t pos);

private:
    Adafruit_NeoPixel m_strip1;
    Adafruit_NeoPixel m_strip2;
    PositionData m_positions[NUM_POSITIONS];
    
    bool m_sequenceAnimActive;
    uint8_t m_sequenceAnimStep;
    uint32_t m_sequenceAnimLastTime;
    bool m_needsUpdate;
    
    // Menu change animation state
    bool m_menuChangeAc