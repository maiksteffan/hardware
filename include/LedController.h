/**
 * @file LedController.h
 * @brief LED Controller for dual addressable LED strips with position-based control
 * 
 * Manages 25 logical LED positions (A-Y) mapped to two physical LED strips.
 * Supports SHOW (single LED), SUCCESS (expansion animation), and HIDE operations.
 * 
 * Uses Adafruit NeoPixel library which has better support for Arduino UNO R4 WiFi
 * (Renesas RA platform) than FastLED.
 */

#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// ============================================================================
// Hardware Configuration
// ============================================================================

// LED strip data pins
constexpr uint8_t STRIP1_PIN = 5;   // D5
constexpr uint8_t STRIP2_PIN = 10;  // D10

// LED counts per strip (can be overridden via build flags)
#ifndef NUM_LEDS_STRIP1
#define NUM_LEDS_STRIP1 150
#endif

#ifndef NUM_LEDS_STRIP2
#define NUM_LEDS_STRIP2 150
#endif

// ============================================================================
// Visual Configuration
// ============================================================================

// Overall brightness (0-255)
constexpr uint8_t LED_BRIGHTNESS = 128;

// Colors for different actions (RGB format for NeoPixel)
// SHOW = Blue
constexpr uint8_t COLOR_SHOW_R = 0;
constexpr uint8_t COLOR_SHOW_G = 0;
constexpr uint8_t COLOR_SHOW_B = 255;

// SUCCESS = Green
constexpr uint8_t COLOR_SUCCESS_R = 0;
constexpr uint8_t COLOR_SUCCESS_G = 255;
constexpr uint8_t COLOR_SUCCESS_B = 0;

// OFF = Black
constexpr uint8_t COLOR_OFF_R = 0;
constexpr uint8_t COLOR_OFF_G = 0;
constexpr uint8_t COLOR_OFF_B = 0;

// Animation settings
constexpr uint8_t SUCCESS_EXPANSION_RADIUS = 5;    // Max LEDs on each side
constexpr uint16_t ANIMATION_STEP_MS = 80;         // Time between expansion steps

// ============================================================================
// Position Definitions
// ============================================================================

// Number of logical LED positions
constexpr uint8_t NUM_POSITIONS = 25;  // A-Y

// Strip identifier
enum class StripId : uint8_t {
    STRIP1 = 0,
    STRIP2 = 1
};

// Mapping of a logical position to a physical LED
struct LedMapping {
    StripId strip;
    uint8_t index;
};

// Position state
enum class PositionState : uint8_t {
    OFF,           // LED is off
    SHOWN,         // Single LED lit (SHOW command)
    ANIMATING,     // SUCCESS animation in progress
    EXPANDED       // SUCCESS animation complete, expanded region lit
};

// Per-position tracking
struct PositionData {
    PositionState state;
    uint8_t animationStep;       // Current expansion step (0 = center only)
    uint32_t lastAnimationTime;  // For non-blocking animation timing
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
     * Call this in setup() before using other methods.
     */
    void begin();

    /**
     * @brief Update LED animations (call every loop iteration)
     * @param nowMillis Current time from millis()
     */
    void update(uint32_t nowMillis);

    /**
     * @brief Show a single LED at the specified position (BLUE)
     * @param position Position index (0 = A, 24 = Y)
     * @return true if successful, false if invalid position
     */
    bool show(uint8_t position);

    /**
     * @brief Hide LEDs at the specified position (clears expanded region too)
     * @param position Position index (0 = A, 24 = Y)
     * @return true if successful, false if invalid position
     */
    bool hide(uint8_t position);

    /**
     * @brief Start SUCCESS expansion animation at the specified position (GREEN)
     * @param position Position index (0 = A, 24 = Y)
     * @return true if successful, false if invalid position
     */
    bool success(uint8_t position);

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
     * @return Number of LEDs on that strip
     */
    uint16_t getStripLength(StripId strip) const;

    /**
     * @brief Get pointer to the appropriate NeoPixel strip
     * @param strip Strip identifier
     * @return Pointer to NeoPixel object
     */
    Adafruit_NeoPixel* getStrip(StripId strip);

    /**
     * @brief Set a single LED color
     * @param strip Strip identifier
     * @param index LED index on that strip
     * @param r Red component (0-255)
     * @param g Green component (0-255)
     * @param b Blue component (0-255)
     */
    void setLed(StripId strip, int16_t index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Clear the expanded region for a position
     * @param position Position index
     * @param mapping LED mapping for the position
     */
    void clearExpandedRegion(uint8_t position, const LedMapping* mapping);

    /**
     * @brief Render the current state of a position to the LED buffer
     * @param position Position index
     */
    void renderPosition(uint8_t position);

    /**
     * @brief Update animation for a single position
     * @param position Position index
     * @param nowMillis Current time
     */
    void updateAnimation(uint8_t position, uint32_t nowMillis);
};

#endif // LED_CONTROLLER_H
