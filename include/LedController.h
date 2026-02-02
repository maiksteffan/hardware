/**
 * @file LedController.h
 * @brief LED Controller for dual addressable LED strips with position-based control
 * 
 * Protocol v2: Non-blocking animations with completion tracking for DONE events.
 * Manages 25 logical LED positions (A-Y) mapped to two physical LED strips.
 * 
 * Operations:
 *   SHOW                - Light single LED at position (instant)
 *   HIDE                - Turn off LED at position (instant)
 *   SUCCESS             - Non-blocking expansion animation
 *   BLINK               - Start blinking LED at position
 *   STOP_BLINK          - Stop blinking LED at position
 *   MISTAKE             - Light LED red (wrong hold indicator)
 *   SEQUENCE_COMPLETED  - Celebration animation on all LEDs
 */

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

// ============================================================================
// Strip identifier
// ============================================================================

enum class StripId : uint8_t {
    STRIP1 = 0,
    STRIP2 = 1
};

// ============================================================================
// Mapping of a logical position to a physical LED
// ============================================================================

struct LedMapping {
    StripId strip;
    uint8_t index;
};

// ============================================================================
// Position state
// ============================================================================

enum class PositionState : uint8_t {
    OFF,           // LED is off
    SHOWN,         // Single LED lit (SHOW command)
    ANIMATING,     // SUCCESS animation in progress
    EXPANDED,      // SUCCESS animation complete, expanded region lit
    BLINKING,      // LED is blinking on/off
    MISTAKE        // LED lit red (wrong hold)
};

// ============================================================================
// Per-position tracking
// ============================================================================

struct PositionData {
    PositionState state;
    uint8_t animationStep;       // Current expansion step (0 = center only)
    uint32_t lastAnimationTime;  // For non-blocking animation timing
    bool blinkOn;                // Current blink state (on/off)
};

// ============================================================================
// LedController Class
// ============================================================================

class LedController {
public:
    /**
     * @brief Construct a new Led Controller object
     */
    LedController();

    /**
     * @brief Initialize the LED controller
     */
    void begin();

    /**
     * @brief Update LED animations (non-blocking)
     * @param nowMillis Current time from millis()
     */
    void update(uint32_t nowMillis);

    /**
     * @brief Tick LED controller (alias for update with current time)
     */
    void tick();

    /**
     * @brief Show a single LED at the specified position (BLUE)
     * @param position Position index (0 = A, 24 = Y)
     * @return true if successful
     */
    bool show(uint8_t position);

    /**
     * @brief Hide LEDs at the specified position
     * @param position Position index (0 = A, 24 = Y)
     * @return true if successful
     */
    bool hide(uint8_t position);

    /**
     * @brief Start blinking LED at the specified position
     * @param position Position index (0 = A, 24 = Y)
     * @return true if successful
     */
    bool blink(uint8_t position);

    /**
     * @brief Stop blinking LED at the specified position (turns off LED)
     * @param position Position index (0 = A, 24 = Y)
     * @return true if successful
     */
    bool stopBlink(uint8_t position);

    /**
     * @brief Show mistake indicator (RED) at the specified position
     * @param position Position index (0 = A, 24 = Y)
     * @return true if successful
     */
    bool mistake(uint8_t position);

    /**
     * @brief Check if position is currently blinking
     * @param position Position index (0 = A, 24 = Y)
     * @return true if blinking
     */
    bool isBlinking(uint8_t position) const;

    /**
     * @brief Start SUCCESS expansion animation at position (GREEN)
     * @param position Position index (0 = A, 24 = Y)
     * @return true if successful
     */
    bool success(uint8_t position);

    /**
     * @brief Check if animation is complete for a position
     * @param position Position index (0 = A, 24 = Y)
     * @return true if no animation running or animation is complete
     */
    bool isAnimationComplete(uint8_t position) const;

    /**
     * @brief Check if any animation is running
     * @return true if any position is animating
     */
    bool hasActiveAnimations() const;

    /**
     * @brief Start SEQUENCE_COMPLETED celebration animation on all LEDs
     */
    void startSequenceCompletedAnimation();

    /**
     * @brief Check if SEQUENCE_COMPLETED animation is complete
     * @return true if animation is complete or not running
     */
    bool isSequenceCompletedAnimationComplete() const;

    /**
     * @brief Start SEQUENCE_FAIL animation on all LEDs (red pulse)
     */
    void startSequenceFailAnimation();

    /**
     * @brief Check if SEQUENCE_FAIL animation is complete
     * @return true if animation is complete or not running
     */
    bool isSequenceFailAnimationComplete() const;

    /**
     * @brief Convert position character (A-Y) to index (0-24)
     * @param c Position character (case-insensitive)
     * @return Position index, or 255 if invalid
     */
    static uint8_t charToPosition(char c);

    /**
     * @brief Convert position index (0-24) to character (A-Y)
     * @param pos Position index
     * @return Position character, or '?' if invalid
     */
    static char positionToChar(uint8_t pos);

private:
    // NeoPixel strip objects
    Adafruit_NeoPixel m_strip1;
    Adafruit_NeoPixel m_strip2;

    // State tracking for each position
    PositionData m_positions[NUM_POSITIONS];

    // SEQUENCE_COMPLETED animation state
    bool m_sequenceAnimActive;       // Whether animation is running
    uint8_t m_sequenceAnimStep;      // Current animation step
    uint32_t m_sequenceAnimLastTime; // Last animation step time

    // SEQUENCE_FAIL animation state
    bool m_sequenceFailAnimActive;       // Whether fail animation is running
    uint8_t m_