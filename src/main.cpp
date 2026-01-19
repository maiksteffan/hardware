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
 *   EXPECT_DOWN <pos> [#id]  Wait for touch, emit TOUCHED_DOWN
 *   EXPECT_UP <pos> [#id]    Wait for release, emit TOUCHED_UP
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
 *   TOUCHED_DOWN <pos> [#id]     Expected touch detected
 *   TOUCHED_UP <pos> [#id]       Expected release detected
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

#include <Arduino.h>
#include "Config.h"
#include "LedController.h"
#include "TouchController.h"
#include "CommandController.h"
#include "EventQueue.h"

// ============================================================================
// Mock Pi Configuration
// ============================================================================
// Uncomment to enable Mock Pi testing (simulates Pi commands on-device)
#define ENABLE_MOCK_PI 1

// Select which program to run (1, 2, 3, or 4)
// 1 = Simple sequence (positions: ABCDE)
// 2 = Simultaneous sequence (spec: "A,B,(C+D),(E+F)")  
// 3 = Record then playback mode
// 4 = Two-hand overlapping sequence (positions: ABCDEFG)
#define MOCK_PI_PROGRAM 4

// Sequence for Program 1 (simple sequential)
#define MOCK_PI_SIMPLE_SEQUENCE "ABCDE"

// Specification for Program 2 (simultaneous steps)
#define MOCK_PI_SIMULTANEOUS_SPEC "A,B,(C+D),(E+F)"

// Sequence for Program 4 (two-hand overlapping)
#define MOCK_PI_TWO_HAND_SEQUENCE "ABCDEFG"

#ifdef ENABLE_MOCK_PI
#include "MockPiPrograms.h"
#endif

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

#ifdef ENABLE_MOCK_PI
// Mock Pi for on-device testing
MockPiPrograms mockPi;
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
    
    // Initialize event queue
    eventQueue.begin();
    
    // Initialize LED controller
    ledController.begin();
    
    // Initialize touch controller
    touchController.setEventQueue(&eventQueue);
    touchController.begin();
    
    // Initialize command controller
    commandController.begin();
    
    // Signal ready - send INFO automatically
    eventQueue.queueInfo(NO_COMMAND_ID);
    eventQueue.flush(1);
    
#ifdef ENABLE_MOCK_PI
    // Initialize Mock Pi
    mockPi.begin();
    mockPi.setTouchController(&touchController);
    mockPi.setCommandController(&commandController);
    mockPi.setVerbose(true);
    
    // Small delay to let serial settle
    delay(500);
    
    // Start the selected program
    #if MOCK_PI_PROGRAM == 1
        Serial.println("MockPi: Starting Program 1 - Simple Sequence");
        mockPi.startSequenceSimple(MOCK_PI_SIMPLE_SEQUENCE);
    #elif MOCK_PI_PROGRAM == 2
        Serial.println("MockPi: Starting Program 2 - Simultaneous Sequence");
        mockPi.startSequenceSimultaneous(MOCK_PI_SIMULTANEOUS_SPEC);
    #elif MOCK_PI_PROGRAM == 3
        Serial.println("MockPi: Starting Program 3 - Record & Playback");
        mockPi.startRecordPlayback();
    #elif MOCK_PI_PROGRAM == 4
        Serial.println("MockPi: Starting Program 4 - Two-Hand Sequence");
        mockPi.startTwoHandSequence(MOCK_PI_TWO_HAND_SEQUENCE);
    #else
        Serial.println("MockPi: No program selected (set MOCK_PI_PROGRAM to 1, 2, 3, or 4)");
    #endif
#endif
}

// ============================================================================
// Arduino Main Loop
// ============================================================================

void loop() {
    // 1. Poll serial for incoming data (non-blocking)
    commandController.pollSerial();
    
    // 2. Process any complete command lines
    commandController.processCompletedLines();
    
    // 3. Tick command executor for long-running commands
    commandController.tick();
    
    // 4. Tick touch controller (poll sensors, debounce, emit events)
    touchController.tick();
    
    // 5. Tick LED controller (update animations)
    ledController.tick();
    
    // 6. Flush pending events to serial
    eventQueue.flush(3);  // Send up to 3 events per loop
    
#ifdef ENABLE_MOCK_PI
    // 7. Update Mock Pi state machine
    mockPi.update();
#endif
}
