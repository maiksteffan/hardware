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
    BLINKING       // LED is blinking on/off
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

    // Flag to indicate strip.show() is needed
    bool m_needsUpdate;

    /**
     * @brief Get the LED mapping for a position
     * @param position Position index (0-24)
     * @return Pointer to mapping, or nullptr if invalid
     */
    const LedMapping* getMapping(uint8_t position) const;

    /**
     * @brief Get the LED count for a strip
     * @param strip Strip identifier
     * @return Number of LEDs
     */
    uint16_t getStripLength(StripId strip) const;

    /**
     * @brief Get pointer to NeoPixel strip
     * @param strip Strip identifier
     * @return Pointer to strip
     */
    Adafruit_NeoPixel* getStrip(StripId strip);

    /**
     * @brief Set a single LED color
     * @param strip Strip identifier
     * @param index LED index
     * @param r Red component
     * @param g Green component
     * @param b Blue component
     */
    void setLed(StripId strip, int16_t index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Clear the expanded region for a position
     * @param position Position index
     * @param mapping LED mapping
     */
    void clearExpandedRegion(uint8_t position, const LedMapping* mapping);

    /**
     * @brief Render the current state of a position
     * @param position Position index
     */
    void renderPosition(uint8_t position);

    /**
     * @brief Update animation for a single position
     * @param position Position index
     * @param nowMillis Current time
     */
    void updateAnimation(uint8_t position, uint32_t nowMillis);

    /**
     * @brief Update SEQUENCE_COMPLETED animation
     * @param nowMillis Current time
     */
    void updateSequenceCompletedAnimation(uint32_t nowMillis);

    /**
     * @brief Update blink state for blinking positions
     * @param nowMillis Current time
     */
    void updateBlinking(uint32_t nowMillis);
};

#endif // LED_CONTROLLER_H
