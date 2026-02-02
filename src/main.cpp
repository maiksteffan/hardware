/**
 * =============================================================================
 * LED & Touch Controller Firmware v2.0
 * Arduino UNO R4 WiFi - Event-Driven Architecture
 * =============================================================================
 * 
 * OVERVIEW
 * --------
 * This firmware implements a "dumb" hardware executor and event source.
 * The Arduino handles:
 *   - LED control (SHOW/HIDE/SUCCESS animations)
 *   - Touch sensor polling with debouncing
 *   - Serial command processing with request-response correlation
 * 
 * All game logic and sequence control resides on the Raspberry Pi.
 * 
 * PROTOCOL VERSION 2
 * ------------------
 * - ASCII line-based protocol, terminated by '\n'
 * - Optional command IDs (#<number>) for request-response correlation
 * - Non-blocking: TOUCH events can interleave with command responses
 * 
 * Commands (Pi -> Arduino):
 *   SHOW <pos> [#id]         Turn on LED at position (blue)
 *   HIDE <pos> [#id]         Turn off LED at position
 *   SUCCESS <pos> [#id]      Play success animation (green, non-blocking)
 *   EXPECT <pos> [#id]       Wait for touch, emit TOUCHED
 *   EXPECT_RELEASE <pos> [#id] Wait for release, emit TOUCH_RELEASED
 *   RECALIBRATE <pos> [#id]  Recalibrate touch sensor
 *   RECALIBRATE_ALL [#id]    Recalibrate all sensors
 *   SEQUENCE_COMPLETED [#id] Play celebration animation
 *   SCAN [#id]               Scan I2C bus for devices
 *   INFO [#id]               Return firmware info
 *   PING [#id]               Health check
 * 
 * Responses (Arduino -> Pi):
 *   ACK <action> [<pos>] [#id]   Command accepted
 *   DONE <action> [<pos>] [#id]  Long-running command completed
 *   ERR <reason> [#id]           Command failed
 *   TOUCH_DOWN <pos>             Touch sensor pressed (spontaneous)
 *   TOUCH_UP <pos>               Touch sensor released (spontaneous)
 *   TOUCHED <pos> [#id]          Expected touch detected
 *   TOUCH_RELEASED <pos> [#id]   Expected release detected
 *   SCANNED[A,B,C,...] [#id]     Active sensors list
 *   RECALIBRATED <pos|ALL> [#id] Recalibration complete
 *   INFO version=... [#id]       Firmware information
 * 
 * HARDWARE
 * --------
 *   Board:      Arduino UNO R4 WiFi
 *   LED Strip 1: D5 (190 LEDs)
 *   LED Strip 2: D10 (190 LEDs)
 *   Touch:      25x CAP1188 sensors via I2C
 *   Baud:       115200
 * 
 * MOCK PI TESTING
 * ---------------
 *   Define ENABLE_MOCK_PI to enable on-device testing without a real Pi.
 *   Select program with MOCK_PI_PROGRAM (1, 2, or 3).
 * 
 * =============================================================================
 */

#include "Config.h"
#include "LedController.h"
#include "TouchController.h"
#include "CommandController.h"
#include "EventQueue.h"

// ============================================================================
// Mock Pi Configuration (New Protocol-Based System)
// ============================================================================
// Uncomment to enable Mock Pi testing (simulates Pi commands on-device)
#define ENABLE_NEW_MOCK_PI 1

// ============================================================================
// Mode Selection: RECORD or PLAY
// ============================================================================
// MOCK_PI_MODE_RECORD: Touch sensors to record a sequence, then auto-play it
// MOCK_PI_MODE_PLAY: Play a predefined sequence string
#define MOCK_PI_MODE_RECORD 0
#define MOCK_PI_MODE_PLAY   1

// *** SELECT MODE HERE ***
#define MOCK_PI_MODE MOCK_PI_MODE_RECORD

// *** CHANGE THIS STRING TO RUN DIFFERENT SEQUENCES (for PLAY mode) ***
// Format: comma-separated positions, use + for simultaneous touches
// Examples:
//   "A,B,C,D,E"           - Simple sequential
//   "A,B,C,D,E+F,G+H,I,J,K" - Mixed single and pair touches
//   "A,G,N,P+T,F,G+H,O+L,R,Q" - Complex pattern
#define MOCK_PI_SEQUENCE "X,C,K,A+Q,E+Y,K,M"

// Whether to send PING/INFO before starting the sequence
#define MOCK_PI_DO_INIT true

// Enable verbose logging
#define MOCK_PI_VERBOSE true

#ifdef ENABLE_NEW_MOCK_PI
#include "mock_pi/MockPi.h"
#include "mock_pi/MockPiRecorder.h"
#endif

// ============================================================================
// Legacy Mock Pi Configuration (kept for reference, disabled)
// ============================================================================
// #define ENABLE_MOCK_PI 1
// #define MOCK_PI_PROGRAM 2
// #define MOCK_PI_SIMPLE_SEQUENCE "ABCDE"
// #define MOCK_PI_SIMULTANEOUS_SPEC "A,B,(C+D),(E+F)"
// #define MOCK_PI_TWO_HAND_SEQUENCE "ABCDEFG"
// #ifdef ENABLE_MOCK_PI
// #include "MockPiPrograms.h"
// #endif

// ============================================================================
// Global Instances
// ============================================================================

// Event queue for outgoing serial messages
EventQueue eventQueue;

// LED controller manages both strips and animations
LedController ledController;

// Touch controller manages CAP1188 touch sensors
TouchController touchController;

// Command controller handles serial protocol
CommandController commandController(ledController, &touchController, eventQueue);

#ifdef ENABLE_NEW_MOCK_PI
// New protocol-based Mock Pi subsystem
MockPI::MockPi newMockPi;
MockPI::MockPiRecorder mockPiRecorder;
bool recordingComplete = false;
#endif

// ============================================================================
// Arduino Setup
// ============================================================================

void setup() {
    // Initialize serial communication
    Serial.begin(SERIAL_BAUD_RATE);
    
    // Wait for serial port to connect (timeout after 3 seconds)
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime < 3000)) {
        // Wait
    }
    
    Serial.println("ARDUINO> Initializing...");
    
    // Initialize event queue
    eventQueue.begin();
    
    // Initialize LED controller
    ledController.begin();
    Serial.println("ARDUINO> LED controller ready");
    
    // Initialize touch controller (this now includes delays and recalibration)
    Serial.println("ARDUINO> Initializing touch sensors (please wait)...");
    touchController.setEventQueue(&eventQueue);
    touchController