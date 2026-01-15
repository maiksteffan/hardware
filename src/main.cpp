/**
 * =============================================================================
 * Dual LED Strip Controller for Arduino UNO R4 WiFi
 * =============================================================================
 * 
 * OVERVIEW
 * --------
 * This project controls two addressable LED strips via serial commands.
 * 25 logical positions (A-Y) are mapped to physical LEDs on either strip.
 * 
 * HARDWARE SETUP
 * --------------
 * - Board:       Arduino UNO R4 WiFi
 * - Framework:   Arduino (PlatformIO)
 * - Strip 1:     Data pin D5
 * - Strip 2:     Data pin D10
 * - LED Type:    WS2812B (addressable RGB LEDs)
 * 
 * LIBRARY CHOICE
 * --------------
 * This project uses Adafruit NeoPixel instead of FastLED because:
 * - Better compatibility with Arduino UNO R4 WiFi (Renesas RA platform)
 * - FastLED has known compilation issues with the Renesas RA MCU
 * - Adafruit NeoPixel is actively maintained for this platform
 * 
 * SERIAL PROTOCOL
 * ---------------
 * Baud Rate: 115200
 * Line ending: '\n' (newline)
 * 
 * Commands (case-insensitive):
 *   SHOW <pos>     - Light single LED at position in BLUE
 *   HIDE <pos>     - Turn off LED(s) at position (including expanded region)
 *   SUCCESS <pos>  - Play green expansion animation (up to 5 LEDs each side)
 * 
 * Position: A-Y (25 positions, case-insensitive)
 * 
 * Responses:
 *   ACK <ACTION> <POSITION>   - Command executed successfully
 *   ERR <reason>              - Command failed
 *     Reasons: unknown_action, unknown_position, bad_format, 
 *              line_too_long, command_failed
 * 
 * Examples:
 *   Input:  "SHOW A\n"       -> Output: "ACK SHOW A\n"
 *   Input:  "success b\n"    -> Output: "ACK SUCCESS B\n"
 *   Input:  "HIDE C\n"       -> Output: "ACK HIDE C\n"
 *   Input:  "INVALID X\n"    -> Output: "ERR unknown_action\n"
 *   Input:  "SHOW Z\n"       -> Output: "ERR unknown_position\n"
 * 
 * LED POSITION MAPPINGS
 * ---------------------
 * Positions A-Y are mapped to specific LEDs on Strip 1 or Strip 2.
 * Edit the LED_MAPPINGS array in LedController.cpp to match your physical layout.
 * 
 * Default mapping (modify as needed):
 *   A-L: Strip 1, indices 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55
 *   M-Y: Strip 2, indices 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 58
 * 
 * CONFIGURATION
 * -------------
 * - LED_BRIGHTNESS:          Overall brightness (0-255), default 128
 * - COLOR_SHOW_*:            Color for SHOW command (default: Blue)
 * - COLOR_SUCCESS_*:         Color for SUCCESS animation (default: Green)
 * - SUCCESS_EXPANSION_RADIUS: Max expansion (default: 5 LEDs each side)
 * - ANIMATION_STEP_MS:       Animation speed (default: 80ms per step)
 * 
 * See platformio.ini for strip length configuration (NUM_LEDS_STRIP1/2).
 * 
 * ARCHITECTURE
 * ------------
 * - LedController:    Manages LED buffers, rendering, animations
 * - CommandController: Parses serial input, dispatches commands, sends responses
 * - main.cpp:         Minimal setup/loop, wires components together
 * 
 * BUILD & UPLOAD
 * --------------
 *   pio run              # Build
 *   pio run -t upload    # Build and upload
 *   pio device monitor   # Open serial monitor
 * 
 * =============================================================================
 */

#include <Arduino.h>
#include "LedController.h"
#include "CommandController.h"
#include "TouchController.h"
#include "SequenceController.h"

// ============================================================================
// Global Instances
// ============================================================================

// LED controller manages both strips and animations
LedController ledController;

// Touch controller manages CAP1188 touch sensors
TouchController touchController;

// Sequence controller manages LED/touch sequences
SequenceController sequenceController(ledController, touchController);

// Command controller handles serial protocol (with touch and sequence controller references)
CommandController commandController(ledController, &touchController, &sequenceController);

// ============================================================================
// Touch Callback - bridges TouchController to SequenceController
// ============================================================================

void onTouchDetected(char letter) {
    sequenceController.onTouched(letter);
}

// ============================================================================
// Arduino Setup
// ============================================================================

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    
    // Wait for serial port to connect (needed for native USB)
    // Timeout after 3 seconds to not block if no USB connected
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime < 3000)) {
        // Wait
    }
    
    // Initialize LED controller (sets up FastLED)
    ledController.begin();
    
    // Initialize touch controller (sets up I2C and CAP1188 sensors)
    touchController.begin();
    
    // Register touch callback to notify sequence controller
    touchController.setTouchCallback(onTouchDetected);
    
    // Initialize sequence controller
    sequenceController.begin();
    
    // Initialize command controller
    commandController.begin();
    
    // Ready message
    Serial.println("LED & Touch Controller Ready");
    Serial.println("Commands: SHOW <A-Y>, HIDE <A-Y>, SUCCESS <A-Y>");
    Serial.println("          EXPECT <A-Y>, RECORD, SCAN, RECALIBRATE <A-Y>");
    Serial.println("          SEQUENCE(A,B,C,...)");
}

// ============================================================================
// Arduino Main Loop
// ============================================================================

void loop() {
    // Get current time once per loop iteration
    uint32_t now = millis();
    
    // Process serial commands (non-blocking)
    commandController.update();
    
    // Update touch sensor state (non-blocking)
    touchController.update();
    
    // Update sequence controller state (non-blocking)
    sequenceController.update();
    
    // Update LED animations (non-blocking)
    ledController.update(now);
}
